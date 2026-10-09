// MALL.EXE segment 10: the Museum floor, where Edison picks a square and
// plays its game (f10_0708).

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "mystery/mystery.h"

namespace edison {

namespace {

// f10_0328: the squares that are Museum buildings, with where their
// pictures (2020+i, and the id masks 22C4+i) go.
constexpr int kRooms = 14;
constexpr int kRoomSquare[kRooms] = {0, 2, 4, 6, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x1B, 0x1C};
constexpr int kRoomX[kRooms] = {0x49, 0xB3, 0x122, 0x1D9, 0x93, 0x37, 0x99, 0xCD, 0x10A, 0x169, 0x1B6, 0x1F3, 0x93, 0xF0};
constexpr int kRoomY[kRooms] = {0x2F, 0x2B, 0x37, 0x3E, 0xC0, 0x105, 0x110, 0x110, 0xFA, 0x10E, 0xFA, 0x130, 0x69, 0x64};

// On the floor's screen 2, square k is painted in colour A1 + k.
constexpr int kFirstSquareColour = 0xA1;

}  // namespace

void Mystery::squareName(int square) {
    // f10_0000: the square's name (DS:15D6) in the bar at the top.
    copyArea(2, 1, 0, 0, Screen::kWidth, 0x14);
    const std::string name = dataString(dataWord(static_cast<uint16_t>(0x15D6 + 4 * square)));
    if (name.empty()) return;
    const int previous = current();
    select(1);
    text(4, 2, name, 0x9B);
    select(previous);
}

void Mystery::floorRegions() {
    // f10_008e: the level's paths (polygons at DS:164A, coloured per level
    // from DS:17D0) are painted in their squares' colours, and the solved
    // (202F) or failed (2031) ones marked. Square 0 is never marked (the
    // original tests > 0).
    const uint16_t colours = static_cast<uint16_t>(0x17D0 + std::min<int>(player_.level, 7) * 0x1E);
    for (int i = 0; i < 15; ++i) {
        const int colour = dataWord(static_cast<uint16_t>(colours + 2 * i));
        if (!colour) continue;
        const uint16_t poly = static_cast<uint16_t>(0x164A + i * 0x1A);
        const int n = dataWord(poly);
        std::vector<std::pair<int, int>> pts;
        for (int k = 0; k < n; ++k)
            pts.emplace_back(static_cast<int16_t>(dataWord(static_cast<uint16_t>(poly + 2 + 4 * k))),
                             static_cast<int16_t>(dataWord(static_cast<uint16_t>(poly + 4 + 4 * k))));
        fillPolygonSolid(pts, static_cast<uint8_t>(colour));  // f32_2e46: draw_poly, the library's spans
    }
    int previous = -1;
    for (int i = 0; i < 15; ++i) {
        const int colour = dataWord(static_cast<uint16_t>(colours + 2 * i));
        if (colour == previous) continue;
        previous = colour;
        const int square = colour - kFirstSquareColour;
        if (square <= 0 || square >= 29) continue;
        const uint8_t state = squares_[square].state;
        if (state != 2 && state != 3) continue;
        const uint16_t poly = static_cast<uint16_t>(0x164A + i * 0x1A);
        const int n = dataWord(poly);
        const int x = (static_cast<int16_t>(dataWord(poly + 2)) +
                       static_cast<int16_t>(dataWord(static_cast<uint16_t>(poly + 2 + 4 * (n - 2))))) >> 1;
        const int y = (static_cast<int16_t>(dataWord(poly + 4)) +
                       static_cast<int16_t>(dataWord(static_cast<uint16_t>(poly + 4 + 4 * (n - 2))))) >> 1;
        const uint16_t mark = state == 2 ? 0x202F : 0x2031;
        const Bitmap& b = ctx_.bitmap(mark);
        drawLogo(x - b.width / 2, y - b.height / 2, mark);  // f06_189a, centred
    }
}

void Mystery::floorRooms(bool masks) {
    // f10_0328: the used buildings' pictures, marked solved (202E) or
    // failed (2030); or (masks) their id masks, drawn on screen 2 so that
    // its colour under the mouse tells the square.
    const int previous = current();
    if (masks) select(2);
    for (int i = 0; i < kRooms; ++i) {
        const Square& sq = squares_[kRoomSquare[i]];
        if (sq.state == 0) continue;
        if (masks) {
            drawLogo(kRoomX[i], kRoomY[i], static_cast<uint16_t>(0x22C4 + i));
            continue;
        }
        const uint16_t picture = static_cast<uint16_t>(0x2020 + i);
        drawLogo(kRoomX[i], kRoomY[i], picture);
        if (sq.state == 2 || sq.state == 3) {
            const Bitmap& p = ctx_.bitmap(picture);
            const uint16_t mark = sq.state == 2 ? 0x202E : 0x2030;
            const Bitmap& b = ctx_.bitmap(mark);
            drawLogo(kRoomX[i] + p.width / 2 - b.width / 2, kRoomY[i] + p.height / 2 - b.height / 2, mark);
        }
    }
    if (masks) select(previous == 2 ? 1 : previous);
}

void Mystery::director() {
    // f10_0638: the Director waves from his window.
    static const uint8_t kFrames[] = {0, 1, 2, 3, 4, 5, 4, 5, 4, 5, 4, 5, 6, 7, 8, 0};
    select(1);
    music(0x41);
    for (uint8_t f : kFrames) {
        drawOpaque(0x178, 0x13A, static_cast<uint16_t>(0x225E + f));
        waitCountdown(1);
    }
    floorDirector_ = false;
}

bool Mystery::puzzle(int kind, int level) {
    switch (kind) {
    case 0: return foldedCube(level);
    case 1: return planetarium(level);
    case 2: return ballSculpture(level);
    case 3: return binaryLights(level);
    case 4: return questionPeriod(level, true);
    case 5: return droppingSquares(level);
    case 6: return codes(level);
    case 7: return concentration(level);
    case 8: return circuitAnalyzer(level);
    case 9: return stackup(level);
    case 10: return slidePuzzle(level);
    case 11: return colourTransformation(level);
    case 12: return switchPuzzle(level);
    case 13: return arrowPuzzle(level);
    case 14: return whatComesNext(level);
    case 15: return dig(level);
    default: return true;  // no such game (the original's switch does nothing)
    }
}

int Mystery::floor() {
    // f10_0708. Returns 1 when a game found a new object, 2 when its object
    // was already found, 0 otherwise (the floor comes back), -1 when Edison
    // goes back to the map (or time is up).
    ctx_.blackout();
    ctx_.showFullScreen(0x1001, 2);
    select(2);
    drawOpaque(0x90, 0x15E, 0x21CA);
    drawOpaque(0x3E, 0x13C, 0x21CC);
    drawOpaque(0x90, 0x13C, 0x21CE);
    floorRegions();
    floorRooms(false);
    number(0x96, 0x14C, 0x40, 0xE, score_);
    show(2);
    select(1);
    floorRooms(true);
    panels_.clear();
    Panels::Panel back;  // DS:18CA
    back.x = 0x3E, back.y = 0x13C, back.w = 0x4E, back.h = 0x4A;
    back.buttons = {{0, 0, 0x4E, 0x4A}};
    back.onPress = [this](int b) { if (b >= 0) floorBack_ = true; };  // f10_05dc
    floorBack_ = false;
    panels_.add(back);
    Panels::Panel window;  // DS:18EE
    window.x = 0x178, window.y = 0x13A, window.w = 0x82, window.h = 0x56;
    window.buttons = {{0, 0, 0x82, 0x56}};
    window.onPress = [this](int b) { if (b >= 0) floorDirector_ = true; };  // f10_060a
    floorDirector_ = false;
    panels_.add(window);
    helpPanel(0x90, 0x15E, 0x4C, 0x28);
    clearInput();

    auto squareAt = [this](int x, int y) {
        if (x < 0 || y < 0 || x >= Screen::kWidth || y >= Screen::kHeight) return -1;
        const int square = ctx_.screens[2].pixels[static_cast<size_t>(y) * Screen::kWidth + x] - kFirstSquareColour;
        return square >= 0 && square < 29 ? square : -1;
    };
    int named = -1;
    int result = 0;
    bool skipGame = false;  // [bp-0Ch]: F typed, the next square's game counts as won unplayed
    for (;;) {
        if (ctx_.platform.takeKeyIf('f') || ctx_.platform.takeKeyIf('F')) skipGame = true;  // 10:080e
        if (floorBack_) {
            drawOpaque(0x3E, 0x13C, 0x21CD);
            result = -1;
            waitCountdown(4);
            break;
        }
        if (outcome_ == 1) {
            result = -1;
            waitCountdown(2);
            break;
        }
        int cx = -1, cy = -1;
        const bool clicked = panels_.poll(ctx_.platform, &cx, &cy);
        ctx_.pump();
        if (helpPressed_) {
            drawOpaque(0x90, 0x15E, 0x21CB);
            help(0x3F07);
            drawOpaque(0x90, 0x15E, 0x21CA);
        }
        if (floorDirector_) director();
        int mx, my;
        bool down;
        ctx_.platform.mouse(&mx, &my, &down);
        const int hover = squareAt(mx, my);
        if (hover < 0) {
            if (named >= 0) copyArea(2, 1, 0, 0, Screen::kWidth, 0x14);
            named = -1;
        } else if (hover != named) {
            squareName(hover);
            named = hover;
        }
        if (!clicked) continue;
        square_ = squareAt(cx, cy);
        if (square_ < 0) continue;
        Square& sq = squares_[square_];
        int game = sq.puzzle, level = sq.level;
        // For testing, as MALLSKIP.EXE's --puzzle and --difficulty:
        // EDISON_SQUARE=P or P,D plays puzzle P (at difficulty D) on every square.
        if (const char* forced = std::getenv("EDISON_SQUARE")) {
            int p = -1, d = -1;
            if (std::sscanf(forced, "%d,%d", &p, &d) >= 1 && p >= 0) game = p;
            if (d >= 0) level = d;
        }
        const bool won = skipGame || (game < 16 && puzzle(game, level));  // 10:0993
        if (won) {
            if (sq.state != 0) sq.state = 2;
            if (sq.object != 0xFF && objects_[sq.object].museum != 0xFF) {
                result = objects_[sq.object].found ? 2 : 1;
                objects_[sq.object].found = 1;
            }
        } else if (sq.state == 1) {
            sq.state = 3;
        }
        break;
    }
    panels_.clear();
    clearInput();
    return result;
}

}  // namespace edison
