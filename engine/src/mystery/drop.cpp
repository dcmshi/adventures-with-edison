// Dropping Squares (segment 18, the original's column.c): columns of three
// squares drop into a well; three or more of a kind in a line (any way)
// vanish. The squares of cloth that vanish go back into the fabric
// pictures round the well; fill them in to win.

#include <algorithm>
#include <random>

#include "mystery/mystery.h"

namespace edison {

namespace {

int random(int n) {
    static std::mt19937 rng{std::random_device{}()};
    return n > 0 ? std::uniform_int_distribution<int>(0, n - 1)(rng) : 0;
}

constexpr int kSecondSlot = 6;
constexpr int kCols = 8, kRows = 15;          // [856E], [8570]
constexpr int kCellW = 0x19, kCellH = 0x14;   // [8572], [8574]
constexpr int kWellX = 0xE5, kWellW = 200, kWellH = 300;  // [8576], [8578]
constexpr int kPicW = 7, kPicH = 8;           // [823C], [823E]: 56 squares of cloth
constexpr int kSquares = kPicW * kPicH;
// DS:84BA: where the four fabric pictures are (8 x 7 squares each).
constexpr int kPicture[4][2] = {{8, 6}, {8, 0xF9}, {0x1B7, 6}, {0x1B7, 0xF9}};

}  // namespace

bool Mystery::droppingSquares(int level) {
    // g18_26da / g18_21f8 / g18_22de.
    level = std::clamp(level, 0, 3);  // [8240]
    int points = 0;                   // [8242]
    int timeLeft = 0, limit = 0;      // [8000]:0, [930E]
    bool timeShown = true;            // [8000]:2
    int speed = 10;                   // [8000]:4: timer calls per drop
    int dropCount = 0;                // [8000]:6
    bool dropDue = false;             // [8000]:8
    int secondCount = 0;              // [8000]:A
    bool fast = false;                // [857A]: dropping
    int outcome = 0;                  // [857C]: 1 lost, 2 won
    bool quit = false, helpWanted = false;

    // A cell (3 bytes at DS:8348, 24 to a row): its colour (0 empty; 0xA0+
    // plain; 0x10+ cloth), the square of cloth it carries (-1 none), and
    // the mark for vanishing (1 cloth, 2 plain).
    struct Cell {
        int colour = 0, square = -1, mark = 0;
        bool empty() const { return colour == 0 && square < 0; }
    };
    Cell well[kRows][kCols];
    Cell piece[3];               // DS:84B0, top first
    int pieceCol = 0, pieceRow = -2;  // [856A], [856C]
    // DS:8246: the 56 squares of cloth: which picture, which square of it,
    // and whether it's back in place.
    struct Square {
        int picture = 0, at = 0;
        bool placed = false;
    } squares[kSquares];

    // g18_1cf0: which pictures the squares come from, and the time.
    int used[4][kSquares] = {}, usedCount[4] = {};
    auto take = [&](int picture, int i) {  // g18_1c72: a square of it not yet taken
        int at;
        do at = random(kSquares);
        while (std::find(used[picture], used[picture] + usedCount[picture], at) != used[picture] + usedCount[picture]);
        used[picture][usedCount[picture]++] = at;
        squares[i] = {picture, at, false};
    };
    switch (level) {
    case 0: {
        speed = 10, limit = 300;
        const int p = random(4);
        for (int i = 0; i < kSquares; ++i) take(p, i);
        break;
    }
    case 1: {
        speed = 6, limit = 0x168;
        const int p = random(4);
        int q;
        do q = random(4);
        while (q == p);
        for (int i = 0; i < kSquares; ++i) take(random(10) < 6 ? q : p, i);
        break;
    }
    case 2: {
        limit = 0x1A4, speed = 6;
        int a, b, c;
        if (random(10) < 6) {
            a = 3, b = random(3);
            do c = random(3);
            while (c == b);
        } else {
            a = 0, b = random(3) + 1;
            do c = random(3) + 1;
            while (c == b);
        }
        for (int i = 0; i < kSquares; ++i) {
            const int r = random(0x1E);
            take(r < 10 ? a : r > 0x13 ? b : c, i);
        }
        break;
    }
    default:
        speed = 4, limit = 0x21C;
        for (int i = 0; i < kSquares; ++i) take(random(4), i);
        break;
    }
    timeLeft = limit;

    // The squares of cloth are kept at the bottom left of screen 2 (8 x 7,
    // by their number), the pictures left with holes.
    auto store = [](int i, int* x, int* y) {  // g18_0000
        *x = i / kPicW * kCellW;
        *y = Screen::kHeight - 0x8C + i % kPicW * kCellH;
    };
    auto home = [&](const Square& s, int* x, int* y) {
        *x = s.at / kPicW * kCellW + kPicture[s.picture][0];
        *y = s.at % kPicW * kCellH + kPicture[s.picture][1];
    };

    auto drawCell = [&](int x, int y, const Cell& c) {  // g18_059e, on the current screen
        if (c.square < 0) {
            fill(x, y, kCellW, kCellH, static_cast<uint8_t>(c.colour));
            frame(x, y, kCellW, kCellH, c.colour ? 0xFF : 0);
        } else {
            int sx, sy;
            store(c.square, &sx, &sy);
            duplicateArea(2, current(), sx, sy, kCellW, kCellH, x, y);
            const uint8_t edge = static_cast<uint8_t>(squares[c.square].picture + 0x90);  // [857E]
            frame(x, y, kCellW, kCellH, edge);
            frame(x + 1, y + 1, kCellW - 2, kCellH - 2, edge);
            frame(x + 2, y + 2, kCellW - 4, kCellH - 4, edge);
        }
    };
    auto drawAt = [&](int col, int row, const Cell& c) {  // g18_0750
        if (row >= 0 && !c.empty()) drawCell(col * kCellW + kWellX, row * kCellH, c);
    };
    auto plain = [&] { return (level == 1 ? random(4) : random(3)) + 0xA0; };  // g18_0cfc
    auto cloth = [&] {  // g18_0d8c: one of the squares not yet placed
        std::vector<int> left;
        for (int i = 0; i < kSquares; ++i)
            if (!squares[i].placed) left.push_back(i);
        Cell c;
        c.square = left.empty() ? random(kSquares) : left[random(static_cast<int>(left.size()))];
        c.colour = level == 0 ? 0x10 : 0x10 + squares[c.square].picture;
        return c;
    };
    auto nextPiece = [&] {  // g18_0fd2
        std::vector<int> open;
        for (int c = 0; c < kCols; ++c)
            if (well[0][c].empty()) open.push_back(c);
        if (open.empty()) {  // the well is full
            outcome = 1;
            return;
        }
        pieceCol = open[random(static_cast<int>(open.size()))];
        const int r = random(10);
        for (Cell& c : piece) c = Cell{};
        if (r < 9) {
            if (r == 6) {
                for (Cell& c : piece) c = cloth();
            } else {
                const int a = random(3);
                piece[a] = cloth();
                if (r > 1) {
                    int b;
                    do b = random(3);
                    while (b == a);
                    piece[b] = cloth();
                }
            }
            for (Cell& c : piece)
                if (c.empty()) c.colour = plain();
        } else {
            for (Cell& c : piece) c.colour = plain();
        }
        pieceRow = -2;
    };
    auto free = [&](int col, int row) { return row < 0 || well[row][col].empty(); };
    auto moveBy = [&](int dx) {  // g18_0a3a / g18_0928
        const int c = pieceCol + dx;
        if (c < 0 || c >= kCols) return;
        for (int k = 0; k < 3; ++k)
            if (!free(c, pieceRow + k)) return;
        pieceCol = c;
    };
    auto rotate = [&] {  // g18_0b6a: the bottom square goes to the top
        const Cell bottom = piece[2];
        piece[2] = piece[1];
        piece[1] = piece[0];
        piece[0] = bottom;
    };
    auto flyHome = [&](int col, int row, int i) {  // g18_0060 / g18_0120: in 40 steps
        const int x0 = col * kCellW + kWellX, y0 = row * kCellH;
        int tx, ty, sqx, sqy;
        home(squares[i], &tx, &ty);
        store(i, &sqx, &sqy);
        select(1);
        for (int step = 1; step < 0x28; ++step) {
            const int x = x0 + (tx - x0) * step / 0x28, y = y0 + (ty - y0) * step / 0x28;
            const int saved = saveArea(x, y, kCellW, kCellH);
            duplicateArea(2, 1, sqx, sqy, kCellW, kCellH, x, y);
            if (step % 2 == 0) ctx_.pump();
            restoreArea(saved);
        }
        duplicateArea(2, 2, sqx, sqy, kCellW, kCellH, tx, ty);
        duplicateArea(2, 1, sqx, sqy, kCellW, kCellH, tx, ty);
    };
    auto run = [&](int col, int row, int dx, int dy) {  // g18_13ba: three or more alike from here
        if (dx == 0 && dy == 0) return false;
        const int colour = well[row][col].colour;
        if (!colour) return false;
        int xs[16], ys[16], n = 0;
        for (int c = col, r = row; c >= 0 && c < kCols && r >= 0 && r < kRows && well[r][c].colour == colour && n < 16;
             c += dx, r += dy)
            xs[n] = c, ys[n] = r, ++n;
        if (n < 3) return false;
        for (int k = 0; k < n; ++k) {
            Cell& cell = well[ys[k]][xs[k]];
            cell.mark = cell.square < 0 ? 2 : 1;
        }
        return true;
    };
    auto settle = [&] {  // g18_155c: take away the lines, drop what's left, again
        for (;;) {
            int found = 0;
            for (int c = 0; c < kCols; ++c)
                for (int r = 0; r < kRows; ++r)
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx) found += run(c, r, dx, dy);
            if (!found) break;
            select(1);
            for (int r = 0; r < kRows; ++r)
                for (int c = 0; c < kCols; ++c) {
                    Cell& cell = well[r][c];
                    if (!cell.mark) continue;
                    music(3);
                    drawCell(c * kCellW + kWellX, r * kCellH, Cell{});
                    if (cell.mark == 1) {
                        squares[cell.square].placed = true;
                        flyHome(c, r, cell.square);
                        points += 5;
                        intBox(0x7E, 0xCD, 0x41, 0x19, points);
                    }
                    cell = Cell{};
                }
            for (int c = 0; c < kCols; ++c) {  // g18_120c
                int to = kRows - 1;
                for (int r = kRows - 1; r >= 0; --r) {
                    if (well[r][c].empty()) continue;
                    if (r != to) {
                        well[to][c] = well[r][c];
                        well[r][c] = Cell{};
                    }
                    --to;
                }
            }
        }
        select(2);
    };
    auto land = [&] {  // g18_17bc, then settle and the next column
        music(2);
        for (int k = 0; k < 3; ++k)
            if (pieceRow + k >= 0) well[pieceRow + k][pieceCol] = piece[k];
        settle();
        nextPiece();
        fast = false;
    };
    auto fall = [&] {  // g18_1928 / g18_1878
        ++pieceRow;
        if (pieceRow >= kRows - 2) {
            pieceRow = kRows - 3;
            land();
        } else if (!well[pieceRow + 2][pieceCol].empty()) {
            --pieceRow;
            land();
        }
    };
    auto won = [&] {  // g18_1b0a: with fewer than 4 to go, the rest go in by themselves
        int left = 0;
        for (const Square& s : squares) left += !s.placed;
        if (left >= 4) return false;
        for (int i = 0; i < kSquares; ++i) {
            if (squares[i].placed) continue;
            squares[i].placed = true;
            int x, y, sx, sy;
            home(squares[i], &x, &y);
            store(i, &sx, &sy);
            duplicateArea(2, 1, sx, sy, kCellW, kCellH, x, y);
        }
        return true;
    };

    // The screen: the backdrop with the four pictures, their squares
    // taken out and kept.
    clearInput();
    ctx_.blackout();
    ctx_.showFullScreen(0x100B, 2);
    select(2);
    drawLogo(8, 0xA3, 0x20FB);
    show(2);
    computeUiColours();
    fill(kWellX, 0, kWellW, kWellH, 0);
    copyArea(2, 1, 0, 0, Screen::kWidth, Screen::kHeight);
    for (int i = 0; i < kSquares; ++i) {
        int x, y, sx, sy;
        home(squares[i], &x, &y);
        store(i, &sx, &sy);
        duplicateArea(1, 2, x, y, kCellW, kCellH, sx, sy);
        fill(x, y, kCellW, kCellH, 0);
        copyArea(2, 1, x, y, kCellW, kCellH);
    }
    digitalTime(0x1C1, 0xCD, 0x41, 0x19, timeLeft);
    intBox(0x7E, 0xCD, 0x41, 0x19, points);
    nextPiece();

    // The panels: exit, help and the four buttons under the well (DS:34EC).
    int pressedButton = -1, pressedFor = 0;
    panels_.clear();
    Panels::Panel exit;  // f06_23d8
    exit.x = 0x223, exit.y = 0xA3, exit.w = 0x55, exit.h = 0x4D;
    exit.buttons = {{0, 0, 0x55, 0x4D}};
    exit.onPress = [&](int k) {
        if (k >= 0) quit = true;
    };
    panels_.add(exit);
    Panels::Panel helpButton;  // f06_2436
    helpButton.x = 8, helpButton.y = 0xA3, helpButton.w = 0x55, helpButton.h = 0x4D;
    helpButton.buttons = {{0, 0, 0x55, 0x4D}};
    helpButton.onPress = [&](int k) {
        if (k >= 0) helpWanted = true;
    };
    panels_.add(helpButton);
    Panels::Panel controls;
    controls.x = 0xD5, controls.y = 0x131, controls.w = 0xC6, controls.h = 0x5E;
    controls.buttons = {{0, 0x14, 0x64, 0x28}, {0x64, 0x14, 0x64, 0x28}, {0x25, 0, 0x7B, 0x14}, {0, 0x3C, 0xC8, 0x20}};
    controls.onPress = [&](int k) {  // g18_1944
        if (k < 0) return;
        music(0x12);
        select(1);
        static constexpr int kPressed[4][3] = {
            {0xE0, 0x14C, 0x20F5}, {0x147, 0x14C, 0x20F6}, {0xE0, 0x134, 0x20F8}, {0xE0, 0x171, 0x20F7}};
        drawLogo(kPressed[k][0], kPressed[k][1], static_cast<uint16_t>(kPressed[k][2]));
        pressedButton = k, pressedFor = 2;
        if (k == 0) moveBy(-1);
        else if (k == 1) moveBy(1);
        else if (k == 2) rotate();
        else {  // g18_0b48
            fast = true;
            music(1);
        }
        select(2);
    };
    panels_.add(controls);
    ctx_.timer.setPeriodic(kSecondSlot, 10, [&] {  // g18_0468
        if (++dropCount >= speed) {
            dropCount = 0;
            dropDue = true;
        }
        if (++secondCount > 9) {
            secondCount = 0;
            if (timeLeft > 0) --timeLeft;
            timeShown = true;
        }
    });
    // The column speeds up once at the start, a step for each minute there
    // is to play (the original's loop in g18_22de).
    for (int m = 1; m <= timeLeft / 60 && speed >= 4; ++m)
        if (--speed < 4) speed = 4;
    clearInput();

    select(2);
    while (!quit && !outcome) {
        panels_.poll(ctx_.platform);
        ctx_.pump();
        for (int key; (key = ctx_.platform.takeKey()) != 0;) {  // g18_0bc4: the arrow keys and space
            if (key == Platform::kLeft) moveBy(-1);
            else if (key == Platform::kRight) moveBy(1);
            else if (key == Platform::kUp) rotate();
            else if (key == ' ' || key == Platform::kDown) {
                fast = true;
                music(1);
            }
        }
        if (helpWanted) {
            helpWanted = false;
            select(1);
            drawLogo(8, 0xA3, 0x20FC);
            std::vector<std::string> text;  // g18_1c0e: DS:3535's lines
            for (uint16_t at = 0x3535;;) {
                const std::string l = dataString(at);
                if (l.empty()) break;
                text.push_back(l);
                at = static_cast<uint16_t>(at + l.size() + 1);
            }
            messageBox(text);
            drawLogo(8, 0xA3, 0x20FB);
            select(2);
        }
        if (dropDue || fast) {
            dropDue = false;
            fall();
        }
        if (timeShown) {
            timeShown = false;
            digitalTime(0x1C1, 0xCD, 0x41, 0x19, timeLeft);
            select(2);
        }
        if (!outcome && won()) outcome = 2;
        // The well, drawn on screen 2 and shown.
        for (int r = 0; r < kRows; ++r)
            for (int c = 0; c < kCols; ++c) drawAt(c, r, well[r][c]);
        for (int k = 0; k < 3; ++k) drawAt(pieceCol, pieceRow + k, piece[k]);
        if (pressedButton >= 0 && --pressedFor <= 0) {
            copyArea(2, 1, 0xE1, kWellH, kWellW + 4, 0x5E);
            pressedButton = -1;
        }
        copyArea(2, 1, kWellX, 0, kWellW, kWellH);
        fill(kWellX, 0, kWellW, kWellH, 0);
    }
    ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);  // g18_26a8
    panels_.clear();
    select(1);
    if (outcome == 2) {
        // 200 points, and 4 for each second left.
        points += 200;
        const int usedTime = limit - timeLeft;  // [B794]
        intBox(0x7E, 0xCD, 0x41, 0x19, points);
        while (timeLeft > 0) {
            --timeLeft;
            digitalTime(0x1C1, 0xCD, 0x41, 0x19, timeLeft);
            points += 4;
            intBox(0x7E, 0xCD, 0x41, 0x19, points);
        }
        digitalTime(0x1C1, 0xCD, 0x41, 0x19, limit - usedTime);
        music(0);
        puzzleResult(true, points, 0, usedTime);
        return true;
    }
    puzzleResult(false, outcome == 1 ? points : 0, 0, 0);
    return false;
}

}  // namespace edison
