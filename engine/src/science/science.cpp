// WMAIN.EXE: start-up (segments 32 and 62) and the story (segment 38).

#include "science/science.h"
#include "formats/paths.h"

#include <algorithm>
#include <cstdlib>
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
    if (std::getenv("SCI_AIMSEARCH")) {
        // (Testing: the search is of a table room's shots, from --room.)
        logLine("SCI_AIMSEARCH: --room " + std::to_string(start) + " isn't a table room (1-110)");
        std::exit(0);
    }
    if (start < 0) {
        title();
        story();
    }
    // Event 9 (f31_0783) first sends the music off (f32_135d(0)): so the
    // title's song plays through the story till the lab.
    if (start < 0 || start == 501) {
        fmSound(0);
        lab();  // room 501
    }
    if (start >= 505 && start <= 510) {
        // (Testing: the look and name as the lab would leave them.)
        loadLook();
        looksConverted_ = true;
        playerName_ = dataString(0x1D6F);  // "Player"
    }
    // Room 501 goes on to the first lesson (f31_0783); the others come
    // from the arcade's holes.
    int room = -1;
    if (start < 0 || start == 501) {
        fmSound(0);  // (event 9)
        room = lesson(5);  // room 505, then room 1
    }
    else if (start >= 505 && start <= 510) room = start;  // (the arcade's loop plays it)
    if (room > 0) arcade(room);
    if (options_.music) ctx_.platform.setFmDriver(std::string());
}

void Science::arcade(int room) {
    // f31_0783, event 9: rooms 1-100 are tables (the old one's objects
    // go; its columns' counts stay the player's); 501 the lab's name
    // (f38_06dd), then room 1 again when it came from there (else the
    // first lesson); 502 the high scores (f40_0000), then room 1 (from
    // room 1, event 9; after a game over, event 2: a new game, f31_1b45);
    // 503 the credits (f38_0eb9), then room 1; 504 leaves (event 3);
    // 505-510 the professor's lessons (f38_0fb5), each leading on.
    int from = 0;  // the player's +90: the last table
    // (Testing: SCI_BALLS=l,r, the columns' balls to start with.)
    if (const char* balls = std::getenv("SCI_BALLS")) std::sscanf(balls, "%d,%d", &leftBalls_, &rightBalls_);
    while (room > 0) {
        // Event 9 begins with the music off (f32_135d(0)); a table's comes on
        // when it's built (enterRoom), a lesson's with its picture.
        fmSound(0);
        if (room <= 110) {
            // The last table's end (f38_020f), unless this is room 1 (a
            // lesson between keeps the old room).
            if (from > 0 && room != 1) roomEnd();
            from = room;
            const int next = playRoom(room);
            previousRoom_ = room;
            room = next;
            continue;
        }
        switch (room) {
        case 501:
            previousRoom_ = 501;
            lab();
            room = from == 1 ? 1 : 505;
            break;
        case 502:
            previousRoom_ = 502;
            highScores();
            if (gameOver_) {
                // (Event 2, f31_1b45: a new game, in room 1.)
                gameOver_ = false, gameWon_ = false;
                leftBalls_ = 7, rightBalls_ = 0, totalScore_ = 0;
                std::fill(std::begin(gameFlag_), std::end(gameFlag_), 0);
            }
            room = 1;
            break;
        case 503:
            previousRoom_ = 503;
            credits();
            room = 1;
            break;
        case 504:
            return;
        default:
            if (room >= 505 && room <= 510) {
                if (from > 0 && roomEndFlag_) roomEnd();
                previousRoom_ = room;
                const int n = room - 500;
                room = lesson(n);
                quietScore_ = true;  // f15_08b7, the lesson's end: [171C]
                if (n == 10) {
                    // Its last step (15:29C5): the game's won ([26CC]),
                    // then f31_06c0: the game recorded (f40_068b: room
                    // 65's level and screen), the high scores (f40_0000),
                    // event 2 (a new game).
                    gameWon_ = true;
                    recordGame();
                    gameOver_ = true, room = 502;
                }
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
    // Then, whatever the screen, its colours 1-254 to the display (f70_0557)
    // and its palette into the other screens' (f70_07c5): so the table's
    // picture (2002, on screen 3) brings the room's colours before its
    // greeting, even after a lesson's picture.
    for (int s = 1; s <= 3; ++s)
        if (s != screen) ctx_.screens[s].palette = pal;
    ctx_.setDisplayPalette(screen);
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
    // f32_135d ([26D0], always 1): with the sounds off ([27F6]) only 0, the
    // music off. 25 is the arcade's music, one of three songs at random
    // (Borland's rand x 20 / 8000h: under 8 song 31, under 14 song 25, else
    // 39; a byte it works out from the first is never used), drawn even
    // with the music off. Sent with the music on ([5FF6]: no switch on the
    // command line).
    if (!soundsOn_ && sound != 0) return;
    if (sound == 0x25) {
        const long r = static_cast<long>(borlandRand()) * 20 / 0x8000;
        sound = r < 8 ? 0x31 : r < 14 ? 0x25 : 0x39;
    }
    if (options_.music) ctx_.platform.sendFm(sound);
}

void Science::narration(int n, bool story) {
    // f36_00ad: the name, 9 bytes apart, then the CD's \science\ or the
    // game's data\ folder.
    // (The second list is reached as the original does, wrapping at 64 KB:
    // (n * 9 + 9700) & FFFF, so the lessons' 6169 is WSA1521.)
    if (!soundsOn_) return;  // ([27F6])
    const size_t at = story ? 0x519u + 9u * static_cast<size_t>(n) : (static_cast<unsigned>(n) * 9u + 0x9700u) & 0xFFFFu;
    std::string name;
    for (size_t i = at; i < strings_.size() && strings_[i] && name.size() < 9; ++i) name += static_cast<char>(strings_[i]);
    std::ifstream in(findPath(cdRoot_ + "/SCIENCE/" + name + ".WAV"), std::ios::binary);
    if (!in) in.open(findPath(options_.saveDir + "/data/" + name + ".wav"), std::ios::binary);
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

bool Science::waitTicks(int ticks, bool interruptible, int* key) {
    // [9558] (a key) and [6EC5] (a button) end it.
    const uint64_t end = ctx_.platform.milliseconds() + static_cast<uint64_t>(ticks) * 20;
    int x, y;
    while (ctx_.platform.milliseconds() < end) {
        ctx_.pump();
        if (!interruptible) continue;
        if (ctx_.platform.takeClick(&x, &y)) return true;
        if (const int k = ctx_.platform.takeKey(); k != 0) {
            if (key) *key = k;
            return true;
        }
    }
    return false;
}

void Science::soundsOver() {
    // f36_004e: the sounds over ([27F6]); off, the WAV stopped (f75_0000)
    // and the music off (SENDSND(0), with [5FF6]). On again, no music.
    soundsOn_ = !soundsOn_;
    if (!soundsOn_) {
        ctx_.platform.stopWav();
        fmSound(0);
    }
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
    // green) and the narration. After each step (a page on the display, a
    // sound played out, a wait) the keys pressed meanwhile: Escape (scan
    // code 1, [9560] bit 1) skips the rest, else S (1Fh, [9563] bit 7) turns
    // the sounds over (f36_004e: off, the title's music stops and the
    // narration's sounds aren't played, f36_00ad).
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
    int taken = 0;  // a key that ended a wait
    const auto keys = [&] {
        if (escapePressed()) return skip = true;
        bool s = taken == 's' || taken == 'S';
        taken = 0;
        while (const int k = ctx_.platform.takeKey())
            if (k == 's' || k == 'S') s = true;
        if (s) soundsOver();
        return false;
    };
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
        ctx_.pump();  // (f36_0000: one message)
        if (keys()) break;
        for (int sound : page.sounds) {
            narration(sound, true);
            waitNarration();
            if (keys()) break;
        }
        if (skip) break;
        for (int i = 0; i < page.waits; ++i)
            if (waitTicks(page.wait, true, &taken)) break;
        keys();
    }
    ctx_.platform.stopWav();
    select(1);
    clearDisplay();
}

}  // namespace edison
