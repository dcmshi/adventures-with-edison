// The Dig (segment 21): twenty tiles in a 5 x 4 wall, each edge showing a
// symbol that matches its neighbour's. Some tiles are dug out onto a belt
// below; drag them back.

#include <algorithm>
#include <cstdlib>

#include "mystery/mystery.h"

namespace edison {

namespace {

constexpr int kSecondSlot = 6;
constexpr int kTileW = 100, kTileH = 60;
constexpr int kWallX = 0x46, kWallY = 0x2D;   // the wall's top left (tiles 100 x 60)
constexpr int kBeltX = 0x44, kBeltY = 0x13F;  // five belt slots, 101 apart
// Panel DS:3DD2's buttons (DS:3D82).
enum Button { kExit, kHelp, kSolution, kRight, kLeft, kBelt, kWall, kGadget, kButtons };
constexpr int kButton[kButtons][4] = {
    {0x222, 0x00, 0x5E, 0x20}, {0x1B0, 0x00, 0x46, 0x20}, {0x24C, 0xE0, 0x34, 0x48},
    {0x24A, 0x148, 0x32, 0x32}, {0x000, 0x146, 0x32, 0x32}, {0x44, 0x13F, 0x1F8, 0x3C},
    {0x46, 0x2D, 0x1F4, 0xF0}, {0x000, 0x48, 0x3A, 0x32},
};

}  // namespace

bool Mystery::dig(int level) {
    // g21_185c.
    static constexpr int kSeconds[8] = {0x78, 0xB4, 0xF0, 0xF0, 0x12C, 0x12C, 0x12C, 0x1A4};
    level = std::min(level, 7);
    const int limit = kSeconds[level];  // [930E]
    int timeLeft = limit;               // [8002]:2
    bool timeShown = false;             // [8002]:0
    // The symbols ([89D4], three from there) and the tile ([89D6]). The
    // penguins (players at level 6 or below) must go back where they were
    // ([89D8]); the others just have to match their neighbours.
    uint16_t symbols = 0x20E3, tileBack = 0x20E6;
    bool exact = true;
    if (player_.level > 6) {
        const int r = random(10);
        symbols = r < 3 ? 0x20C6 : r < 6 ? 0x20C9 : 0x20CC;
        tileBack = 0x20CF;
        exact = false;
    }
    int points = 0;  // [88BA]

    struct Tile {
        int v[4];  // the symbols on the top, right, bottom and left edges
    };
    Tile tiles[20];     // DS:88BC
    int wall[5][4];     // DS:895C: [column][row], a tile or -1
    int solution[5][4]; // DS:8984
    int belt[20];       // DS:89AC
    int beltStart = 0;  // [3DEC]

    // g21_02a8: tiles at random places, their shared edges made to match,
    // then (level + 1) * 2 of them dug out onto the belt.
    auto deal = [&] {
    for (int& b : belt) b = -1;
    for (auto& c : wall) for (int& t : c) t = -1;
    for (auto& c : solution) for (int& t : c) t = -1;
    for (int i = 0; i < 20;) {
        const int a = random(5), b = random(4);
        if (solution[a][b] >= 0) continue;
        solution[a][b] = wall[a][b] = i;
        for (int& v : tiles[i].v) v = random(3);
        ++i;
    }
    for (int a = 1; a < 5; ++a)
        for (int b = 0; b < 4; ++b) tiles[solution[a][b]].v[3] = tiles[solution[a - 1][b]].v[1];
    for (int b = 1; b < 4; ++b)
        for (int a = 0; a < 5; ++a) tiles[solution[a][b]].v[0] = tiles[solution[a][b - 1]].v[2];
    for (int i = 0; i < (level + 1) * 2;) {
        const int a = random(5), b = random(4);
        if (wall[a][b] < 0) continue;
        belt[i++] = wall[a][b];
        wall[a][b] = -1;
    }
    };

    auto drawTile = [&](int t, int x, int y, int screen) {  // g21_0586
        // Put together at (0x19, 0x19) on screen 2 (the symbols straddle
        // the edges), then copied.
        const int previous = current();
        select(2);
        if (t < 0) {
            drawOpaque(0x19, 0x19, 0x20D0);
        } else {
            drawOpaque(0x19, 0x19, tileBack);
            auto symbol = [&](int cx, int cy, int v) {
                const uint16_t id = static_cast<uint16_t>(symbols + v);
                const Bitmap& bmp = ctx_.bitmap(id);
                ctx_.screens.drawSprite(2, bmp, cx - bmp.width / 2, cy - bmp.height / 2);
            };
            symbol(0x19, 0x37, tiles[t].v[3]);
            symbol(0x7C, 0x37, tiles[t].v[1]);
            symbol(0x4B, 0x19, tiles[t].v[0]);
            symbol(0x4B, 0x54, tiles[t].v[2]);
        }
        duplicateArea(2, screen, 0x19, 0x19, kTileW, kTileH, x, y);
        select(previous);
    };
    auto drawWall = [&](int grid[5][4]) {  // g21_06a8
        for (int a = 0; a < 5; ++a)
            for (int b = 0; b < 4; ++b) drawTile(grid[a][b], a * 100 + kWallX, b * 60 + kWallY, 1);
    };
    auto drawBelt = [&](int offset, int count, int x, int y, int screen) {  // g21_0840
        for (int i = 0; i < count; ++i) {
            const int k = ((i + beltStart + offset) % 20 + 20) % 20;
            drawTile(belt[k], x + i + i * 100, y, screen);
            if (i < count - 1) {
                const int previous = current();
                select(screen);
                const int lx = x + i + (i + 1) * 100;
                displayLine(lx, y, lx, y + 0x3B, 0xFF);
                select(previous);
            }
        }
        select(1);
    };
    auto button = [&](int k, uint16_t id) { drawLogo(kButton[k][0], kButton[k][1], id); };  // g21_01d4
    auto showTime = [&](int y = 10) {  // g21_027a (y 0xC while a tile is held: 21:0da0)
        if (!timeShown) return;
        digitalTime(0x6A, y, 0x2A, 0x14, timeLeft);
        timeShown = false;
    };
    auto beltEmpty = [&] {  // g21_148e
        for (int b : belt)
            if (b >= 0) return false;
        return true;
    };
    auto shownEmpty = [&] {  // g21_090c: the five slots in view
        for (int i = 0; i < 5; ++i)
            if (belt[(beltStart + i) % 20] >= 0) return false;
        return true;
    };
    auto startTimer = [&](bool on) {
        if (on)
            ctx_.timer.setPeriodic(kSecondSlot, 1, [&] {  // g21_0000
                if (timeLeft > 0) --timeLeft;
                timeShown = true;
            });
        else
            ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);
    };
    auto scroll = [&](int dir) {  // g21_096e: 0 right, 1 left
        button(kRight + dir, static_cast<uint16_t>(0x20C2 + dir));
        const int start = dir ? 100 : 0, delta = dir ? -100 : 100;
        drawBelt(-dir, 6, 0, 200, 2);
        ctx_.countdown[0] = 4;
        for (;;) {
            const int t = ctx_.countdown[0];
            duplicateArea(2, 1, (4 - t + 1) * delta / 5 + start, 200, 0x1F8, 0x3C, kBeltX, kBeltY);
            if (t <= 0) break;
            ctx_.pump();
        }
        beltStart = ((beltStart + (dir ? -1 : 1)) % 20 + 20) % 20;
        drawBelt(0, 5, kBeltX, kBeltY, 1);
        button(kRight + dir, static_cast<uint16_t>(0x20C4 + dir));
    };
    auto gadget = [&] {  // g21_0082
        static constexpr int kFrames[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7, 10, 11, 2, 0};
        music(7);
        select(1);
        drawLogo(0, 0x48, 0x20E7);
        for (int f : kFrames) {
            select(2);
            drawLogo(0, 0x90, static_cast<uint16_t>(0x20E9 + f));
            copyArea(2, 1, 0, 0x90, 0x32, 0x3C);
            select(1);
            waitCountdown(f == 7 || f == 9 ? 4 : 2);
        }
        drawLogo(0, 0x48, 0x20E8);
    };
    // g21_0a74: the slot nearest (x, y): the belt's five and the wall's
    // twenty, by |dx| + |dy|.
    struct Slot {
        int index = 0, row = -1;  // a belt index (row -1), or the wall's column and row
        int cx = 0, cy = 0;
        int tile = -1;
    };
    auto nearest = [&](int x, int y) {
        Slot s;
        int best = 30000;
        for (int i = 0; i < 5; ++i) {
            const int cx = i * 0x65 + 0x76, cy = 0x15D, d = std::abs(y - cy) + std::abs(x - cx);
            if (d >= best) continue;
            best = d;
            s.index = (i + beltStart) % 20;
            s.row = -1, s.cx = cx, s.cy = cy, s.tile = belt[s.index];
        }
        for (int a = 0; a < 5; ++a)
            for (int b = 0; b < 4; ++b) {
                const int cx = a * 0x64 + 0x78, cy = b * 0x3C + 0x4B, d = std::abs(y - cy) + std::abs(x - cx);
                if (d >= best) continue;
                best = d;
                s.index = a, s.row = b, s.cx = cx, s.cy = cy, s.tile = wall[a][b];
            }
        return s;
    };
    auto fits = [&](int t, int a, int b) {
        if (exact) {
            for (int k = 0; k < 4; ++k)
                if (tiles[solution[a][b]].v[k] != tiles[t].v[k]) return false;
            return true;
        }
        if (a > 0 && wall[a - 1][b] >= 0 && tiles[wall[a - 1][b]].v[1] != tiles[t].v[3]) return false;
        if (a < 4 && wall[a + 1][b] >= 0 && tiles[wall[a + 1][b]].v[3] != tiles[t].v[1]) return false;
        if (b > 0 && wall[a][b - 1] >= 0 && tiles[wall[a][b - 1]].v[2] != tiles[t].v[0]) return false;
        if (b < 3 && wall[a][b + 1] >= 0 && tiles[wall[a][b + 1]].v[0] != tiles[t].v[2]) return false;
        return true;
    };
    auto drag = [&] {  // g21_0be8: a click picks a tile up, the next one puts it down
        int mx, my;
        bool down;
        ctx_.platform.mouse(&mx, &my, &down);
        const Slot from = nearest(mx, my);
        if (from.tile < 0) return;
        const int t = from.tile;
        for (int& b : belt)
            if (b == t) b = -1;
        for (auto& c : wall)
            for (int& w : c)
                if (w == t) w = -1;
        drawBelt(0, 5, kBeltX, kBeltY, 1);
        drawWall(wall);
        // The tile's picture, made at (0x140, 0) on screen 2, is drawn
        // shrinking to half size while held and growing back as it flies
        // to where it lands.
        drawTile(t, 0x140, 0, 2);
        // It's grabbed into a DIB (g35_071a: 0x36 bytes of headers and one
        // palette entry, then the rows bottom up), but the scaler is given
        // the block's start as the pixels' (21:0d4e; the Ball Sculpture's
        // g30_0f98 skips the headers): each row 0x3A bytes early, the left
        // of it from the row below, the bottom row's left the headers.
        std::vector<uint8_t> block(0x3A + kTileW * kTileH);
        const uint8_t info[0x28] = {0x28, 0, 0, 0, kTileW, 0, 0, 0, kTileH, 0, 0, 0, 1, 0, 8, 0,
                                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};  // biClrUsed 1
        std::copy(info, info + 0x28, block.begin() + 0x0E);
        for (int r = 0; r < kTileH; ++r)
            for (int c = 0; c < kTileW; ++c)
                block[0x3A + (kTileH - 1 - r) * kTileW + c] =
                    ctx_.screens[2].pixels[static_cast<size_t>(r) * Screen::kWidth + 0x140 + c];
        std::vector<uint8_t> image(kTileW * kTileH);
        for (int r = 0; r < kTileH; ++r)
            for (int c = 0; c < kTileW; ++c) image[r * kTileW + c] = block[(kTileH - 1 - r) * kTileW + c];
        int saved = saveArea(mx - 50, my - 30, kTileW, kTileH);
        int shownScale = -1, shownX = -1, shownY = -1;
        int w = kTileW, h = kTileH;
        bool held = true;
        Slot to = from;
        ctx_.countdown[0] = 5;
        while (held || ctx_.countdown[0] > 0) {
            ctx_.pump();
            showTime(0xC);
            ctx_.platform.mouse(&mx, &my, &down);
            const int tick = ctx_.countdown[0];
            const int scale = held ? tick * 128 / 5 + 128 : (5 - tick) * 128 / 5 + 128;
            int x = std::clamp(mx, w / 2, Screen::kWidth - w / 2 - 1);
            int y = std::clamp(my, h / 2, Screen::kHeight - h / 2 - 1);
            if (!held) {
                x = (x - to.cx) * tick / 5 + to.cx;
                y = (y - to.cy) * tick / 5 + to.cy;
            }
            if (scale != shownScale || x != shownX || y != shownY) {
                w = kTileW * scale / 256;
                h = kTileH * scale / 256;
                restoreArea(saved);
                saved = saveArea(x - w / 2, y - h / 2, w, h);
                // The library's scaler (f41_0024, 21:10f7): 16.16 steps of 256 / scale.
                const uint32_t step = scaleStep(scale);
                drawVia3(x - w / 2, y - h / 2, w, h, [&](int s) {
                    Screen& scr = ctx_.screens[s];
                    for (int r = 0; r < h; ++r)
                        for (int c = 0; c < w; ++c) {
                            const int px = x - w / 2 + c, py = y - h / 2 + r;
                            if (px < 0 || py < 0 || px >= Screen::kWidth || py >= Screen::kHeight) continue;
                            const int sr = std::min(static_cast<int>(r * step >> 16), kTileH - 1);
                            const int sc = std::min(static_cast<int>(c * step >> 16), kTileW - 1);
                            scr.pixels[static_cast<size_t>(py) * Screen::kWidth + px] = image[sr * kTileW + sc];
                        }
                });
                shownScale = scale, shownX = x, shownY = y;
            }
            // The next press ([739F], cleared at 21:0d74; 21:11db), not the
            // release, puts it down.
            int px, py;
            if (held && ctx_.platform.takeClick(&px, &py)) {
                mx = px, my = py;
                to = nearest(mx, my);
                bool ok = to.tile < 0;
                if (ok && to.row < 0) {
                    belt[to.index] = t;
                } else if (ok) {
                    ok = fits(t, to.index, to.row);
                    if (ok) {
                        wall[to.index][to.row] = t;
                        ++pic_.moves;
                        points += 0x19;
                    } else {
                        points -= 10;
                    }
                    intBox(0x142, 0xC, 0x28, 0x14, points);
                }
                if (!ok) {  // back where it came from
                    to = from;
                    if (from.row < 0) belt[from.index] = t;
                    else wall[from.index][from.row] = t;
                }
                held = false;
                ctx_.countdown[0] = 5;
            }
        }
        restoreArea(saved);
        drawBelt(0, 5, kBeltX, kBeltY, 1);
        drawWall(wall);
    };

    clearInput();
    backdrop(0x100A);
    select(2);
    intBox(0x142, 0xC, 0x28, 0x14, points);
    button(kExit, 0x20BC);
    button(kHelp, 0x20BE);
    button(kSolution, 0x20C0);
    button(kRight, 0x20C4);
    button(kLeft, 0x20C5);
    pic_.moves = 0;
    bool pressed[kButtons] = {}, held[kButtons] = {};
    panels_.clear();
    Panels::Panel panel;  // DS:3DD2
    panel.x = 0, panel.y = 0, panel.w = Screen::kWidth, panel.h = Screen::kHeight;
    for (const auto& b : kButton) panel.buttons.push_back({b[0], b[1], b[2], b[3]});
    panel.onPress = [&](int k) {  // g21_0036
        if (k >= 0) pressed[k] = held[k] = true;
    };
    panel.onRelease = [&](int k) {  // g21_005c
        if (k >= 0) held[k] = false;
    };
    panels_.add(panel);
    select(1);
    show(2);
    deal();  // after f04_005c's stir (21:1A24)
    // (No f06_01f6 here: the UI colours stay the previous screen's.)
    drawWall(wall);
    drawBelt(0, 5, kBeltX, kBeltY, 1);
    startTimer(true);  // (the clock is drawn when a second has gone: 21:1a88)

    auto take = [&](int k) {
        const bool on = pressed[k] || held[k];
        pressed[k] = false;
        return on;
    };
    bool moved = true;
    bool quit = false;
    while (!beltEmpty()) {
        panels_.poll(ctx_.platform);
        ctx_.pump();
        showTime();
        if (take(kHelp)) {
            startTimer(false);
            button(kHelp, 0x20BF);
            // g21_17e4: the three symbols on the belt while the help shows.
            fill(kBeltX, kBeltY, 0x1F8, 0x3C, 0);
            for (int k = 0; k < 3; ++k) drawLogo(0x1F8 * k / 3 + kBeltX, kBeltY, static_cast<uint16_t>(symbols + k));
            help(0x3F0A);
            drawBelt(0, 5, kBeltX, kBeltY, 1);
            button(kHelp, 0x20BE);
            held[kHelp] = false;
            startTimer(true);
        }
        if (take(kSolution)) {
            button(kSolution, 0x20C1);
            drawWall(solution);
            for (;;) {
                int mx, my;
                bool down;
                ctx_.platform.mouse(&mx, &my, &down);
                if (!down) break;
                ctx_.pump();
                showTime();
            }
            drawWall(wall);
            button(kSolution, 0x20C0);
            held[kSolution] = false;
        }
        if (take(kExit)) {
            button(kExit, 0x20BD);
            quit = true;
            break;
        }
        if (take(kLeft)) {
            music(0x12);
            scroll(1);
        }
        if (take(kGadget)) {
            held[kGadget] = false;
            gadget();
        }
        if (take(kRight)) {
            music(0x12);
            scroll(0);
        }
        const bool fromBelt = take(kBelt), fromWall = take(kWall);
        if (fromBelt || fromWall) {
            music(0x12);
            drag();
            moved = true;
            held[kBelt] = held[kWall] = false;
        }
        if (moved && !beltEmpty() && shownEmpty()) {
            // Nothing in view: the arrows flash.
            for (int i = 0, lit = 0; i < 10; ++i, lit = lit ? 0 : 2) {
                button(kRight, static_cast<uint16_t>(0x20C2 + lit));
                button(kLeft, static_cast<uint16_t>(0x20C3 + lit));
                waitCountdown(2);
            }
            moved = false;
        }
    }
    startTimer(false);
    panels_.clear();
    bool won = false;
    if (!quit && beltEmpty()) {
        // 200 points, and 2 for each second left.
        points += 200;
        intBox(0x142, 0xC, 0x28, 0x14, points);
        const int used = limit - timeLeft;
        while (timeLeft > 0) {
            --timeLeft;
            digitalTime(0x6A, 10, 0x2A, 0x14, timeLeft);
            points += 2;
            intBox(0x142, 0xC, 0x28, 0x14, points);
        }
        digitalTime(0x6A, 10, 0x2A, 0x14, limit - used);
        music(0x20);
        select(1);
        if (symbols == 0x20E3) {
            // g21_1684: two penguins waddle off, one up and one down. Each
            // step waits 2000h passes of an empty loop (21:179e), about 1 ms
            // under winevdm (measured: the walk takes 0.2-0.3 s); the port
            // gives each step 1 ms of the clock.
            const Bitmap& p = ctx_.bitmap(0x20E3);
            int x1 = 0, y1 = (Screen::kHeight - p.height) / 2;
            int x2 = Screen::kWidth - p.width, y2 = y1;
            bool done1 = false, done2 = false;
            const uint64_t start = ctx_.platform.milliseconds();
            for (uint64_t step = 0; !(done1 && done2); ++step) {
                if (x1 <= Screen::kWidth / 2) x1 += 2;
                else if ((y1 -= 2) < 0) done1 = true;
                if (!done1) drawLogo(x1, y1, 0x20E3);
                if (x2 >= Screen::kWidth / 2) x2 -= 2;
                else if ((y2 += 2) >= Screen::kHeight - p.height) done2 = true;
                if (!done2) drawLogo(x2, y2, 0x20E3);
                while (ctx_.platform.milliseconds() - start <= step) ctx_.pump();
            }
        } else {
            // g21_14e2: a ball bounces over its shadow (DS:3DEE heights).
            static constexpr int kHeights[9] = {5, 18, 38, 82, 150, 99, 40, 8, 1};
            int ball = -1, shadow = -1;
            clearInput();
            for (int i = 0; i < 0x24 && !anyInput(); ++i) {
                const int k = i % 9;
                if (ball >= 0) restoreArea(ball);
                if (shadow >= 0) restoreArea(shadow);
                const Bitmap& b = ctx_.bitmap(static_cast<uint16_t>(0x20D1 + k));
                const Bitmap& s = ctx_.bitmap(static_cast<uint16_t>(0x20DA + k));
                const int by = kHeights[k] + 0x8F;
                ball = saveArea(0x140 - b.width / 2, by - b.height / 2, b.width, b.height);
                shadow = saveArea(0x140 - s.width / 2, 0x136 - s.height / 2, s.width, s.height);
                drawLogo(0x140 - s.width / 2, 0x136 - s.height / 2, static_cast<uint16_t>(0x20DA + k));
                drawLogo(0x140 - b.width / 2, by - b.height / 2, static_cast<uint16_t>(0x20D1 + k));
                waitCountdown(1);
            }
            if (ball >= 0) restoreArea(ball);
            if (shadow >= 0) restoreArea(shadow);
        }
        won = puzzleResult(true, points, 0, used);
    } else {
        puzzleResult(false, 0, 0, 0);
    }
    select(1);
    fill(0, 0, Screen::kWidth, Screen::kHeight, 0);
    return won;
}

}  // namespace edison
