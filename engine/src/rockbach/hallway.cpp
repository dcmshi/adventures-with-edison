// WINMAIN's segment 24: the hallway. The first time, Edison walks in, asks
// the player's name (user.yyy keeps each player's look for him) and offers
// to change how he looks; then the hallway waits for a click on a door, the
// sign or the credits.

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>

#include "rockbach/rockbach.h"

namespace edison {
namespace {

constexpr int kSignX = 0x20A, kSignY = 0x150, kSignW = 0x76, kSignH = 0x3E;
constexpr int kEdisonY = 0xF0, kEdisonW = 0x56, kEdisonH = 0x8C;
constexpr int kRecords = 26, kRecordSize = 0x19, kNameSize = 0x13;  // user.yyy
constexpr int kEdisonSlot = 7, kSignSlot = 8;                       // timer slots

}  // namespace

void RockBach::hallwayWidgets() {
    // f24_0000: four hot spots over the look menu and its "done" (no
    // bevels, nothing drawn), then three pairs of Yes / No buttons.
    auto spot = [](int x0, int y0, int x1, int y1) { return Widget{x0, y0, x1, y1, 0x204}; };
    auto yesNo = [](int x0, int y0, int x1, int y1, bool yes) {
        Widget w{x0, y0, x1, y1, 0x300};
        w.faceDown = w.faceUp = 0x74;
        w.bitmapDown = yes ? 0x2201 : 0x2202;
        w.bitmapUp = yes ? 0x21F1 : 0x21F2;
        w.overlayUp = w.overlayDown = 0x2000;
        return w;
    };
    Widget done = spot(0x1C9, 0xDA, 0x237, 0x106);
    done.bitmapDown = 0x21E5;
    done.bitmapUp = 0x21E6;
    hallwayWidgets_ = {
        spot(0x1BD, 0x50, 0x231, 0x6E), spot(0x1BD, 0x6F, 0x231, 0x8D), spot(0x1BD, 0x8D, 0x231, 0xAB),
        spot(0x1BD, 0xAB, 0x231, 0xC9), done,
        yesNo(0xE6, 0x7E, 0x129, 0xAD, true), yesNo(0xE6, 0xAE, 0x129, 0xDD, false),   // (unused)
        yesNo(0x98, 0x7E, 0xDB, 0xAD, true), yesNo(0x98, 0xAE, 0xDB, 0xDD, false),     // change my look?
        yesNo(0x8C, 0x7E, 0xCF, 0xAD, true), yesNo(0x8C, 0xAE, 0xCF, 0xDD, false),     // quit?
    };
}

int RockBach::hallway(bool again) {
    // f24_1d4a.
    hallwayWidgets();
    look_.fill(0);
    blackout();
    backdrop(0x1007);
    show(2);
    initWidgets(hallwayWidgets_, {0x63, 0x3F, 0x3A, 0x38});
    select(1);
    ctx_.timer.setPeriodic(kEdisonSlot, 5, [this] { edisonTick_ = true; });  // g24_0650
    edisonArea_ = saveArea(0, 0, 2, 2);
    int sign1 = saveArea(kSignX, kSignY, kSignW, kSignH);
    select(2);
    int sign2 = saveArea(kSignX, kSignY, kSignW, kSignH);
    select(1);
    edisonTick_ = false;
    if (!again) {
        loadLook();
        lookColours(0);
        clearInput();
        // He walks in and says hello.
        edison(0, 0);
        edison(0, 0);
        edison(1, 8);
        edison(22, 23);
        clearInput();
        const int hello = say(0, 1, -1, -1, false);  // "Hi I'm Edison." "What's your name?"
        sound(0x6020);
        edison(22, 23);
        edison(22, 24);
        edison(22, 23);
        edison(22, 24);
        enterName();
        restoreArea(hello);
        bool found = false;
        findPlayer(&found);
        if (found) lookColours(0);
        if (!found) {
            // A new player makes Edison's look.
            clearInput();
            lookEditor();
            savePlayer(false);
            clearInput();
            edison(12, 13);
            edison(13, 14);
            sound(0x6001);  // cool
            edison(15, 15);
            edison(15, 15);
            edison(20, 20);
            edison(17, 18);
            sound(0x6029);  // what do you want to do?
            edison(22, 24);
            edison(22, 24);
            edison(17, 19);
        } else {
            lookColours(0);
            const int question = say(6, 13, 8, 9, false);  // "Do you want to change the" "way, I look?"
            clearInput();
            edison(22, 24);
            const int change = askChangeLook();
            restoreArea(question);
            if (change == 0) {
                clearInput();
                lookEditor();
                savePlayer(true);
            }
            edison(20, 20);
            edison(22, 23);
            sound(0x6029);
            edison(22, 24);
            edison(22, 24);
            edison(22, 24);
        }
        saveLook();
    }
    backdrop(0x1006);  // the hot-spot mask, on screen 2
    clearInput();
    // The sign: frames of 22BA, 3 a second, when it's clicked. (It draws
    // on screen 2 over the mask, so the sign's rectangle then reads the
    // hallway's colours until the mask comes back: as in the original.)
    static const int kSign[16] = {0, 1, 2, 3, 4, 5, 6, 7, 10, 7, 10, 11, 12, 13, 14, 15};
    bool signOn = false, signDue = false;  // [bp-14], [21B4]
    int signFrame = 0;
    int result = 0, spot = 0;
    while (result == 0) {
        ctx_.pump();
        if (signOn && signDue) {
            if (signFrame < 16) {
                if (kSign[signFrame] == 6) sound(0x6001);
                select(2);
                restoreArea(sign1);
                sign1 = saveArea(kSignX, kSignY, kSignW, kSignH);
                drawLogo(kSignX, kSignY, static_cast<uint16_t>(0x22BA + kSign[signFrame]));
                select(1);
                copyArea(2, 1, kSignX, kSignY, kSignW, kSignH);
                ++signFrame;
            } else {
                signOn = false;
                signFrame = 0;
                ctx_.timer.setPeriodic(kSignSlot, 0, nullptr);
                select(2);
                restoreArea(sign2);
                sign2 = saveArea(kSignX, kSignY, kSignW, kSignH);
                select(1);
                restoreArea(sign1);
                sign1 = saveArea(kSignX, kSignY, kSignW, kSignH);
            }
            signDue = false;
        }
        int x, y;
        if (!ctx_.platform.takeClick(&x, &y)) continue;
        if (x < 0 || y < 0 || x >= Screen::kWidth || y >= Screen::kHeight) continue;
        spot = ctx_.screens[2].pixels[static_cast<size_t>(y) * Screen::kWidth + x];  // f37_23ce
        if (spot >= 1) result = 1;
        if (spot == 5) {
            result = 0;
            credits();
        }
        if (spot == 10) {
            result = 0;
            signDue = false;
            signOn = true;
            signFrame = 0;
            restoreArea(sign1);
            sign1 = saveArea(kSignX, kSignY, kSignW, kSignH);
            select(2);
            restoreArea(sign2);
            sign2 = saveArea(kSignX, kSignY, kSignW, kSignH);
            select(1);
            ctx_.timer.setPeriodic(kSignSlot, 3, [&signDue] { signDue = true; });  // g24_0432
        }
        if (spot == 1) {
            loadLook();
            lookColours(0);
            result = askQuit();
        }
        clearInput();
    }
    ctx_.timer.setPeriodic(kSignSlot, 0, nullptr);
    ctx_.timer.setPeriodic(kEdisonSlot, 0, nullptr);
    freeArea(sign1);
    freeArea(sign2);
    freeArea(edisonArea_);
    return spot;
}

void RockBach::edison(int from, int to) {
    // f24_0666: Edison's frames 21C3 + n on screen 2 (each over the last
    // one's saved area) and onto the display, 5 a second. A click or key
    // (not cleared here: it skips the frames after it too) goes to the last.
    static const int kX[25] = {0x64, 0x64, 0x6B, 0x88, 0xA8, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6,
                               0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6, 0xD6};
    auto frame = [&](int n) {
        restoreArea(edisonArea_);
        edisonArea_ = saveArea(kX[n], kEdisonY, kEdisonW, kEdisonH);
        drawLogo(kX[n], kEdisonY, static_cast<uint16_t>(0x21C3 + n));
        copyArea(2, 1, kX[n], kEdisonY, kEdisonW, kEdisonH);
    };
    select(2);
    for (int n = from; n <= to; ++n) {
        frame(n);
        if (n > from) copyArea(2, 1, kX[n - 1], kEdisonY, kEdisonW, kEdisonH);
        edisonTick_ = false;
        while (!edisonTick_) ctx_.pump();
        if (anyInput()) inputSeen_ = true;
        if (inputSeen_ && n < to) {
            frame(to);
            copyArea(2, 1, kX[n], kEdisonY, kEdisonW, kEdisonH);
            break;
        }
    }
    select(1);
}

void RockBach::bubble(int x, int y, int w, int h, int tail) {
    // f24_0448: a speech bubble in colour EF with its corners; the tail at
    // the right (0) or the left (1).
    x = std::max(x, 0);
    y = std::max(y, 0);
    fill(x + 30, y + 2, w - 30, h + 26, 0xEF);
    fill(x + 2, y + 30, w + 26, h - 30, 0xEF);
    drawLogo(x, y, 0x229A);
    drawLogo(x + w, y, 0x229B);
    drawLogo(x, y + h, 0x229C);
    drawLogo(x + w, y + h, 0x229D);
    if (tail == 0) drawLogo(x + w - 30, y + h + 28, 0x229E);
    if (tail == 1) drawLogo(x + 30, y + h + 28, 0x229E);
}

int RockBach::say(int a, int b, int c, int d, bool withName) {
    // f24_1452: Edison's bubble with up to four of his lines (-1 none): two
    // centred, then two next to the Yes and No buttons. Returns the saved
    // area under it.
    static const uint16_t kLines[16] = {0x23E7, 0x2408, 0x2432, 0x244F, 0x2457, 0x245D, 0x247A, 0x24BE,
                                        0x24EC, 0x24F3, 0x24FF, 0x2521, 0x2540, 0x2553, 0x256C, 0x2588};
    auto line = [&](int i) { return dataString(kLines[i]); };
    std::string first = line(a);
    if (withName) first += dataString(0x259A) + name_;
    int w = font_->width(first) + 20, h = 20;
    if (b >= 0) w = std::max(w, font_->width(line(b)) + 20), h = 0x24;
    if (c >= 0) w = std::max(w, font_->width(line(c)) + 0x60), h = 0x50;
    if (d >= 0) w = std::max(w, font_->width(line(d)) + 0x60), h = 0x80;
    w = std::max(w, 0x28);
    h = std::max(h, 0x28);
    const int x = std::max(0, 0x148 - w), y = std::max(0, 0xC8 - h);
    const int handle = saveArea(x, y, w + 30, h + 30 + ctx_.bitmap(0x229E).height);
    bubble(x, y, w, h, 0);
    // f49_0020: straight onto the current screen.
    const int tx = x + 16;
    Screen& s = ctx_.screens[current()];
    font_->draw(s, tx + w / 2 - font_->width(first) / 2, y + 12, first, 2);
    if (b >= 0) font_->draw(s, tx + w / 2 - font_->width(line(b)) / 2, y + 28, line(b), 2);
    if (c >= 0) font_->draw(s, tx + 0x60, y + 60, line(c), 2);
    if (d >= 0) font_->draw(s, tx + 0x60, y + 108, line(d), 2);
    return handle;
}

void RockBach::enterName() {
    // f24_058e: at most 10 letters, upper case; "Player" when none.
    do {
        name_.clear();
    } while (!editLine(name_, 11, 0xD0, 0xCE, 11 * 12, 2));
    if (name_.empty()) name_ = dataString(0x21B5);
    for (char& c : name_) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

bool RockBach::editLine(std::string& s, int max, int x, int y, int w, uint8_t colour) {
    // f04_0d52: a line with a '|' cursor, typed until Enter (true) or Escape
    // (false, the line as it was).
    const std::string before = s;
    for (;;) {
        const int handle = saveArea(x, y, w + 20, 16);
        text(x, y, s + "|", colour);
        int key = 0;
        while ((key = ctx_.platform.takeKey()) == 0) ctx_.pump();  // f32_0070
        restoreArea(handle);
        if (key == Platform::kEnter) return true;
        if (key == Platform::kEscape) {
            s = before;
            return false;
        }
        if (key == Platform::kBackspace) {
            if (!s.empty()) s.pop_back();
        } else if (key >= 0x20 && key < 0x100 && static_cast<int>(s.size()) < max - 1 && font_->width(s) < w) {
            s += static_cast<char>(key);
        }
    }
}

// --- user.yyy and ed.yyy ------------------------------------------------------

void RockBach::findPlayer(bool* found) {
    // f24_09c0: 26 records of 0x19 bytes: the name (13 bytes, "***" when
    // free), 0, a flag byte, the look. The player's own record, else the
    // first free one, else the first with the flag clear.
    *found = false;
    slot_ = -1;
    playerFlag_ = 0;
    const std::string path = options_.saveDir + "/user.yyy";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::vector<char> blank(static_cast<size_t>(kRecords) * kRecordSize, 0);
        for (int i = 0; i < kRecords; ++i) std::fill_n(blank.begin() + i * kRecordSize, kNameSize, '*');
        std::ofstream out(path, std::ios::binary);
        if (!out) {
            logLine("Rock and Bach: can't create " + path);
            slot_ = 0;
            return;
        }
        out.write(blank.data(), static_cast<std::streamsize>(blank.size()));
        slot_ = 0;
        return;
    }
    std::vector<uint8_t> records(static_cast<size_t>(kRecords) * kRecordSize, 0);
    in.read(reinterpret_cast<char*>(records.data()), static_cast<std::streamsize>(records.size()));
    std::string padded = name_;
    padded.resize(kNameSize, '\0');
    for (int i = 0; i < kRecords; ++i) {
        const uint8_t* r = &records[static_cast<size_t>(i) * kRecordSize];
        if (std::strncmp(reinterpret_cast<const char*>(r), padded.c_str(), kNameSize) == 0) {
            std::copy_n(r + 0x15, 4, look_.begin());
            playerFlag_ = r[0x14];
            slot_ = i;
            *found = true;
            return;
        }
    }
    for (int i = 0; i < kRecords && slot_ < 0; ++i)
        if (std::memcmp(&records[static_cast<size_t>(i) * kRecordSize], "***", 3) == 0) slot_ = i;
    for (int i = 0; i < kRecords && slot_ < 0; ++i)
        if (records[static_cast<size_t>(i) * kRecordSize + 0x14] == 0) slot_ = i;
}

void RockBach::savePlayer(bool keep) {
    // f24_088a: the name and look into the player's record (the flag kept
    // or cleared).
    if (slot_ < 0) return;
    uint8_t record[kRecordSize] = {};
    std::memcpy(record, name_.data(), std::min<size_t>(name_.size(), kNameSize));
    record[0x14] = keep ? static_cast<uint8_t>(playerFlag_) : 0;
    std::copy(look_.begin(), look_.end(), record + 0x15);
    std::fstream f(options_.saveDir + "/user.yyy", std::ios::binary | std::ios::in | std::ios::out);
    if (!f) {
        logLine("Rock and Bach: error trying to create user.yyy");
        return;
    }
    f.seekp(static_cast<std::streamoff>(slot_) * kRecordSize);
    f.write(reinterpret_cast<const char*>(record), kRecordSize);
}

void RockBach::loadLook() {
    // f24_0e94: Edison's look from ed.yyy (the last player's).
    std::ifstream in(options_.saveDir + "/ed.yyy", std::ios::binary);
    if (in) in.read(reinterpret_cast<char*>(look_.data()), 4);
}

void RockBach::saveLook() {
    // f24_0f10.
    std::ofstream out(options_.saveDir + "/ed.yyy", std::ios::binary);
    if (out) out.write(reinterpret_cast<const char*>(look_.data()), 4);
}

void RockBach::lookColours(int which) {
    // f24_0cd8: colours E1-ED are Edison's four parts (3, 3, 4 and 3
    // colours, 8 choices each). 0: all four into screen 2's palette (and
    // the display as it was); 1-4: that part onto the display. The
    // colours shown are kept (DS:8CC0).
    static const uint16_t kTables[4] = {0x223E, 0x2286, 0x22CE, 0x232E};
    static const int kCount[4] = {3, 3, 4, 3}, kFirst[4] = {0xE1, 0xE4, 0xE7, 0xEB};
    if (lookTables_[0].empty()) {
        // f24_0c0c: the tables are 6-bit R, G, B; only the first 0x18
        // colours of each are made 8-bit and reversed into the palettes'
        // B, G, R (seg 63's entries go to the RGBQUADs as they are,
        // f43_0186), so the last two choices of the third part stay as
        // they are: as in the original.
        for (int t = 0; t < 4; ++t)
            for (int k = 0; k < 8 * kCount[t]; ++k) {
                const uint8_t* p = &data_[kTables[t] + 3 * k];
                lookTables_[t].push_back(k < 0x18 ? Rgb{static_cast<uint8_t>(p[0] << 2), static_cast<uint8_t>(p[1] << 2),
                                                        static_cast<uint8_t>(p[2] << 2)}
                                                  : Rgb{p[2], p[1], p[0]});
            }
    }
    auto part = [&](int t) {
        const auto begin = lookTables_[t].begin() + look_[t] % 8 * kCount[t];
        return std::vector<Rgb>(begin, begin + kCount[t]);
    };
    if (which == 0) {
        for (int t = 0; t < 4; ++t) {
            const std::vector<Rgb> colours = part(t);
            std::copy(colours.begin(), colours.end(), ctx_.screens[2].palette.begin() + kFirst[t]);
        }
        const Palette& p = ctx_.screens[1].palette;
        setColours(std::vector<Rgb>(p.begin() + 1, p.begin() + 0xFF), 1);
    } else if (which >= 1 && which <= 4) {
        setColours(part(which - 1), kFirst[which - 1]);
    }
    std::copy_n(ctx_.screens[1].palette.begin() + 0xE1, savedLook_.size(), savedLook_.begin());
}

// --- the questions ------------------------------------------------------------

void RockBach::lookEditor() {
    // f24_1bba: the look menu (f24_1b00); a click on one of its four rows
    // takes that part's next choice. Edison nags after 8 s, then every 20 s
    // (the count starting again at each click), until "done" (or Enter).
    select(2);
    for (int i = 0; i < 5; ++i) hallwayWidgets_[i].flags &= ~Widget::kHidden;
    const int panel = saveArea(0x186, 10, 0xF8, 0x186);
    drawLogo(0x186, 10, 0x21E2);
    drawLogo(0x186, 0xCD, 0x21E3);
    drawLogo(0x1BD, 0x47, 0x21E4);
    select(1);
    copyArea(2, 1, 0x186, 10, 0xF8, 0x186);
    clearInput();
    edison(9, 10);
    edison(10, 11);
    sound(0x6028);
    edison(17, 18);
    edison(17, 19);
    int r = -1;
    clearInput();
    ctx_.countdown[0] = 0x50;
    do {
        int mx, my;
        bool down;
        ctx_.platform.mouse(&mx, &my, &down);
        if (down) ctx_.countdown[0] = 0xC8;
        r = pollWidgets();
        if (lastKey_ == Platform::kEnter) r = 4;
        if (ctx_.countdown[0] == 0 && r != 4) {
            sound(0x6022);
            edison(17, 18);
            edison(17, 19);
            ctx_.countdown[0] = 0xC8;
        }
        if (r >= 0 && r <= 3) {
            look_[r] = look_[r] + 1 > 7 ? 0 : static_cast<uint8_t>(look_[r] + 1);
            lookColours(r + 1);
        }
        if (r == 4) {
            drawLogo(0x1C9, 0xDA, 0x21E5);
            hideWidgets();
        }
    } while (r != 4);
    restoreArea(panel);
    clearInput();
}

int RockBach::askChangeLook() {
    // f24_191e: Yes (0) or No (1, or Enter), with Edison talking.
    clearInput();
    showWidgets({7, 8});
    sound(0x6021);
    edison(22, 24);
    edison(22, 24);
    edison(22, 24);
    int answer = 1;
    for (int r = -1; r == -1;) {
        r = pollWidgets();
        if (lastKey_ == Platform::kEnter) answer = 1, r = 1;
        if (r == 7) answer = 0, r = 1;
        if (r == 8) answer = 1, r = 1;
    }
    hideWidgets();
    clearInput();
    return answer;
}

int RockBach::askQuit() {
    // f24_0f8c: "Do you want to quit this game?" in a bubble over a clean
    // hallway (with Edison's colours), Yes (1) or No (0).
    const std::string lines[3] = {dataString(0x23AB), dataString(0x23CE), dataString(0x23D5)};
    int w = 0;
    for (const std::string& s : lines) w = std::max(w, font_->width(s) + 12);
    const int h = 120, x = std::max(0, 0x148 - w), y = std::max(0, 0xC8 - h);
    const int saved = saveArea(x, y, w + 30, Screen::kHeight - y);
    select(2);
    ctx_.showFullScreen(0x1007, 2);
    Palette& p = ctx_.screens[2].palette;
    std::copy(savedLook_.begin(), savedLook_.end(), p.begin() + 0xE1);
    setColours(std::vector<Rgb>(p.begin() + 1, p.begin() + 0xFF), 1);
    drawLogo(0xD6, 0x122, 0x2413);
    bubble(x, y, w, h, 0);
    for (int i = 0; i < 3; ++i)
        font_->draw(ctx_.screens[2], x + (w + 12) / 2 - font_->width(lines[i]) / 2, y + 6 + 44 * i, lines[i], 2);
    showWidgets({9, 10});
    select(1);
    copyArea(2, 1, x, y, w + 30, Screen::kHeight - y);
    static const int kTalk[13] = {3, 1, 0, 1, 2, 3, 1, 3, 0, 3, 2, 2, 4};
    sound(0x601E);
    for (int f : kTalk) {
        drawLogo(0xD6, 0x122, static_cast<uint16_t>(0x2413 + f));
        waitCountdown(1);
    }
    int answer = 0;
    for (int r = -1; r == -1;) {
        r = pollWidgets();
        if (r == 9) answer = 1, r = 1;
        if (r == 10) answer = 0, r = 1;
    }
    hideWidgets();
    restoreArea(saved);
    backdrop(0x1006);
    return answer;
}

void RockBach::credits() {
    // f24_12e6: backdrop 100E for 40 s or until a key or click; then the
    // hallway as it was.
    const int previous = current();
    select(1);
    const int saved = saveArea(0, 0, Screen::kWidth, Screen::kHeight);
    blackout();
    ctx_.showFullScreen(0x100E, 3);
    show(3);
    clearInput();
    waitOrClick(400);
    clearInput();
    blackout();
    ctx_.showFullScreen(0x1007, 3);
    show(3);
    backdrop(0x1006);
    restoreArea(saved, true);  // f37_1664: onto the display only (screen 3 keeps the clean hallway)
    select(previous);
}

}  // namespace edison
