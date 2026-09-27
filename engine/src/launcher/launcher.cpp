#include "launcher/launcher.h"

#include <algorithm>
#include <cstdio>

#include "formats/ne_file.h"

namespace edison {
namespace {

// Periodic timer slots (the library allows 10).
enum Slot { kGameTick, kCountdowns, kIntro };

// Script, anim and sound ids used by the menu (SHELL.D01).
constexpr uint16_t kMenuScreen = 0x1010, kStarfield = 0x1011;
constexpr uint16_t kIntroFrames = 0x2300;  // + frame index
constexpr uint16_t kMenuMusic = 0xDE;      // CADLIB sound

}  // namespace

bool Launcher::load(const Options& options, std::string* error) {
    options_ = options;
    if (!ctx_.archive.open(options.cdDir + "/SHELL.D01", error)) return false;
    if (options.music) ctx_.platform.setFmDriver(options.cdDir + "/CADLIB.DLL");

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

bool Launcher::runScripts(ScriptEvent& event) {
    event = ScriptEvent{};
    return scripts_.run(event);
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
    ctx_.blackout();
    ctx_.showFullScreen(kStarfield, 3);
    ctx_.screens[2].palette = ctx_.screens[3].palette;
    ctx_.setDisplayPalette(3);
    ctx_.screens.copyAll(3, 2);
    ctx_.screens.copyAll(3, 1);
    // (The original also copies a 16x16 box at the mouse position here; with
    // the cursor outside the window that fails with "Invalid xPos". Skipped.)

    ctx_.timer.setPeriodic(kIntro, 16, [this] { introFrameDue_ = true; });
    ctx_.playWav(0x401D);

    int prev = -1;
    int frame = 0;
    bool ending = false;
    while (!ctx_.platform.escapeHeld()) {
        ctx_.pump();
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
    ctx_.timer.setPeriodic(kIntro, 0, nullptr);
}

// --- the menu (f04_0172) -------------------------------------------------------------

Launcher::Choice Launcher::menu() {
    ctx_.gameTicks = 0;
    ctx_.timer.setPeriodic(kGameTick, 16, [this] { ++ctx_.gameTicks; });
    fm(kMenuMusic);
    ctx_.blackout();
    ctx_.showFullScreen(kMenuScreen, 3);
    ctx_.screens[2].palette = ctx_.screens[3].palette;
    ctx_.setDisplayPalette(3);
    ctx_.screens.copyAll(3, 2);
    ctx_.screens.copyAll(3, 1);

    ScriptEvent ev;
    // Waits until `done` returns true; the EXIT button leaves the menu.
    auto waitFor = [&](const std::function<bool()>& done) {
        for (;;) {
            ctx_.pump();
            if (pollButton() == 4) return false;
            runScripts(ev);
            if (done()) return true;
        }
    };
    auto finished = [&](uint16_t script) {
        return ev.type == ScriptEvent::kFinished && (script == 0 || ev.archiveId == script);
    };
    auto quit = [&] {
        ctx_.timer.setPeriodic(kGameTick, 0, nullptr);
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
        ctx_.drawLogo(3, 0xE2, 0x17, 0x212F);
        scripts_.start(0x30B, 0);
        if (!waitFor([&] { return finished(0); })) return quit();
        ctx_.playWav(0x4016);
        ctx_.drawLogo(3, 10, 0x18, 0x2001);
        scripts_.start(0x308, 0);
        if (!waitFor([&] { return finished(0); })) return quit();
        ctx_.drawLogo(3, 0x1B2, 0x25, 0x20E3);
        ctx_.drawLogo(1, 0x1B2, 0x25, 0x20E3);
        scripts_.start(0x301, 0);
        if (!waitFor([&] { return finished(0x301); })) return quit();
        scripts_.start(0x303, 0);
    } else {
        for (int s : {3, 1}) {
            ctx_.drawLogo(s, 0x1B2, 0x25, 0x20E3);
            ctx_.drawLogo(s, 0xE2, 0x17, 0x212F);
            ctx_.drawLogo(s, 10, 0x18, 0x2001);
        }
        scripts_.start(0x303, 0);
    }

    // Wait for a choice.
    int choice = 0;
    while (choice == 0) {
        ctx_.pump();
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
        ctx_.drawLogo(1, p.x, p.y, p.logo);
        ctx_.drawLogo(3, p.x, p.y, p.logo);
        ctx_.playWav(p.wav);
        scripts_.start(p.script, 0);
    }
    countdown_[0] = choice == 4 ? 5 : 0x14;
    while (!ctx_.platform.escapeHeld() && countdown_[0] != 0) {
        ctx_.pump();
        runScripts(ev);
    }
    ctx_.timer.setPeriodic(kGameTick, 0, nullptr);
    scripts_.killAll();
    anims_.clear();
    fm(0);
    return choice == 4 ? kQuit : static_cast<Choice>(choice);
}

Launcher::Choice Launcher::run() {
    ctx_.timer.setPeriodic(kCountdowns, 10, [this] {
        for (int& c : countdown_)
            if (c > 0) --c;
    });
    ctx_.timer.reset(ctx_.platform.milliseconds());
    if (!options_.skipOpening) opening();
    return menu();
}

}  // namespace edison
