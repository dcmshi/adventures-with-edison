#include "shell/launcher.h"

#include <algorithm>
#include <cstdio>

#include "formats/ne_file.h"

namespace edison {
namespace {

// Periodic timer slots (the library allows 10).
enum Slot { kGameTick, kCountdowns, kIntro };

constexpr uint32_t kTimerMs = 13;
constexpr uint32_t kThreshold = 1000 / kTimerMs;  // 76

// Script, anim and sound ids used by the menu (SHELL.D01).
constexpr uint16_t kMenuScreen = 0x1010, kStarfield = 0x1011;
constexpr uint16_t kIntroFrames = 0x2300;  // + frame index
constexpr uint16_t kMenuMusic = 0xDE;      // CADLIB sound

}  // namespace

bool Launcher::load(const Options& options, std::string* error) {
    options_ = options;
    if (!ctx_.archive.open(options.cdDir + "/SHELL.D01", error)) return false;

    // The opening's flight path is a table in EDISON.EXE's data segment
    // (the last segment), read here rather than copied into the source.
    NeFile exe;
    if (!exe.load(options.cdDir + "/EDISON.EXE", error)) return false;
    const auto data = exe.segment(exe.segmentCount());
    constexpr size_t kPath = 0x150;
    for (size_t i = 0; i < introPath_.size() && kPath + 2 * i + 1 < data.size(); ++i)
        introPath_[i] = static_cast<int16_t>(data[kPath + 2 * i] | data[kPath + 2 * i + 1] << 8);
    for (introEntries_ = 0; introEntries_ < 112 && introPath_[4 * introEntries_] >= 0; ++introEntries_) {}
    if (introPath_[0] != 538 || introEntries_ == 0 || introEntries_ == 112) {
        if (error) *error = "EDISON.EXE: unexpected data segment (opening path not found)";
        return false;
    }
    return true;
}

// --- timer and main loop ------------------------------------------------------

void Launcher::setPeriodic(int slot, uint32_t rate, std::function<void()> fn) {
    periodic_[slot].rate = rate;
    periodic_[slot].acc = 0;
    periodic_[slot].fn = std::move(fn);
}

void Launcher::advanceTimer() {
    const uint64_t target = ctx_.platform.milliseconds() / kTimerMs;
    // After a long stall (window dragged, debugger), don't replay minutes.
    if (target > timerTicks_ + 100) timerTicks_ = target - 100;
    while (timerTicks_ < target) {
        ++timerTicks_;
        for (auto& p : periodic_) {
            if (!p.rate) continue;
            p.acc += p.rate;
            if (p.acc >= kThreshold) {
                p.acc -= kThreshold;
                p.fn();
            }
        }
    }
}

void Launcher::pump() {
    if (!ctx_.platform.pumpEvents()) throw Closed{};
    advanceTimer();
    ctx_.platform.present(ctx_.screens[1], displayPalette_);
}

bool Launcher::runScripts(ScriptEvent& event) {
    event = ScriptEvent{};
    return scripts_.run(event);
}

// --- drawing helpers -------------------------------------------------------------

void Launcher::blackout() {
    ctx_.screens[2].clear();
    ctx_.screens.copyAll(2, 1);
    displayPalette_ = {};
}

void Launcher::showFullScreen(uint16_t bitmap, int screen) {
    const Bitmap& bmp = ctx_.bitmap(bitmap);
    Screen& s = ctx_.screens[screen];
    s.clear();
    for (int y = 0; y < std::min(bmp.height, Screen::kHeight); ++y)
        for (int x = 0; x < std::min(bmp.width, Screen::kWidth); ++x)
            s.pixels[static_cast<size_t>(y) * Screen::kWidth + x] = bmp.at(x, y);
    s.palette = bmp.palette;
}

void Launcher::setDisplayPalette(int screen) {
    displayPalette_ = ctx_.screens[screen].palette;
    displayPalette_[0] = Rgb{0, 0, 0};  // the system colours stay black and white
    displayPalette_[255] = Rgb{255, 255, 255};
}

void Launcher::drawLogo(int screen, int x, int y, uint16_t id) {
    ctx_.screens.drawSprite(screen, ctx_.bitmap(id), x, y);
}

void Launcher::fm(uint16_t sound) {
    if (options_.music) ctx_.platform.sendFm(sound);
}

int Launcher::pollButton() {
    int x, y;
    if (!ctx_.platform.takeClick(&x, &y)) return 0;
    if (x >= 0x228 && x <= 0x268 && y >= 0x15B && y <= 0x186) return 4;  // EXIT
    if (y > 0x40 && y < 0x8D) {
        if (x > 0x27 && x < 0xAB) return kRockAndBach;
        if (x > 0xEF && x < 0x173) return kWildScience;
        if (x > 0x1D5 && x < 0x259) return kMystery;
    }
    return 0;
}

// --- the opening (f05_0034) -------------------------------------------------------

void Launcher::opening() {
    blackout();
    showFullScreen(kStarfield, 3);
    ctx_.screens[2].palette = ctx_.screens[3].palette;
    setDisplayPalette(3);
    ctx_.screens.copyAll(3, 2);
    ctx_.screens.copyAll(3, 1);
    // (The original also copies a 16x16 box at the mouse position here; with
    // the cursor outside the window that fails with "Invalid xPos". Skipped.)

    setPeriodic(kIntro, 16, [this] { introFrameDue_ = true; });
    ctx_.playWav(0x401D);

    int prev = -1;
    int frame = 0;
    bool ending = false;
    while (!ctx_.platform.escapeHeld()) {
        pump();
        if (ending) {
            if (countdown_[0] == 0) break;
            continue;
        }
        if (!introFrameDue_) continue;
        introFrameDue_ = false;

        const int16_t* cur = &introPath_[4 * frame];
        const Bitmap& sprite = ctx_.bitmap(static_cast<uint16_t>(kIntroFrames + frame));
        if (prev == -1 || prev >= 90) {
            ctx_.screens.copyArea(3, 2, cur[0], cur[1], cur[2], cur[3]);
            ctx_.screens.drawSprite(2, sprite, cur[0], cur[1]);
            ctx_.screens.copyArea(2, 1, cur[0], cur[1], cur[2], cur[3]);
        } else {
            // Erase the previous frame, draw this one, show both areas.
            const int16_t* p = &introPath_[4 * prev];
            ctx_.screens.copyArea(3, 2, p[0], p[1], p[2], p[3]);
            ctx_.screens.drawSprite(2, sprite, cur[0], cur[1]);
            const int left = std::min(cur[0], p[0]), top = std::min(cur[1], p[1]);
            const int right = std::max(cur[0] + cur[2], p[0] + p[2]);
            const int bottom = std::max(cur[1] + cur[3], p[1] + p[3]);
            ctx_.screens.copyArea(2, 1, left, top, right - left, bottom - top);
        }
        prev = frame++;
        if (frame >= introEntries_) {
            ending = true;
            countdown_[0] = 0x16;  // 2.2 s
        }
    }
    setPeriodic(kIntro, 0, nullptr);
}

// --- the menu (f04_0172) -------------------------------------------------------------

Launcher::Choice Launcher::menu() {
    ctx_.gameTicks = 0;
    setPeriodic(kGameTick, 16, [this] { ++ctx_.gameTicks; });
    fm(kMenuMusic);
    blackout();
    showFullScreen(kMenuScreen, 3);
    ctx_.screens[2].palette = ctx_.screens[3].palette;
    setDisplayPalette(3);
    ctx_.screens.copyAll(3, 2);
    ctx_.screens.copyAll(3, 1);

    ScriptEvent ev;
    // Waits until `done` returns true; the EXIT button leaves the menu.
    auto waitFor = [&](const std::function<bool()>& done) {
        for (;;) {
            pump();
            if (pollButton() == 4) return false;
            runScripts(ev);
            if (done()) return true;
        }
    };
    auto finished = [&](uint16_t script) {
        return ev.type == ScriptEvent::kFinished && (script == 0 || ev.archiveId == script);
    };
    auto quit = [&] {
        setPeriodic(kGameTick, 0, nullptr);
        scripts_.killAll();
        fm(0);
        return kQuit;
    };

    if (!options_.skipOpening) {
        // Edison walks in; the sign flashes until he reaches the button.
        scripts_.start(0x304, 0);
        scripts_.start(0x30E, 0);
        ctx_.playWav(0x4017);
        ctx_.resetFinished();
        bool chime = false;
        const bool ok = waitFor([&] {
            if (ctx_.finished(0x5013)) {
                ctx_.clearFinished(0x5013);
                chime = true;
                countdown_[0] = 0x10;
            }
            if (chime && countdown_[0] == 0) {
                ctx_.playWav(0x401C);
                chime = false;
            }
            return finished(0x304);
        });
        if (!ok) return quit();

        // The three game buttons appear one after another.
        scripts_.start(0x30F, 0);
        scripts_.start(0x302, 0);
        ctx_.playWav(0x4014);
        if (!waitFor([&] { return finished(0); })) return quit();
        ctx_.playWav(0x4015);
        drawLogo(3, 0xE2, 0x17, 0x212F);
        scripts_.start(0x30B, 0);
        if (!waitFor([&] { return finished(0); })) return quit();
        ctx_.playWav(0x4016);
        drawLogo(3, 10, 0x18, 0x2001);
        scripts_.start(0x308, 0);
        if (!waitFor([&] { return finished(0); })) return quit();
        drawLogo(3, 0x1B2, 0x25, 0x20E3);
        drawLogo(1, 0x1B2, 0x25, 0x20E3);
        scripts_.start(0x301, 0);
        if (!waitFor([&] { return finished(0x301); })) return quit();
        scripts_.start(0x303, 0);
    } else {
        for (int s : {3, 1}) {
            drawLogo(s, 0x1B2, 0x25, 0x20E3);
            drawLogo(s, 0xE2, 0x17, 0x212F);
            drawLogo(s, 10, 0x18, 0x2001);
        }
        scripts_.start(0x303, 0);
    }

    // Wait for a choice.
    int choice = 0;
    while (choice == 0) {
        pump();
        runScripts(ev);
        choice = pollButton();
    }
    // The pressed button, its sound and its anim.
    struct Pressed { int x, y; uint16_t logo, wav, script; };
    static const Pressed kPressed[] = {
        {10, 0x18, 0x2002, 0x4011, 0x306},     // Rock and Bach
        {0xE2, 0x17, 0x2131, 0x4012, 0x305},   // Wild Science Arcade
        {0x1B2, 0x25, 0x20E5, 0x4013, 0x307},  // Mystery at the Museums
    };
    if (choice != 4) {
        const Pressed& p = kPressed[choice - 1];
        drawLogo(1, p.x, p.y, p.logo);
        drawLogo(3, p.x, p.y, p.logo);
        ctx_.playWav(p.wav);
        scripts_.start(p.script, 0);
    }
    countdown_[0] = choice == 4 ? 5 : 0x14;
    while (!ctx_.platform.escapeHeld() && countdown_[0] != 0) {
        pump();
        runScripts(ev);
    }
    setPeriodic(kGameTick, 0, nullptr);
    scripts_.killAll();
    anims_.clear();
    fm(0);
    return choice == 4 ? kQuit : static_cast<Choice>(choice);
}

Launcher::Choice Launcher::run() {
    setPeriodic(kCountdowns, 10, [this] {
        for (int& c : countdown_)
            if (c > 0) --c;
    });
    timerTicks_ = ctx_.platform.milliseconds() / kTimerMs;
    if (!options_.skipOpening) opening();
    return menu();
}

}  // namespace edison
