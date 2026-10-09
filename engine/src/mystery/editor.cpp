// MALL.EXE segment 11: the custom level editor (f11_19b4). The player picks
// one of the eight maps (the levels' sets of squares), then puts a game and
// its difficulty on each square of a small copy of the Museum floor.

#include <algorithm>
#include <string>

#include "mystery/mystery.h"

namespace edison {

namespace {

// f11_098c: the floor's buildings (as f10_0328), drawn scaled by BE/100 x
// C1/100 into the view at (52, 2C).
constexpr int kRooms = 14;
constexpr int kRoomSquare[kRooms] = {0, 2, 4, 6, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x1B, 0x1C};
constexpr int kRoomX[kRooms] = {0x49, 0xB3, 0x122, 0x1D9, 0x93, 0x37, 0x99, 0xCD, 0x10A, 0x169, 0x1B6, 0x1F3, 0x93, 0xF0};
constexpr int kRoomY[kRooms] = {0x2F, 0x2B, 0x37, 0x3E, 0xC0, 0x105, 0x110, 0x110, 0xFA, 0x10E, 0xFA, 0x130, 0x69, 0x64};
constexpr int kScaleX = 0xBE, kScaleY = 0xC1, kViewX = 0x52, kViewY = 0x2C, kViewW = 0x1DE, kViewH = 0x110;
int toViewX(int x) { return static_cast<int>((static_cast<long>(x) * kScaleX) >> 8) + kViewX; }
int toViewY(int y) { return static_cast<int>((static_cast<long>(y) * kScaleY) >> 8) + kViewY; }

// The message area under the map, and its text colour.
constexpr int kTextX = 0x76, kTextY = 0x152, kTextW = 0x194, kTextH = 0x2E;
constexpr int kInk = 0x5B;
// The games' icons (203E + game) and, in the editor's panel DS:1D1C,
// where they sit around the map.
constexpr int kIconW = 0x44, kIconH = 0x2A;
constexpr int kIconX[16] = {8, 8, 8, 8, 8, 0x60, 0xAC, 0xF8, 0x144, 0x190, 0x1DC, 0x236, 0x236, 0x236, 0x236, 0x236};
constexpr int kIconY[16] = {0xC8, 0x96, 0x64, 0x32, 2, 2, 2, 2, 2, 2, 2, 2, 0x32, 0x64, 0x96, 0xC8};
// The Ready box (panel DS:1D40).
constexpr int kReadyX = 0x212, kReadyY = 0x156, kReadyW = 0x3A, kReadyH = 0x28;
// The square with colour A1 + k on the masks.
constexpr int kFirstSquareColour = 0xA1;

}  // namespace

void Mystery::editorView(int mode) {
    // f11_098c: mode 1 the whole floor (backdrop 100D, buildings and
    // paths), 2 the buildings again, 3 the buildings' id masks (for the
    // square under the mouse). All on screen 2; mode 1 copies it to 1.
    select(2);
    if (mode == 1) ctx_.showFullScreen(0x100D, 2);
    const uint16_t first = mode == 3 ? 0x22C4 : 0x2020;
    for (int i = 0; i < kRooms; ++i) {
        if (squares_[kRoomSquare[i]].state == 0) continue;
        const uint16_t id = static_cast<uint16_t>(first + i);
        const Bitmap& b = ctx_.bitmap(id);
        drawScaledCentred(toViewX((b.width >> 1) + kRoomX[i]), toViewY((b.height >> 1) + kRoomY[i]), kScaleX, kScaleY, id);
    }
    if (mode == 1) {
        const uint16_t colours = static_cast<uint16_t>(0x17D0 + editorMap_ * 0x1E);
        for (int i = 0; i < 15; ++i) {
            const int colour = dataWord(static_cast<uint16_t>(colours + 2 * i));
            if (!colour) continue;
            const uint16_t poly = static_cast<uint16_t>(0x164A + i * 0x1A);
            std::vector<std::pair<int, int>> pts;
            for (int k = 0; k < static_cast<int>(dataWord(poly)); ++k)
                pts.emplace_back(toViewX(static_cast<int16_t>(dataWord(static_cast<uint16_t>(poly + 2 + 4 * k)))),
                                 toViewY(static_cast<int16_t>(dataWord(static_cast<uint16_t>(poly + 4 + 4 * k)))));
            fillPolygonSolid(pts, static_cast<uint8_t>(colour));  // f32_2e46: draw_poly
        }
        copyArea(2, 1, kViewX, kViewY, kViewW, kViewH);
    }
    select(1);
}

void Mystery::editorLabels() {
    // f11_18b8: each square's game number (1-16).
    for (int s = 0; s < 29; ++s) {
        if (squares_[s].state == 0 || squares_[s].puzzle == 0xFF) continue;
        text(dataWord(static_cast<uint16_t>(0x1908 + 4 * s)), dataWord(static_cast<uint16_t>(0x190A + 4 * s)),
             std::to_string(squares_[s].puzzle + 1), 0);
    }
}

void Mystery::editorButton(int x, int y, int w, int h, uint8_t colour) {
    // A bevelled box (light top and left, dark bottom and right), in MALL's
    // line (f06_1af8: on the display each far end stays off).
    fill(x, y, w, h, colour);
    displayLine(x, y, x + w, y, 0xF8);
    displayLine(x, y + h, x + w, y + h, 0xFE);
    displayLine(x, y, x, y + h, 0xF8);
    displayLine(x + w, y, x + w, y + h, 0xFE);
}

void Mystery::editorMessage(const std::string& first, const std::string& second, int secondY) {
    fill(kTextX, kTextY, kTextW, kTextH, 0);
    text(0x78, kTextY, first, kInk);
    if (!second.empty()) text(0x78, secondY, second, kInk);
}

void Mystery::editorGames() {
    // f11_14aa: the game icons around the map, the hint, and Ready.
    for (int i = 0; i < 16; ++i) drawOpaque(kIconX[i], kIconY[i], static_cast<uint16_t>(0x203E + i));
    if (!editing_)
        editorMessage(dataString(0x1D59), dataString(0x1D97), 0x162);
    else
        editorMessage(dataString(0x1DD7));
    editorPanels(1);
    editorButton(kReadyX, kReadyY, kReadyW, kReadyH, 0);
    text(kReadyX + 6, kReadyY + 4, dataString(0x1DFB), kInk);
    text(kReadyX + 6, kReadyY + 0x14, dataString(0x1E01), kInk);
}

void Mystery::editorHold(int game) {
    // The icon follows the mouse (at 9448/944A), kept on the screen.
    int mx, my;
    bool down;
    ctx_.platform.mouse(&mx, &my, &down);
    heldX_ = std::clamp(mx, 0, Screen::kWidth - kIconW - 1);
    heldY_ = std::clamp(my, 0, Screen::kHeight - kIconH - 1);
    heldUnder_ = saveArea(heldX_, heldY_, kIconW, kIconH);
    drawOpaque(heldX_, heldY_, static_cast<uint16_t>(0x203E + game));
}

void Mystery::editorLevels() {
    // g11_0e50: the difficulty buttons 1-N for the game just placed (a
    // game with one level gets it at once).
    const int levels = data_[0x197C + editorGame_];
    if (levels == 1) {
        squares_[editorSquare_].level = 0;
        editorState_ = 1;
        return;
    }
    editorMessage(dataString(0x1BFF));
    editorButtons_.clear();
    for (int i = 0; i < levels; ++i) {
        const int x = i * 0x28 + 0x7C;
        editorButton(x, 0x168, 0x1E, 0x14, 0x9A);
        text(i < 9 ? i * 0x28 + 0x86 : i * 0x28 + 0x84, 0x16A, std::to_string(i + 1), 0);
        editorButtons_.push_back({x - kTextX, 0x168 - kTextY, 0x1E, 0x14});
    }
    pendingPanels_ = true;
}

void Mystery::editorPlace() {
    // g11_104c: a click on the map. The id masks tell the square; holding
    // a game puts it there (then its difficulty), otherwise the square's
    // game and level are shown.
    int mx, my;
    bool down;
    ctx_.platform.mouse(&mx, &my, &down);
    editorView(3);
    const int square = ctx_.screens[2].pixels[static_cast<size_t>(my) * Screen::kWidth + mx] - kFirstSquareColour;
    if (square < 0 || square >= 29) return;
    editorSquare_ = square;
    Square& sq = squares_[square];
    const int lx = dataWord(static_cast<uint16_t>(0x1908 + 4 * square));
    const int ly = dataWord(static_cast<uint16_t>(0x190A + 4 * square));
    if (editorState_ == 2) {
        if (sq.state == 0) return;  // the original stops ("CUSTOM: NOT A VALID BOARD SQUARE")
        if (sq.puzzle != 0xFF) {
            editorView(2);
            copyArea(2, 1, lx, ly, sq.puzzle < 10 ? 0x12 : 0x24, 0x10);
        }
        restoreArea(heldUnder_);
        sq.puzzle = static_cast<uint8_t>(editorGame_);
        text(lx, ly, std::to_string(editorGame_ + 1), 0);
        editorState_ = 3;
        editorLevels();
    } else if (editorState_ == 1) {
        if (sq.puzzle == 0xFF) {
            editorMessage(dataString(0x1C45));
            return;
        }
        editorMessage(dataString(dataWord(static_cast<uint16_t>(0x1A9C + 4 * sq.puzzle))));
        text(0x78, 0x162, dataString(0x1C6B), kInk);
        text(0xB4, 0x162, std::to_string(sq.level + 1), kInk);
    }
}

void Mystery::editorPanels(int stage) {
    // The editor's panels: stage 0 the map icons (DS:1BAA), 1 the games
    // and the map (DS:1D1C), with the buttons under the map (DS:1B40) and
    // Ready (DS:1D40).
    panels_.clear();
    if (stage == 0) {
        Panels::Panel maps;  // DS:1BAA
        maps.x = 0, maps.y = 0, maps.w = 0x280, maps.h = 0x2E;
        for (int i = 0; i < 8; ++i) maps.buttons.push_back({0xC + i * 0x4E, 0, 0x46, 0x2C});
        maps.onPress = [this](int b) { editorPickMap(b); };  // g11_07e4
        panels_.add(maps);
    } else {
        Panels::Panel games;  // DS:1D1C: the icons, then the map (button 16)
        games.x = 0, games.y = 0, games.w = 0x280, games.h = 0x140;
        for (int i = 0; i < 16; ++i) games.buttons.push_back({kIconX[i], kIconY[i], kIconW, kIconH});
        games.buttons.push_back({kViewX, kViewY, kViewW, kViewH});
        games.onPress = [this](int b) {  // g11_1320
            if (b < 0 || editorState_ >= 3) return;
            if (b == 16) {
                editorPlace();
                return;
            }
            if (editorState_ == 2) restoreArea(heldUnder_);
            editorGame_ = b;
            editorMessage(dataString(dataWord(static_cast<uint16_t>(0x1A9C + 4 * b))));
            editorHold(b);
            editorState_ = 2;
        };
        panels_.add(games);
        Panels::Panel ready;  // DS:1D40
        ready.x = kReadyX, ready.y = kReadyY, ready.w = kReadyW, ready.h = kReadyH;
        ready.buttons = {{0, 0, kReadyW, kReadyH}};
        ready.onPress = [this](int b) {  // g11_1448
            if (b < 0 || editorState_ >= 3) return;
            if (editorState_ == 2) restoreArea(heldUnder_);
            editorEvent_ = 3;
            recolour(kReadyX, kReadyY, kReadyW, kReadyH, 0, 0x9A);
        };
        panels_.add(ready);
    }
    if (stage == 0 && editorMap_ < 0) return;
    Panels::Panel below;  // DS:1B40: OK, the difficulties, or Defaults/Continue
    below.x = kTextX, below.y = kTextY, below.w = kTextW, below.h = kTextH;
    below.buttons = editorButtons_;
    below.onPress = [this](int b) { editorBelow(b); };  // g11_0116
    panels_.add(below);
}

void Mystery::editorPickMap(int b) {
    // g11_07e4: a map icon. The first time, "If this is the map you
    // want, click on the okay button."
    if (b < 0 || editorState_ != 0 || b == editorMap_) return;
    if (editorMap_ >= 0) {
        drawOpaque(0xC + editorMap_ * 0x4E, 0, static_cast<uint16_t>(0x205D + editorMap_));
    } else {
        editorMessage(dataString(0x1BC3));
        editorButton(0x12C, 0x168, 0x28, 0x14, 0x9A);
        text(0x134, 0x16A, dataString(0x1BFA), 0);
        editorButtons_ = {{0x12C - kTextX, 0x168 - kTextY, 0x28, 0x14}};
    }
    editorMap_ = b;
    // g11_02a6: the map's squares, empty.
    const char* used = levelSquares(b);
    for (int s = 0; s < 29; ++s) squares_[s] = Square{0xFF, 0, 0xFF, static_cast<uint8_t>(used[s] == '1' ? 1 : 0)};
    drawOpaque(0xC + b * 0x4E, 0, static_cast<uint16_t>(0x2065 + b));
    editorEvent_ = 1;
    pendingPanels_ = true;
}

void Mystery::editorBelow(int b) {
    // g11_0116: the buttons under the map.
    if (b < 0) return;
    if (editorState_ == 0) {
        if (b > 0) return;
        editorState_ = 1;
        editorEvent_ = 2;
        fill(kTextX, kTextY, kTextW, kTextH, 0);
        fill(0, 0, 0x280, 0x2C, 0);
    } else if (editorState_ == 3) {
        if (b >= data_[0x197C + editorGame_]) return;
        Square& sq = squares_[editorSquare_];
        recolour(sq.level * 0x28 + 0x7C, 0x168, 0x1E, 0x14, 0x99, 0x9A);
        sq.level = static_cast<uint8_t>(b);
        recolour(b * 0x28 + 0x7C, 0x168, 0x1E, 0x14, 0x9A, 0x99);
        waitCountdown(4);
        fill(kTextX, kTextY, kTextW, kTextH, 0);
        editorState_ = 1;
    } else if (editorState_ == 4) {
        if (b > 1) return;
        if (b == 0) {
            // g11_0000: the level's own games on the squares left empty.
            size_t next = static_cast<size_t>(levelPuzzles(editorMap_));
            for (Square& sq : squares_) {
                if (sq.state == 0) continue;
                if (sq.puzzle == 0xFF && next + 1 < puzzles_.size()) {
                    sq.puzzle = puzzles_[next];
                    sq.level = puzzles_[next + 1];
                }
                next += 4;
            }
            editorEvent_ = 4;
        } else {
            fill(kTextX, kTextY, kTextW, kTextH, 0);
            recolour(kReadyX, kReadyY, kReadyW, kReadyH, 0x9A, 0);
            editorState_ = 1;
        }
    }
}

void Mystery::editorCheck() {
    // f11_165a: Ready. Every used square needs a game, or "Defaults" /
    // "Continue".
    bool full = true;
    for (const Square& sq : squares_)
        if (sq.state != 0 && sq.puzzle == 0xFF) full = false;
    if (full) {
        editorEvent_ = 4;
        return;
    }
    editorMessage(dataString(0x1E07), dataString(0x1E3D), 0x160);
    editorButton(0x8C, 0x16E, 0x5E, 0x12, 0x9A);
    text(0x98, 0x170, dataString(0x1E78), 0);
    editorButton(0x190, 0x16E, 0x5E, 0x12, 0x9A);
    text(0x19C, 0x170, dataString(0x1E81), 0);
    editorButtons_ = {{0x8C - kTextX, 0x16E - kTextY, 0x5E, 0x12}, {0x190 - kTextX, 0x16E - kTextY, 0x5E, 0x12}};
    editorState_ = 4;
    pendingPanels_ = true;
}

void Mystery::customLevelEditor(bool edit) {
    // f11_19b4: in its own font (resource 101). Editing starts from the
    // player's custom level (already on the board); a new one from a map.
    const Font* font = font_;
    font_ = &ctx_.font(0x101);
    clearInput();
    fill(0, 0, Screen::kWidth, Screen::kHeight, 0);
    ctx_.showFullScreen(0x100D, 2);
    editing_ = edit;
    select(2);
    editorGame_ = -1;
    editorSquare_ = -1;
    editorButtons_.clear();
    editorEvent_ = 0;
    if (!edit) {
        editorState_ = 0;
        editorMap_ = -1;
        for (int i = 0; i < 8; ++i) drawOpaque(0xC + i * 0x4E, 0, static_cast<uint16_t>(0x205D + i));
        editorMessage(dataString(0x1E8F), dataString(0x1ECE), 0x162);
        editorPanels(0);
    } else {
        editorState_ = 1;
        editorMap_ = player_.customLevel;
        editorGames();
    }
    select(1);
    // (Not f04_005c: no stir, and the picture's own colours 0 and FF, not
    // black and white: its palette straight into the DIBs' colour tables,
    // 11:1a55, and 1-FE onto the display, 11:1b86.)
    ctx_.displayPalette = ctx_.screens[2].palette;
    ctx_.screens[1].palette = ctx_.screens[2].palette;
    ctx_.screens.copyAll(2, 1);
    if (edit) {
        editorView(1);
        editorLabels();
    }
    pendingPanels_ = false;
    for (;;) {
        panels_.poll(ctx_.platform);
        ctx_.pump();
        if (editorEvent_ != 0) {
            const int event = editorEvent_;
            editorEvent_ = 0;
            if (event == 1 && editorState_ == 0) editorView(1);
            if (event == 2) editorGames();
            if (event == 3) editorCheck();
            if (event == 4) break;
        }
        if (pendingPanels_) {
            // Panels change between polls (the original edits their
            // tables in place).
            pendingPanels_ = false;
            editorPanels(editorState_ == 0 ? 0 : 1);
        }
        if (editorState_ == 2) {
            int mx, my;
            bool down;
            ctx_.platform.mouse(&mx, &my, &down);
            const int x = std::clamp(mx, 0, Screen::kWidth - kIconW - 1);
            const int y = std::clamp(my, 0, Screen::kHeight - kIconH - 1);
            if (x != heldX_ || y != heldY_) {
                restoreArea(heldUnder_);
                editorHold(editorGame_);
            }
        }
    }
    customLevel_ = true;
    player_.level = static_cast<uint8_t>(editorMap_);
    storeCustomLevel();
    panels_.clear();
    clearInput();
    fill(0, 0, Screen::kWidth, Screen::kHeight, 0);
    font_ = font;
}

}  // namespace edison
