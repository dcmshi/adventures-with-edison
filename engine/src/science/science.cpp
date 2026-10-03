// WMAIN.EXE: start-up (segments 32 and 62) and the story (segment 38).

#include "science/science.h"

#include <algorithm>
#include <fstream>
#include <iterator>

#include "formats/ne_file.h"

namespace edison {

bool Science::load(const Options& options, std::string* error) {
    options_ = options;
    // The CD root holds the SCIENCE folder of sounds, next to DSK3.
    cdRoot_ = options.cdDir + "/..";
    if (!ctx_.archive.open(options.cdDir + "/GRAFX.DAT", error)) return false;
    NeFile exe;
    if (!exe.load(options.cdDir + "/WMAIN.EXE", error)) return false;
    if (exe.segmentCount() < 103) {
        if (error) *error = "WMAIN.EXE: unexpected segments";
        return false;
    }
    data_ = exe.segment(103);
    strings_ = exe.segment(97);
    looks_ = exe.segment(95);
    sines_ = exe.segment(86);
    atans_ = exe.segment(87);
    std::copy_n(data_.begin() + 0x1C56, 4, look_);
    if (dataString(0x2766) != "Player") {
        if (error) *error = "WMAIN.EXE: unexpected data segment";
        return false;
    }
    font_ = &ctx_.font(0x0103);  // f32_0319: resource 103, f32_073e
    return true;
}

std::string Science::dataString(uint16_t offset) const {
    std::string s;
    for (size_t i = offset; i < data_.size() && data_[i]; ++i) s += static_cast<char>(data_[i]);
    return s;
}

void Science::run() {
    // f62_0020 builds the game object (f31_0025 over f32_0319); with
    // [26CE] set the title and the story run first.
    ctx_.startTimer();
    if (options_.music) ctx_.platform.setFmDriver(options_.cdDir + "/SADLIB.DLL");
    select(1);
    const int start = options_.startRoom;
    if (start >= 1 && start <= 110) {
        // (Testing: a room's table, as far as it's ported.)
        showTable(start);
        if (options_.music) ctx_.platform.setFmDriver(std::string());
        return;
    }
    if (start < 0) {
        title();
        story();
    }
    if (start < 0 || start == 501) lab();  // room 501
    if (start >= 505 && start <= 510) {
        // (Testing: the look and name as the lab would leave them.)
        loadLook();
        looksConverted_ = true;
        playerName_ = dataString(0x1D6F);  // "Player"
    }
    // Room 501 goes on to the first lesson (f31_0783); the others come
    // from the arcade's holes.
    int room = -1;
    if (start < 0 || start == 501) room = lesson(5);  // room 505, then room 1
    else if (start >= 505 && start <= 510) room = lesson(start - 500);
    if (room > 0) arcade(room);
    if (options_.music) ctx_.platform.setFmDriver(std::string());
}

void Science::arcade(int room) {
    // f31_0783, event 9: rooms 1-100 are tables (the old one's objects
    // go; its columns' counts stay the player's); 501 the lab's name
    // (f38_06dd), then room 1 again when it came from there (else the
    // first lesson); 502 the high scores (segment 40), 503 the credits
    // (f38_0eb9), neither ported: back to the room it came from (the
    // original goes back to room 1 fresh from there, or to the old room
    // with the ball spat out of its hole, mode 2); 504 leaves (event 3);
    // 505-510 the professor's lessons (f38_0fb5), each leading on.
    int from = 0;  // the player's +90: the last table
    while (room > 0) {
        if (room <= 110) {
            from = room;
            room = playRoom(room);
            continue;
        }
        switch (room) {
        case 501:
            lab();
            room = from == 1 ? 1 : 505;
            break;
        case 502:
        case 503:
            logLine(std::string("Wild Science Arcade: the ") + (room == 502 ? "high scores aren't" : "credits aren't") + " ported yet");
            room = from > 0 ? from : 1;
            break;
        case 504:
            return;
        default:
            if (room >= 505 && room <= 510) {
                room = lesson(room - 500);
                break;
            }
            logLine("Wild Science Arcade: room " + std::to_string(room) + " isn't ported");
            return;
        }
    }
}

// --- the framework's helpers ------------------------------------------------

void Science::showScreen(uint16_t picture, int screen) {
    // f63_09d7: the picture and its palette on the screen (colour 0
    // black, 255 white, as the library keeps them).
    ctx_.showFullScreen(picture, screen);
    Palette& pal = ctx_.screens[screen].palette;
    pal[0] = Rgb{0, 0, 0};
    pal[255] = Rgb{255, 255, 255};
    if (screen == 1) ctx_.setDisplayPalette(1);
}

void Science::showScreenWithLook(uint16_t picture, int screen) {
    // f14_092c: the tables are copied into the palette as they are, 6-bit
    // B, G, R bytes in the palette's 8-bit B, G, R (so the look comes out
    // dark: as in the original).
    showScreen(picture, screen);
    static const struct { uint16_t table; int count, first; } kParts[4] = {
        {0x004F, 3, 0xE1}, {0x0097, 3, 0xE4}, {0x00DF, 4, 0xE7}, {0x013F, 3, 0xEB}};
    Palette& pal = ctx_.screens[screen].palette;
    for (int t = 0; t < 4; ++t)
        for (int k = 0; k < kParts[t].count; ++k) {
            const size_t at = kParts[t].table + 3u * (static_cast<size_t>(look_[t]) * kParts[t].count + k);
            if (at + 2 >= looks_.size()) continue;
            // Once the lab has turned the tables into 8-bit colours
            // (f19_0614), they're right.
            pal[kParts[t].first + k] = looksConverted_ ? lookColour(t, look_[t], k) : Rgb{looks_[at + 2], looks_[at + 1], looks_[at]};
        }
    if (screen == 1) ctx_.setDisplayPalette(1);
}

void Science::toDisplay(int screen) {
    // f20_00f3: the screen's palette to the display, with its picture.
    ctx_.screens.copyAll(screen, 1);
    ctx_.screens[1].palette = ctx_.screens[screen].palette;
    ctx_.setDisplayPalette(screen);
}

void Science::clearDisplay() {
    // f20_0094: screen 3 filled with colour 2 and copied to the display.
    const int previous = current();
    select(3);
    fill(0, 0, Screen::kWidth, Screen::kHeight, 2);
    copyArea(3, 1, 0, 0, Screen::kWidth, Screen::kHeight);
    select(previous);
}

void Science::fmSound(uint16_t sound) {
    // f32_135d.
    if (options_.music) ctx_.platform.sendFm(sound);
}

void Science::narration(int n, bool story) {
    // f36_00ad: the name, 9 bytes apart, then the CD's \science\ or the
    // game's data\ folder.
    // (The second list is reached as the original does, wrapping at 64 KB:
    // (n * 9 + 9700) & FFFF, so the lessons' 6169 is WSA1521.)
    const size_t at = story ? 0x519u + 9u * static_cast<size_t>(n) : (static_cast<unsigned>(n) * 9u + 0x9700u) & 0xFFFFu;
    std::string name;
    for (size_t i = at; i < strings_.size() && strings_[i] && name.size() < 9; ++i) name += static_cast<char>(strings_[i]);
    std::ifstream in(cdRoot_ + "/SCIENCE/" + name + ".WAV", std::ios::binary);
    if (!in) in.open(options_.saveDir + "/data/" + name + ".wav", std::ios::binary);
    if (!in) {
        warnOnce("Wild Science Arcade: no sound " + name);
        return;
    }
    const std::vector<uint8_t> wav((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (wav.size() > 120000) return;  // f75_02e8
    ctx_.platform.playWav(wav);
}

void Science::waitNarration() {
    while (ctx_.platform.wavPlaying()) ctx_.pump();
}

bool Science::waitTicks(int ticks, bool interruptible) {
    // [9558] (a key) and [6EC5] (a button) end it.
    const uint64_t end = ctx_.platform.milliseconds() + static_cast<uint64_t>(ticks) * 20;
    int x, y;
    while (ctx_.platform.milliseconds() < end) {
        ctx_.pump();
        if (interruptible && (ctx_.platform.takeClick(&x, &y) || ctx_.platform.takeKey() != 0)) return true;
    }
    return false;
}

bool Science::escapePressed() {
    return ctx_.platform.escapeHeld();
}

// --- the opening ---------------------------------------------------------------

void Science::title() {
    // f32_0319: FM sound D, picture 2000 on the display for 130 countdown
    // ticks (10 a second) or until a key or click, then the display
    // cleared.
    fmSound(0x0D);
    showScreen(0x2000, 1);
    ctx_.countdown[0] = 0x82;  // DS:95F2
    int x, y;
    while (ctx_.countdown[0] > 0) {
        ctx_.pump();
        if (ctx_.platform.takeClick(&x, &y) || ctx_.platform.takeKey() != 0) break;
    }
    clearDisplay();
    // Picture 2002 onto screen 2 (the lab's first backdrop).
    showScreen(0x2002, 2);
}

void Science::story() {
    // f38_0718: three pictures with captions (colour 10, the picture's own
    // green) and the narration; Escape after a sound skips the rest.
    struct Page {
        uint16_t picture;
        std::vector<uint16_t> lines;
        std::vector<int> sounds;
        int waits, wait;  // waits x f32_07aa(wait)
    };
    static const Page kPages[3] = {
        {0x2007, {0x29F6, 0x2A2B, 0x2A5F}, {0, 1}, 60, 10},
        {0x2008, {0x2A73, 0x2AA9, 0x2AE0}, {2, 3, 4}, 60, 10},
        {0x2008, {0x2B05, 0x2B38}, {5, 6, 7}, 600, 1},
    };
    // Captions from (112, 284), a line every height + 4 (measured on the
    // original: f38_0718 places them by bitmaps 1318 and 1319's sizes).
    constexpr int kTextX = 112, kTextY = 284;
    narration(8, true);  // SILENT
    waitNarration();
    bool skip = false;
    for (const Page& page : kPages) {
        if (skip) break;
        select(2);
        // Only the first page goes through f14_092c (with the look).
        if (&page == &kPages[0]) showScreenWithLook(page.picture, 2);
        else showScreen(page.picture, 2);
        int y = kTextY;
        for (uint16_t line : page.lines) {
            font_->draw(ctx_.screens[2], kTextX, y, dataString(line), 0x10);
            y += font_->height() + 4;
        }
        toDisplay(2);
        for (int sound : page.sounds) {
            narration(sound, true);
            waitNarration();
            if (escapePressed()) {
                skip = true;
                break;
            }
        }
        if (skip) break;
        for (int i = 0; i < page.waits; ++i)
            if (waitTicks(page.wait)) break;
        if (escapePressed()) skip = true;
    }
    ctx_.platform.stopWav();
    select(1);
    clearDisplay();
}

}  // namespace edison
