// MALL.EXE segment 9: the game loop and the map screen (f09_1dd8).

#include <algorithm>
#include <cstdio>
#include <random>

#include "mystery/mystery.h"

namespace edison {

namespace {

// f46_001d: random(n).
int random(int n) {
    static std::mt19937 rng{std::random_device{}()};
    return std::uniform_int_distribution<int>(0, n - 1)(rng);
}

// f09_1dd8's per-level tables (immediates in its code): the time limit in
// seconds, which of the 29 squares are used, how many of the 16 object
// slots are filled, and where the level's puzzles start in segment 62.
struct Level {
    int seconds;
    const char* squares;  // '1' = used
    int objects;
    int puzzles;          // offset in segment 62, 4 bytes per used square
};
constexpr Level kLevels[8] = {
    {0x708, "10100000000000000100010100010", 3, 0x000},
    {0x708, "10101000000000000100010100011", 4, 0x018},
    {0xE10, "10001000000000000100111100011", 6, 0x038},
    {0xE10, "11111100000000000100101100011", 8, 0x05C},
    {0xE10, "11111000000000000100111110111", 8, 0x08C},
    {0x1518, "00101000011111110100111100011", 10, 0x0C4},
    {0x1518, "00001101111111100001101100011", 12, 0x104},
    {0x1518, "11111110000111110110101101111", 16, 0x144},
};
// Object slots per level (the second mask): slots in use, in order.
constexpr const char* kObjectSlots[8] = {
    "1110000000000000", "1111000000000000", "1111011000000000", "1111111100000000",
    "1111111100000000", "1111111101100000", "1111111111110000", "1111111111111111",
};

// The object grid (DS:0CF8): 16 buttons of 10 bytes, relative to (DC, 5C).
constexpr int kGridX = 0xDC, kGridY = 0x5C;
int slotX(int k) { return (k % 4) * 0x36 + 4; }
int slotY(int k) { return (k / 4) * 0x2C + 6; }
constexpr int kSlotW = 0x30, kSlotH = 0x24;

// The timer slot of Smitty's idle moments (g05_03de).
constexpr int kIdleSlot = 7;

// The clock's hands (DS:0CA0, 0CA8): length, start and end angles, period.
struct Hand {
    int length;
    uint16_t start, end;
    int period;
};

}  // namespace

const char* Mystery::levelSquares(int level) { return kLevels[std::clamp(level, 0, 7)].squares; }

int Mystery::levelPuzzles(int level) { return kLevels[std::clamp(level, 0, 7)].puzzles; }

// --- drawing ------------------------------------------------------------

void Mystery::show(int screen) {
    ctx_.setDisplayPalette(screen);
    ctx_.screens[1].palette = ctx_.screens[screen].palette;
    ctx_.screens.copyAll(screen, 1);
}

void Mystery::backdrop(uint16_t id) {
    ctx_.blackout();
    ctx_.showFullScreen(id, 2);
}

void Mystery::drawOpaque(int x, int y, uint16_t id) {
    const Bitmap& bmp = ctx_.bitmap(id);
    if (x + bmp.width > Screen::kWidth) x = Screen::kWidth - bmp.width - 1;
    if (y + bmp.height > Screen::kHeight) y = Screen::kHeight - bmp.height - 1;
    x = std::max(x, 0);
    y = std::max(y, 0);
    drawVia3(x, y, bmp.width, bmp.height, [&](int s) { ctx_.screens.drawSprite(s, bmp, x, y, true); });
}

void Mystery::line(int x0, int y0, int x1, int y1, uint8_t colour) {
    x1 = std::min(x1, Screen::kWidth - 1);
    y1 = std::min(y1, Screen::kHeight - 1);
    x0 = std::max(x0, 0);
    y0 = std::max(y0, 0);
    auto plot = [&](int s) {
        Screen& scr = ctx_.screens[s];
        const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy, x = x0, y = y0;
        for (;;) {
            if (x >= 0 && x < Screen::kWidth && y >= 0 && y < Screen::kHeight)
                scr.pixels[static_cast<size_t>(y) * Screen::kWidth + x] = colour;
            if (x == x1 && y == y1) break;
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x += sx; }
            if (e2 <= dx) { err += dx; y += sy; }
        }
    };
    const int x = std::min(x0, x1), y = std::min(y0, y1);
    drawVia3(x, y, std::abs(x1 - x0) + 1, std::abs(y1 - y0) + 1, plot);
}

void Mystery::clockHand(int cx, int cy, uint16_t hand, int value, uint8_t colour) {
    // f05_00a0: the angle runs from start to end over the period (a full
    // turn is 0x10000, 0 pointing right, clockwise); f51_103b is sine.
    Hand h{data_[hand], dataWord(hand + 1), dataWord(hand + 3), static_cast<int16_t>(dataWord(hand + 5))};
    if (hand == 0x0CA8) {
        h.start = hourStart_;
        h.period = hourPeriod_;
    }
    if (h.period < value) value = h.period;
    uint16_t range = static_cast<uint16_t>(h.end - h.start);
    if (range == 0) range = 0xFFFF;
    const int32_t turned = h.period ? static_cast<int32_t>(static_cast<int64_t>(range) * value / h.period) : 0;
    const uint16_t angle = static_cast<uint16_t>(h.start + turned);
    auto sine = [&](uint16_t a) {
        int index = (a >> 2) & 0xFFE;
        if (a & 0x4000) index = 0xFFE - index;
        int v = (sine_[index] | sine_[index + 1] << 8) >> 4;
        return (a & 0x8000) ? -v : v;
    };
    const int x = (h.length * sine(static_cast<uint16_t>(angle + 0x4000))) >> 12;
    const int y = (h.length * sine(angle)) >> 12;
    line(cx, cy, cx + x, cy + y, colour);
}

void Mystery::digitalTime(int x, int y, int w, int h, int seconds) {
    const int previous = current();
    select(2);
    char text[16];
    std::snprintf(text, sizeof text, "%d:%02d", seconds / 60, seconds % 60);
    fill(x, y, w, h, 0);
    this->text((w - font_->width(text)) / 2 + x, (h - font_->height()) / 2 + y, text, 0xFF);
    copyArea(2, 1, x, y, w, h);
    select(previous);
}

void Mystery::number(int x, int y, int w, int h, long value) {
    const int previous = current();
    select(2);
    const std::string text = std::to_string(value);
    fill(x, y, w, h, 0);
    this->text((w - font_->width(text)) / 2 + x, (h - 0x10) / 2 + y, text, 0xFF);
    copyArea(2, 1, x, y, w, h);
    select(previous);
}

void Mystery::clock(bool force) {
    // f09_0ab2: the digital time left, and the clock face with its hands
    // for the time spent, redrawn each second.
    if (timeLeft_ == clockShown_ && !force) return;
    const int previous = current();
    digitalTime(0x1F1, 0xB8, 0x38, 0x16, timeLeft_);
    select(2);
    drawOpaque(0x1D4, 0x60, 0x22C1);
    clockHand(500, 0x80, 0x0CA8, timeTotal_ - timeLeft_, 0xCE);
    clockHand(500, 0x80, 0x0CA0, (timeTotal_ - timeLeft_) % 60, 0xCE);
    const Bitmap& face = ctx_.bitmap(0x22C1);
    copyArea(2, 1, 0x1D4, 0x60, face.width, face.height);
    select(previous);
    clockShown_ = timeLeft_;
}

void Mystery::drawObjects(int found) {
    // f09_08dc: each object's museum icon in its slot of the grid, with a
    // check on the found ones (except the one just found, which
    // smittySays checks with a flourish).
    int left = 0;
    for (int k = 0; k < 16; ++k) {
        const Object& o = objects_[k];
        if (o.museum == 0xFF) continue;
        const int x = slotX(k) + kGridX, y = slotY(k) + kGridY;
        drawOpaque(x, y, static_cast<uint16_t>(0x2295 + o.museum));
        if (!o.found) {
            ++left;
        } else if (found != 1 || squares_[square_].object != k) {
            drawLogo(x, y, 0x22A7);
        }
    }
    lastOne_ = left == 1;
    if (left == 0) outcome_ = 2;
}

bool Mystery::allFound() const {
    for (const Object& o : objects_)
        if (o.museum != 0xFF && !o.found) return false;
    return true;
}

bool Mystery::waitOrClick(int tenths) {
    ctx_.countdown[0] = tenths;
    while (ctx_.countdown[0] != 0) {
        ctx_.pump();
        if (anyInput()) return true;
    }
    return false;
}

// --- Edison and Smitty ----------------------------------------------------

void Mystery::mapTalk(int side, int frames) {
    // f09_0dc4: Edison talking on the map, standing (side 1) or pointing
    // at the list (side 0); the mouth moves every 0.2 s, then closes.
    // Screen 2's picture of Edison is kept (f06_1424 / f06_14fc) and put
    // back afterwards, so his talking pose only reaches the display.
    select(2);
    constexpr int kX = 0x1A3, kY = 0xE8, kW = 0xDC, kH = 0xA6;
    duplicateArea(2, 3, kX, kY, kW, kH, 0, 0xA0);
    const int mx = side ? 0x1E8 : 0x1E6, mw = side ? 0x3C : 0x52, mh = side ? 0x36 : 0x3A;
    duplicateArea(2, 3, mx, 0xE8, mw, mh, 0, 0);
    for (int i = 0; i <= frames; ++i) {
        if (side) {
            drawLogo(0x1DA, 0xE8, 0x22BD);
            drawLogo(mx, 0xE8, static_cast<uint16_t>(i < frames ? 0x22B6 + (i & 1) : 0x22B8));
        } else {
            drawLogo(0x1A4, 0xE8, 0x22BE);
            drawLogo(mx, 0xE8, static_cast<uint16_t>(i < frames ? 0x22B2 + (i & 1) : 0x22B1));
        }
        copyArea(2, 1, mx, 0xE8, mw, mh);
        duplicateArea(3, 2, 0, 0, mw, mh, mx, 0xE8);
        waitCountdown(2);
    }
    duplicateArea(3, 2, 0, 0xA0, kW, kH, kX, kY);
    select(1);
}

void Mystery::smittyTalk() {
    // f09_10fe: Smitty's mouth, four moves of 0.3 s.
    const int previous = current();
    select(2);
    duplicateArea(2, 3, 0x6C, 0xF0, 0x32, 0x5E, 200, 0);
    for (int i = 0; i <= 4; ++i) {
        drawLogo(0, 0x4A, 0x22AD);
        drawLogo(0x6C, 0xF0, static_cast<uint16_t>(i < 4 ? 0x22AA + (i & 1) : 0x22AB));
        copyArea(2, 1, 0x6C, 0xF0, 0x32, 0x5E);
        duplicateArea(3, 2, 200, 0, 0x32, 0x5E, 0x6C, 0xF0);
        waitCountdown(3);
    }
    select(previous);
}

void Mystery::smittySays(int mode, int result) {
    // f09_1212: Edison's words to Smitty. mode 0x80: Smitty just talks.
    // Otherwise a greeting (mode != 0) or the news after a game: result
    // 1 found an object (checked off with a blink), -1 came back.
    if (mode == 0x80) {
        smittyTalk();
        return;
    }
    const int previous = current();
    select(1);
    uint16_t lines, speech;
    if (mode != 0) {
        if (savedGame_) {
            lines = 0x11C8, speech = 0x4035;
        } else {
            lines = 0x11E8, speech = 0x4056;  // [B786] is never set by the port yet
        }
    } else if (result == -1) {
        lines = 0x122C, speech = 0x4042;
    } else if (allFound()) {
        lines = 0x1264, speech = 0x4045;
    } else {
        switch (random(3)) {
        case 0: lines = 0x1292, speech = 0x403D; break;
        case 1: lines = 0x12B8, speech = 0x4040; break;
        default:
            if (greeted_) {
                lines = 0x12E2, speech = 0x4031;
            } else {
                lines = 0x1292, speech = 0x403D;
                greeted_ = true;
            }
        }
    }
    const int bubble = speechBox(Screen::kWidth / 6, Screen::kHeight * 2 / 5, dataLines(lines), 0, true);
    sound(speech);
    smittyTalk();
    restoreArea(bubble);
    if (result == 1) {
        const int k = squares_[square_].object;
        const int x = slotX(k) + kGridX, y = slotY(k) + kGridY;
        drawLogo(x, y, 0x22A7);
        ctx_.countdown[0] = 2;
        for (int i = 0; i < 6; ++i) {
            while (ctx_.countdown[0] != 0) ctx_.pump();
            if (i & 1)
                recolour(x, y, kSlotW, kSlotH, 0xB4, 0xB9);
            else
                recolour(x, y, kSlotW, kSlotH, 0xB9, 0xB4);
            ctx_.countdown[0] = 2;
        }
    }
    select(previous);
}

void Mystery::entrance(bool first, int found) {
    // f09_15f8: Edison and Smitty come on (a table of sprites, each shown
    // for a while), with Edison's greeting at step 5 and, the first time,
    // "Okay, here are the objects." at step 8.
    struct Step {
        uint16_t sprite;
        int x, y, w, h, wait;
    };
    static const Step kSteps[] = {
        {0x22AE, 0x1DA, 0xE8, 0x53, 0x50, 6}, {0x22AD, 0, 0x4A, 0x3B, 0xA3, 4},
        {0x22A8, 0x72, 0xF0, 0x0E, 0x2F, 2},  {0x22A9, 0x6C, 0xF0, 0x19, 0x2F, 2},
        {0x22AA, 0x6C, 0xF0, 0x19, 0x2F, 2},  {0x22AB, 0x6C, 0xF0, 0x19, 0x2F, 4},
        {0x22AF, 0x1AA, 0xE6, 0x6B, 0x50, 2}, {0x22AC, 0x6C, 0xF0, 0x1A, 0x2F, 0},
        {0x22B0, 0x1A4, 0xE8, 0x6E, 0x50, 2}, {0x22AF, 0x1AA, 0xE6, 0x6B, 0x50, 2},
        {0x22AE, 0x1DA, 0xE8, 0x53, 0x50, 0}, {0x22AB, 0x6C, 0xF0, 0x19, 0x2F, 0},
    };
    select(2);
    music(0x23);
    waitCountdown(4);
    for (int i = 0; i < static_cast<int>(std::size(kSteps)); ++i) {
        const Step& s = kSteps[i];
        if (i == 1) music(0x28);
        // The area the sprite covers: Edison's poses after step 6 share
        // step 8's; Smitty's last pose is a little wider.
        int rx = s.x, ry = s.y, rw = s.w * 2, rh = s.h * 2;
        if (s.sprite >= 0x22AE && i >= 6) {
            rx = kSteps[8].x, ry = kSteps[8].y, rw = kSteps[8].w * 2, rh = kSteps[8].h * 2;
        } else if (i == 11) {
            rw += 4;
        }
        if (i != 1) duplicateArea(2, 3, rx, ry, rw, rh, 0x190, 0);
        if (s.sprite >= 0x22A8 && s.sprite <= 0x22AC) drawLogo(kSteps[1].x, kSteps[1].y, kSteps[1].sprite);
        drawLogo(s.x, s.y, s.sprite);
        if (s.sprite >= 0x22AE) drawLogo(0x1FC, 0x158, 0x22BB);
        copyArea(2, 1, rx, ry, rw, rh);
        if (i != 1) duplicateArea(3, 2, 0x190, 0, rw, rh, rx, ry);
        if (i == 8 && first) {
            museumList();
            select(1);
            const std::vector<std::string> lines = {dataString(0x12FD), dataString(0x1319)};
            const int bubble =
                speechBox(Screen::kWidth * 3 / 5, Screen::kHeight * 2 / 5, lines, 3, true);
            sound(0x401A);
            mapTalk(0, 5);
            restoreArea(bubble);
            select(2);
        } else if (i == 5) {
            smittySays(first ? 1 : 0, found);
            select(2);
        }
        waitCountdown(s.wait);
        if (!first && i == 5) break;
    }
    select(1);
}

void Mystery::museumList() {
    // f09_0f8a: " Look at this." and the Director's letter on the slide
    // screen, five lines at a time; the arrow button turns the page.
    select(1);
    int bubble = speechBox(Screen::kWidth * 3 / 5, Screen::kHeight * 2 / 5, dataLines(0x11A2), 3, true);
    sound(0x4019);
    mapTalk(0, 3);
    restoreArea(bubble);
    panels_.clear();
    Panels::Panel arrow;  // DS:0ECA
    arrow.x = 0x114, arrow.y = 0x156, arrow.w = 0x66, arrow.h = 0x3A;
    arrow.buttons = {{0, 0, 0x66, 0x3A}, {0, 0, 1, 1}};
    arrow.onPress = [this](int b) {  // f09_0d4a
        if (b < 0) return;
        if (listTop_ < 0x16) {
            listTop_ += 5;
            listState_ = 1;
            drawLogo(0x114, 0x156, 0x22B4);
        } else {
            listState_ = 2;
        }
    };
    arrow.onRelease = [this](int b) {  // f09_0d96
        if (b >= 0) drawLogo(0x114, 0x156, 0x22B5);
    };
    panels_.add(arrow);
    drawLogo(0x114, 0x156, 0x22B5);
    listState_ = 1;
    listTop_ = 0;
    select(1);
    do {
        panels_.poll(ctx_.platform);
        ctx_.pump();
        while (listState_ == 1) {
            listState_ = 0;
            copyArea(2, 1, 0xDC, 0x5C, 0xE0, 0x82);
            for (int i = 0; i < 5 && listTop_ + i < 0x1B; ++i)
                text(0xE6, i * 0x14 + 0x78, dataString(dataWord(static_cast<uint16_t>(0x1124 + 4 * (listTop_ + i)))), 0xCC);
            waitCountdown(1);
            drawLogo(0x114, 0x156, 0x22B5);
        }
    } while (listState_ != 2);
    listState_ = 0;
    panels_.clear();
    copyArea(2, 1, 0x114, 0x156, 0x66, 0x3A);
    select(2);
}

void Mystery::smittyShow() {
    // f09_002e: the slide screen plays a little film while Edison and
    // Smitty watch.
    static const uint8_t kFrames[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x80, 10, 11, 12, 0x80, 13, 14, 0x80, 13,
                                      15, 0x80, 13, 12, 11, 16, 17, 2, 0};
    select(1);
    drawOpaque(0x176, 0x12E, 0x21E5);
    drawLogo(0x6C, 0xF0, 0x22C0);
    drawLogo(0x1DA, 0xE8, 0x22BD);
    drawLogo(0x1E8, 0xE8, 0x22BF);
    for (uint8_t f : kFrames) {
        if (f == 0x80) {
            waitCountdown(4);
        } else {
            drawOpaque(0xB0, 4, static_cast<uint16_t>(0x21E6 + f));
            waitCountdown(1);
        }
    }
    drawOpaque(0x176, 0x12E, 0x21E4);
    drawLogo(0x6C, 0xF0, 0x22AB);
    drawLogo(0x1DA, 0xE8, 0x22AE);
    drawLogo(0x1FC, 0x158, 0x22BB);
    wantShow_ = false;
    sound(0x4036);
}

void Mystery::mapView() {
    // f09_01ee: the map of the Museum (1015) full screen, until a click, a
    // key or 40 s.
    const std::vector<uint8_t> pixels = ctx_.screens[1].pixels;
    const Palette palette = ctx_.displayPalette, palette1 = ctx_.screens[1].palette;
    ctx_.showFullScreen(0x1015, 3);
    show(3);
    clearInput();
    waitOrClick(400);
    clearInput();
    ctx_.screens[1].pixels = pixels;
    ctx_.screens[1].palette = palette1;
    ctx_.displayPalette = palette;
    wantMap_ = false;
}

bool Mystery::askQuit() {
    // f09_0592: "Do you want to quit this game?" with Yes and No.
    const int fh = font_->height();
    const int x = Screen::kWidth / 4, y = Screen::kHeight * 2 / 5;
    const int bubble = speechBox(x, y, dataLines(0x0E04), 1, true);
    sound(0x4050);
    smittySays(0x80, 0);
    modal_ = true;
    const int answer = yesNo(x + fh, y + fh + fh / 2, 0xA0, 0x1E);
    restoreArea(bubble);
    modal_ = false;
    helpPressed_ = false;
    return answer == 0;
}

void Mystery::quitPressed() {
    // f09_0680: quit, then "Do you want to play again?"; no leaves (the
    // original saves the game here).
    if (menu_ != 0) return;
    drawLogo(0x1FC, 0x158, 0x22BC);
    if (!askQuit()) {
        menu_ = 3;
        drawLogo(0x1FC, 0x158, 0x22BB);
        return;
    }
    const int fh = font_->height();
    const int x = Screen::kWidth / 4, y = Screen::kHeight * 2 / 5;
    const int bubble = speechBox(x, y, dataLines(0x0E34), 1, true);
    sound(0x404C);
    smittySays(0x80, 0);
    modal_ = true;
    const int answer = yesNo(x + fh, y + fh + fh / 2, 0xA0, 0x1E);
    restoreArea(bubble);
    modal_ = false;
    helpPressed_ = false;
    clearInput();
    if (answer == 0) {
        menu_ = 2;
        greeted_ = false;
        return;
    }
    speechBox(x, y, dataLines(0x0E60), 1, false);
    sound(0x4033);
    smittySays(0x80, 0);
    waitCountdown(2);
    menu_ = 1;
}

// --- help ---------------------------------------------------------------

void Mystery::helpPanel(int x, int y, int w, int h, int textColour, int fillColour) {
    // f05_0266: pressing Smitty (or the button) asks for help.
    if (textColour >= 0) {
        fill(x, y, w, h, static_cast<uint8_t>(fillColour));
        text(x + 4, y + 2, dataString(0xB1), textColour);
    }
    Panels::Panel panel;  // DS:0098
    panel.x = x, panel.y = y, panel.w = w, panel.h = h;
    panel.buttons = {{0, 0, 0, 0}};
    panel.onPress = [this](int) { helpPressed_ = true; };  // f05_0242
    panels_.add(panel);
    helpPressed_ = false;
}

void Mystery::help(uint16_t id) {
    // f05_0320: a text resource (a line count, then the lines).
    std::vector<uint8_t> raw;
    std::vector<std::string> lines;
    if (ctx_.read(id, raw) && !raw.empty()) {
        size_t at = 1;
        for (int i = 0; i < raw[0] && at < raw.size(); ++i) {
            std::string l;
            while (at < raw.size() && raw[at] && l.size() < 0x50) l += static_cast<char>(raw[at++]);
            ++at;
            lines.push_back(l);
        }
    }
    messageBox(lines);
    helpPressed_ = false;
}

void Mystery::messageBox(const std::vector<std::string>& text) {
    // f06_0f34 with f06_09ee's style 0: a bevelled box in the middle of
    // the screen, the lines centred and "< Press mouse/any key >" last;
    // it waits for a click or key.
    computeUiColours();
    const int previous = current();
    select(1);
    std::vector<std::string> lines(text.begin(), text.begin() + std::min<size_t>(text.size(), 0x1E));
    lines.push_back(dataString(0x103));
    int widest = 0;
    for (const auto& l : lines) widest = std::max(widest, font_->width(l));
    const int w = std::min(widest + 0x20, Screen::kWidth - 1);
    const int h = std::min(static_cast<int>(lines.size() + 1) * 0x10, Screen::kHeight - 1);
    const int x = (Screen::kWidth - w) >> 1, y = (Screen::kHeight - h) >> 1;
    const int saved = saveArea(x, y, w, h);
    select(3);
    copyArea(1, 3, x, y, w, h);
    fill(x, y, w, h, ui_[7]);
    for (int i = 0; i < 3; ++i) line(x, y + i, x + w - 1, y + i, ui_[5]);
    for (int i = 0; i < 3; ++i) line(x, y + h - 1 - i, x + w - 1, y + h - 1 - i, ui_[9]);
    for (int i = 0; i < 3; ++i) line(x + w - 1 - i, y + 1 + i, x + w - 1 - i, y + h - 2 - i, ui_[6]);
    for (int i = 0; i < 3; ++i) line(x + i, y + 1 + i, x + i, y + h - 1 - i, ui_[8]);
    for (size_t i = 0; i < lines.size(); ++i)
        this->text(((w - font_->width(lines[i])) >> 1) + x, static_cast<int>(i) * 0x10 + y + 8, lines[i],
                   i + 1 == lines.size() ? ui_[12] : ui_[6]);
    copyArea(3, 1, x, y, w, h);
    select(1);
    clearInput();
    while (!anyInput()) ctx_.pump();
    clearInput();
    restoreArea(saved);
    select(previous);
}

// --- the game -------------------------------------------------------------

void Mystery::newBoard() {
    // Each used square gets the next puzzle (and its difficulty) from the
    // level's list; each object slot in use gets a museum (all different)
    // and a random used square without an object.
    const int levelIndex = std::min<int>(player_.level, 7);
    const Level& level = kLevels[levelIndex];
    timeLeft_ = timeTotal_ = level.seconds;
    score_ = 0;
    size_t next = level.puzzles;
    for (int s = 0; s < 29; ++s) {
        Square& sq = squares_[s];
        if (customLevel_) {
            // A custom level keeps the squares it was given (f08_1eca).
            sq.object = 0xFF;
            continue;
        }
        if (level.squares[s] == '1' && next + 1 < puzzles_.size()) {
            sq.puzzle = puzzles_[next];
            sq.level = puzzles_[next + 1];
            sq.state = 1;
            next += 4;
        } else {
            sq.puzzle = sq.level = 0xFF;
            sq.state = 0;
        }
        sq.object = 0xFF;
    }
    objectCount_ = 0;
    bool usedMuseum[18] = {};
    for (int k = 0; k < 16; ++k) {
        Object& o = objects_[k];
        if (kObjectSlots[levelIndex][k] != '1') {
            o = Object{};
            continue;
        }
        int museum;
        do museum = random(18);
        while (usedMuseum[museum]);
        usedMuseum[museum] = true;
        int s;
        do s = random(29);
        while (squares_[s].state == 0 || squares_[s].object != 0xFF);
        squares_[s].object = static_cast<uint8_t>(k);
        o = Object{static_cast<uint8_t>(museum), static_cast<uint8_t>(s), 0};
        ++objectCount_;
    }
}

int Mystery::play() {
    bool first = true;  // [81CC] = -1 until the first game is played
    outcome_ = 0;
    greeted_ = false;
    if (!savedGame_) newBoard();
    int found = 0;
    constexpr int kSecondSlot = 5;
    for (;;) {
        // The map screen.
        clearInput();
        backdrop(0x1003);
        applyColours(2, false);
        select(2);
        drawOpaque(0x86, 0xBC, 0x22C2);
        drawOpaque(0x98, 0x8C, 0x22B9);
        if (player_.level < 2)
            hourStart_ = 0x1555, hourPeriod_ = 0x708;
        else if (player_.level < 5)
            hourStart_ = 0xEAAA, hourPeriod_ = 0xE10;
        else
            hourStart_ = 0xC000, hourPeriod_ = 0x1518;
        if (!first) drawObjects(found);
        drawLogo(0x1FC, 0x158, 0x22BB);
        drawOpaque(0x176, 0x12E, 0x21E4);
        select(1);
        show(2);
        clock(true);
        number(0x13A, 0x32, 0x38, 0x10, score_);
        computeUiColours();
        entrance(first, found);

        // The buttons.
        panels_.clear();
        Panels::Panel grid;  // DS:0D98: the objects; pressing one names it
        grid.x = kGridX, grid.y = kGridY, grid.w = 0xD8, grid.h = 0xB4;
        for (int k = 0; k < 16; ++k) grid.buttons.push_back({slotX(k), slotY(k), kSlotW, kSlotH});
        grid.onPress = [this](int b) {  // f09_0432
            if (b < 0 || modal_ || objects_[b].museum == 0xFF) return;
            const uint8_t m = objects_[b].museum;
            const std::vector<std::string> lines = {dataString(dataWord(static_cast<uint16_t>(0xC56 + 4 * m))),
                                                    dataString(dataWord(0xDB4))};
            bubble_ = speechBox(Screen::kWidth * 3 / 5, Screen::kHeight * 2 / 5, lines, 3, true);
            sound(static_cast<uint16_t>(0x401F + m));
            mapTalk(1, 2);
        };
        grid.onRelease = [this](int b) {  // f09_0540
            if (b < 0 || objects_[b].museum == 0xFF) return;
            restoreArea(bubble_);
            clearInput();
        };
        panels_.add(grid);
        Panels::Panel door;  // DS:0E76: go to the Museum
        door.x = 0xD2, door.y = 0x11C, door.w = 0x74, door.h = 0x32;
        door.buttons = {{0, 0, 0x74, 0x32}};
        door.onPress = [this](int b) { if (b >= 0) go_ = true; };  // f09_0822
        go_ = false;
        panels_.add(door);
        Panels::Panel map;  // DS:0CDE: the map of the Museum
        map.x = 0xC, map.y = 0x8C, map.w = 0x42, map.h = 0x52;
        map.buttons = {{0, 0, 0x42, 0x52}};
        map.onPress = [this](int b) { if (b >= 0) wantMap_ = true; };  // f09_01ca
        wantMap_ = false;
        panels_.add(map);
        Panels::Panel quit;  // DS:0DC6
        quit.x = 0x1FC, quit.y = 0x158, quit.w = 0x3C, quit.h = 0x16;
        quit.buttons = {{0, 0, 0x3C, 0x16}};
        quit.onPress = [this](int b) { if (b >= 0) quitPressed(); };  // f09_0680
        menu_ = 0;
        panels_.add(quit);
        Panels::Panel film;  // DS:0CBA
        film.x = 0x176, film.y = 0x12E, film.w = 0x2A, film.h = 0x12;
        film.buttons = {{0, 0, 0x2A, 0x12}};
        film.onPress = [this](int b) { if (b >= 0) wantShow_ = true; };  // f09_0000
        wantShow_ = false;
        panels_.add(film);
        Panels::Panel scores;  // DS:0E9A: high scores
        scores.x = 0x86, scores.y = 0xBC, scores.w = 0x4E, scores.h = 0x2C;
        scores.buttons = {{0, 0, 0x4E, 0x2C}};
        scores.onPress = [this](int b) {  // f09_0846
            if (b < 0 || modal_) return;
            drawOpaque(0x86, 0xBC, 0x22C3);
            showHighScores();
            redrawMap();
        };
        scores.onRelease = [this](int b) {  // f09_0896
            if (b >= 0 && !modal_) drawOpaque(0x86, 0xBC, 0x22C2);
        };
        panels_.add(scores);

        // What Edison says.
        const int bx = Screen::kWidth * 3 / 5, by = Screen::kHeight * 2 / 5;
        int bubble = 0;
        if (!first) {
            if (outcome_ == 2) {
                bubble = speechBox(bx, Screen::kHeight / 3, dataLines(0x139C), 3, true);
                sound(0x401C);
                mapTalk(1, 5);
                sound(0x401E);
                mapTalk(1, 5);
                waitCountdown(10);
            } else if (outcome_ == 1) {
                const int b = speechBox(bx, by, dataLines(0x13C8), 3, true);
                sound(0x4062);
                mapTalk(1, 5);
                waitOrClick(4);
                restoreArea(b);
                bubble = speechBox(bx, by, dataLines(0x13E2), 3, true);
                sound(0x4017);
                mapTalk(1, 2);
            } else if (found == 1) {
                const int m = objects_[squares_[square_].object].museum;
                const std::vector<std::string> lines = {dataString(dataWord(0x1470)),
                                                        dataString(dataWord(static_cast<uint16_t>(0xC56 + 4 * m)))};
                const int b = speechBox(bx, by, lines, 3, true);
                sound(0x4018);
                mapTalk(1, 5);
                sound(static_cast<uint16_t>(0x401F + m));
                mapTalk(1, 5);
                waitOrClick(4);
                restoreArea(b);
                if (lastOne_) {
                    bubble = speechBox(bx, by, dataLines(0x1404), 3, true);
                    sound(0x4014);
                } else {
                    bubble = speechBox(bx, by, dataLines(0x1426), 3, true);
                    sound(0x4015);
                }
                mapTalk(1, 3);
            } else {
                bubble = speechBox(bx, by, dataLines(0x144E), 3, true);
                sound(0x4013);
                mapTalk(1, 5);
            }
        } else {
            copyArea(2, 1, kGridX, kGridY, 0xD8, 0xB4);
            drawObjects(0);
            waitOrClick(10);
            bubble = speechBox(bx, by, dataLines(0x1346), 3, true);
            sound(0x4012);
            mapTalk(1, 5);
        }
        waitOrClick(2);
        if (bubble) restoreArea(bubble);
        helpPanel(0x88, 0x76, 0x54, 0x40);
        if (first) {
            idleMap();
            startIdleTimer();
        }
        ctx_.timer.setPeriodic(kSecondSlot, 1, [this] {  // f09_0a7a
            if (--timeLeft_ < 0) {
                timeLeft_ = 0;
                if (outcome_ == 0) outcome_ = 1;
            }
        });
        modal_ = false;
        clearInput();

        // Until Edison goes into the Museum, the game ends or the player
        // leaves.
        int result = 0;
        for (;;) {
            if (menu_ == 1 || menu_ == 2) {
                result = menu_;
                break;
            }
            menu_ = 0;
            panels_.poll(ctx_.platform);
            ctx_.pump();
            clock(false);
            if (wantMap_) mapView();
            if (go_) break;
            if (outcome_ != 0) {
                endOfGame();
                break;
            }
            if (idle_) {
                idle_ = false;
                idleMap();
            }
            if (wantShow_) smittyShow();
            if (helpPressed_) {
                drawOpaque(0x98, 0x8C, 0x22BA);
                help(0x3F06);
                drawOpaque(0x98, 0x8C, 0x22B9);
            }
        }
        if (!go_ || outcome_ != 0) {
            // Leaving mid-game saves it ("I'll save this game.").
            if (result == 1 && outcome_ == 0) saveGame();
            ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);
            ctx_.timer.setPeriodic(kIdleSlot, 0, nullptr);
            panels_.clear();
            clearInput();
            return result;
        }
        // Into the Museum: its floor until a game finds an object or
        // Edison comes back.
        do {
            clearInput();
            fill(0, 0, Screen::kWidth, Screen::kHeight, 0);
            first = false;
            found = floor();
        } while (found != 1 && found != -1);
    }
}

// --- Smitty's idle moments ------------------------------------------------

void Mystery::startIdleTimer() {
    // f09_1dd8 on the first visit to the map: [93B0] = random(300) and
    // g05_03de each second. (It picks random(180) for the next wait and
    // then uses 120.)
    idleCountdown_ = random(300);
    ctx_.timer.setPeriodic(kIdleSlot, 1, [this] {
        if (idleCountdown_-- == 0) {
            idleCountdown_ = 0x78;
            idle_ = true;
        }
    });
}

void Mystery::idleMap() {
    // f06_21c6: frames 234E-2350 at (190, 164), the middle one longer.
    const int previous = current();
    select(1);
    const int under = saveArea(0x190, 0x164, 0x28, 0x28);
    for (int f : {0, 1, 2, 0}) {
        drawLogo(0x190, 0x164, static_cast<uint16_t>(0x234E + f));
        waitCountdown(f == 2 ? 4 : 2);
    }
    restoreArea(under);
    select(previous);
}

void Mystery::idleHint(int kind) {
    if (kind == 3) {
        // g30_127c: frames 2292-2294 centred at (80, F4), with a pause.
        if (!idle_) return;
        const int previous = current();
        select(1);
        const int under = saveArea(0x6C, 0xE0, 0x28, 0x28);
        for (int f : {0, 1, 2, 0x80, 0}) {
            if (f != 0x80) drawCentred(0x80, 0xF4, static_cast<uint16_t>(0x2292 + f));
            waitCountdown(f == 0x80 ? 4 : 2);
        }
        restoreArea(under);
        select(previous);
        idle_ = false;
        return;
    }
    // The frame lists (DS:1F12, 2914, 3106): 0x80 is a pause, -1 the end.
    static const int kPicture[] = {0, 1, 2, 0x80, 3, 2, 0x80, 3, 2, 1, 0, -1};
    static const int kCards[] = {0, 1, 2, 0x80, 0, -1};
    static const int kCircuit[] = {0, 1, 2, 3, 2, 3, 2, 3, 0, -1};
    const int* frames = kind == 0 ? kPicture : kind == 1 ? kCards : kCircuit;
    if (idle_) {
        idle_ = false;
        hinting_ = true;
        hintStep_ = 0;
        if (kind == 2) {
            // g16_07e6: what's under the corner, kept at (5A, 0) on 2.
            hintUnder_ = saveArea(0, 0x8A, 0x5A, 0x6A);
            duplicateArea(1, 2, 0, 0x8A, 0x5A, 0x6A, 0x5A, 0);
        }
        ctx_.countdown[1] = 0;
    }
    if (!hinting_ || ctx_.countdown[1] > 0) return;
    const int previous = current();
    select(1);
    if (kind == 0) {  // g12_1bec
        copyArea(2, 1, 0x23C, 0x15E, 0x3E, 0x32);
        drawLogo(0x23C, 0x15E, static_cast<uint16_t>(0x228E + frames[hintStep_]));
    } else if (kind == 1) {  // g15_10c2
        drawOpaque(0x264, 0x132, static_cast<uint16_t>(0x2287 + frames[hintStep_]));
    } else {  // g16_0830
        duplicateArea(2, 1, 0x5A, 0, 0x5A, 0x6A, 0, 0x8A);
        drawLogo(0, 0x8A, static_cast<uint16_t>(0x228A + frames[hintStep_]));
    }
    ++hintStep_;
    if (frames[hintStep_] == -1) {
        hinting_ = false;
        if (kind == 0) copyArea(2, 1, 0x23C, 0x15E, 0x3E, 0x32);
        if (kind == 1) copyArea(2, 1, 0x264, 0x132, 0x1A, 0x24);
        if (kind == 2) restoreArea(hintUnder_);
    }
    select(previous);
    ctx_.countdown[1] = 2;
    if (kind != 2 && frames[hintStep_] == 0x80) {
        ctx_.countdown[1] = kind == 0 ? 4 : 5;
        ++hintStep_;
    }
}

void Mystery::redrawMap() {
    // g09_1c92: the map again (after the high scores). Screen 2 gets the
    // map without Smitty and Edison, for their animations.
    ctx_.showFullScreen(0x1003, 2);
    applyColours(2, false);
    select(2);
    drawObjects(0);
    clock(true);
    const int smitty = saveArea(0, 0x4A, 0xA8, 0x146);
    drawLogo(0, 0x4A, 0x22AD);
    drawLogo(0x6C, 0xF0, 0x22AB);
    const int edison = saveArea(0x1DA, 0xE8, 0xA6, 0xAC);
    drawLogo(0x1DA, 0xE8, 0x22AE);
    drawLogo(0x1FC, 0x158, 0x22BB);
    drawOpaque(0x176, 0x12E, 0x21E4);
    number(0x13A, 0x32, 0x38, 0x10, score_);
    drawOpaque(0x86, 0xBC, 0x22C2);
    select(1);
    show(2);
    select(2);
    restoreArea(smitty);
    restoreArea(edison);
    select(1);
}

}  // namespace edison
