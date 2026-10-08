// Color Transformation (segment 27): a key at the top turns colours into
// others; each row's figure is shown with four recolourings, one of them
// following the key.

#include <algorithm>
#include <cmath>
#include <utility>

#include "mystery/mystery.h"

namespace edison {

namespace {

constexpr int kSecondSlot = 6;
constexpr int kFigureW = 0x46, kFigureH = 0x28;
constexpr int kChoiceX = 0xF5, kChoiceStep = 0x50, kRowY = 0x4B, kRowStep = 0x30;

}  // namespace

bool Mystery::colourTransformation(int level) {
    // g27_1032.
    const int limit = 0x78;  // [930E]
    int timeLeft = limit;    // [800A]:0
    bool timeShown = true;   // [800A]:2
    int points = 0;          // [4F4C]
    int misses = 0;          // [9024]
    int tries = 0;           // [C4FE]
    int solvedRows = 0;

    // The key: `pairs` colours and what they become. The easiest has just
    // black and white trading places.
    int pairs = std::min(level, 8) + 2;
    int count = pairs;  // colours in a figure's list
    int from[16], to[16];
    if (pairs <= 2) {
        pairs = 2, count = 16;
        for (int i = 0; i < 16; ++i) {
            from[i] = i & 1 ? 8 : 0xFF;
            to[i] = i & 1 ? 0xFF : 8;
        }
    } else {
        // g27_0d5e: different colours from 0x90-0x9F; each becomes the one 8 on.
        for (int i = 0; i < count; ++i) {
            bool again;
            do {
                from[i] = random(16) + 0x90;
                again = std::find(from, from + i, from[i]) != from + i;
            } while (again);
            to[i] = from[i] + 8;
            if (to[i] >= 0xA0) to[i] -= 0x10;
        }
    }

    // g27_0496: the ten figures, built of rectangles, diamonds, triangles
    // and circles, each piece in the next colour of the list; a white
    // border round the 70 x 40 box.
    auto figure = [&](int kind, const int* colours, int x, int y) {
        int at = 0;
        auto next = [&] {
            const int c = colours[at];
            if (++at >= count) at = 0;
            return static_cast<uint8_t>(c);
        };
        // draw_poly (f32_2e46): the library's spans take in the edges.
        auto poly = [&](std::vector<std::pair<int, int>> pts, uint8_t colour) { fillPolygonSolid(pts, colour); };
        auto rect = [&](int rx, int ry, int w, int h, uint8_t c) {  // g27_0082
            poly({{rx, ry}, {rx + w - 1, ry}, {rx + w - 1, ry + h - 1}, {rx, ry + h - 1}}, c);
        };
        auto diamond = [&](int rx, int ry, int w, int h) {  // g27_015c
            poly({{rx + (w - 1) / 2, ry}, {rx + w - 1, ry + (h - 1) / 2}, {rx + (w - 1) / 2, ry + h - 1},
                  {rx, ry + (h - 1) / 2}},
                 next());
        };
        auto upper = [&](int rx, int ry, int w, int h) {  // g27_025a
            poly({{rx, ry}, {rx + w - 1, ry}, {rx + w - 1, ry + h - 1}}, next());
        };
        auto lower = [&](int rx, int ry, int w, int h) {  // g27_0308
            poly({{rx, ry}, {rx + w - 1, ry + h - 1}, {rx, ry + h - 1}}, next());
        };
        auto circle = [&](int cx, int cy, int r) {  // g27_03b6: twenty points
            std::vector<std::pair<int, int>> pts;
            for (int k = 0; k < 20; ++k) {
                const double a = static_cast<uint16_t>(k * 0xD79) * (2 * 3.14159265358979 / 65536);
                pts.emplace_back(cx + static_cast<int>(std::lround(std::cos(a) * 32767) * r >> 15),
                                 cy + static_cast<int>(std::lround(std::sin(a) * 32767) * r >> 15));
            }
            poly(pts, next());
        };
        auto box = [&](int rx, int ry, int w, int h) { rect(rx, ry, w, h, next()); };
        switch (kind) {
        case 0:
            for (int k = 5; k > 0; --k) circle(x + 0x23, y + 0x14, 0x14 * k / 5);
            break;
        case 1:
            box(x, y, 0x23, 0x28);
            box(x + 0x23, y, 0x23, 0x28);
            break;
        case 2:
            box(x + 0x23, y, 0x23, 0x14);
            box(x + 0x23, y + 0x14, 0x23, 0x14);
            box(x, y, 0x23, 0x14);
            box(x, y + 0x14, 0x23, 0x14);
            break;
        case 3:
            circle(x + 0x34, y + 0x1E, 10);
            circle(x + 0x11, y + 0x0A, 10);
            circle(x + 0x34, y + 0x0A, 10);
            circle(x + 0x11, y + 0x1E, 10);
            circle(x + 0x23, y + 0x14, 15);
            break;
        case 4:
            lower(x, y, kFigureW, kFigureH);
            upper(x, y, kFigureW, kFigureH);
            circle(x + 0x23, y + 0x14, 13);
            break;
        case 5:
            box(x, y, kFigureW, kFigureH);
            diamond(x, y, kFigureW, kFigureH);
            break;
        case 6:
            box(x, y, kFigureW, kFigureH);
            lower(x, y + 0x14, 0x23, 0x14);
            upper(x + 0x23, y, 0x23, 0x14);
            box(x, y, 0x23, 0x14);
            box(x + 0x23, y + 0x14, 0x23, 0x14);
            break;
        case 7:
            lower(x, y, 0x23, 0x14);
            upper(x, y, 0x23, 0x14);
            lower(x + 0x23, y + 0x14, 0x23, 0x14);
            upper(x + 0x23, y + 0x14, 0x23, 0x14);
            circle(x + 0x23, y + 0x14, 20);
            diamond(x + 0x17, y + 0x0D, 0x17, 0x0D);
            break;
        case 8:
            box(x, y, kFigureW, kFigureH);
            circle(x + 0x23, y + 0x14, 20);
            diamond(x + 0x0F, y, 0x28, 0x28);
            break;
        default:
            lower(x, y, kFigureW, kFigureH);
            upper(x, y, kFigureW, kFigureH);
            circle(x + 0x23, y + 0x14, 20);
            diamond(x + 0x0F, y, 0x28, 0x28);
            break;
        }
        rect(x, y, kFigureW, 1, 0xFF);
        rect(x, y + 0x27, kFigureW, 1, 0xFF);
        rect(x + 0x45, y, 1, kFigureH, 0xFF);
        rect(x, y, 1, kFigureH, 0xFF);
    };

    struct Row {
        int figure = 0;
        int right = 0;
        bool tried[4] = {};
        bool solved = false;
        int colours[4][16] = {};
    } rows[5];

    clearInput();
    ctx_.blackout();
    ctx_.showFullScreen(0x1008, 2);
    select(2);
    intBox(0xCC, 0x14E, 0x60, 0x19, points);
    drawOpaque(0x12C, 0x162, 0x21D9);
    // The key.
    const int cell = 0x208 / pairs, w = cell / 3;
    for (int i = 0; i < pairs; ++i) {
        const int x = i * 0x208 / pairs + 0x4E;
        fill(x, 0x25, w, 0x1E, static_cast<uint8_t>(from[i]));
        fill(x + w, 0x25, w, 0x1E, static_cast<uint8_t>(to[i]));
        drawCentred(x + w, 0x34, 0x20A7);
    }
    // The rows: a figure (none twice), and four recolourings, the right
    // one at random and the others shuffles of the key's colours, none the
    // same as another (g27_0df2, g27_0ec4).
    for (int i = 0; i < 5; ++i) {
        Row& r = rows[i];
        bool again;
        do {
            r.figure = random(10);
            again = false;
            for (int j = 0; j < i; ++j) again |= rows[j].figure == r.figure;
        } while (again);
        const int y = i * kRowStep + kRowY;
        figure(r.figure, from, 0x5F, y);
        drawCentred(0xD2, i * kRowStep + 0x5F, 0x20A8);
        r.right = random(4);
        for (int k = 0; k < 4;) {
            int* c = r.colours[k];
            bool ok = true;
            if (k == r.right) {
                std::copy(to, to + count, c);
            } else {
                std::copy(to, to + count, c);
                for (int j = 0; j < count; ++j) std::swap(c[j], c[random(count)]);
                if (c[0] == to[0] && c[1] == to[1]) std::swap(c[0], c[1]);
                for (int j = 0; j < k && ok; ++j) ok = !std::equal(c, c + count, r.colours[j]);
                if (std::equal(c, c + count, to)) ok = false;
            }
            figure(r.figure, c, k * kChoiceStep + kChoiceX, y);
            if (ok) ++k;
        }
    }
    select(1);
    show(2);
    computeUiColours();

    // Panel DS:5034: the exit, help, the gadget and the twenty choices.
    bool quit = false, helpWanted = false, gadget = false;
    int picked = -1;
    panels_.clear();
    Panels::Panel panel;
    panel.x = 0, panel.y = 0, panel.w = Screen::kWidth, panel.h = Screen::kHeight;
    panel.buttons = {{0x1FF, 0x15F, 0x44, 0x1C}, {0x26, 0x15F, 0x44, 0x1C}, {0x12C, 0x162, 0x38, 0x24}};
    for (int i = 0; i < 5; ++i)
        for (int k = 0; k < 4; ++k)
            panel.buttons.push_back({k * kChoiceStep + kChoiceX, i * kRowStep + kRowY, kFigureW, kFigureH});
    panel.onPress = [&](int k) {  // g27_0036
        if (k == 0) quit = true;
        else if (k == 1) helpWanted = true;
        else if (k == 2) gadget = true;
        else if (k > 2) picked = k - 3;
    };
    panels_.add(panel);
    ctx_.timer.setPeriodic(kSecondSlot, 1, [&] {  // g27_0000
        if (timeLeft > 0) --timeLeft;
        timeShown = true;
    });
    clearInput();

    while (!quit && solvedRows < 5) {
        panels_.poll(ctx_.platform);
        ctx_.pump();
        if (timeShown) {
            digitalTime(0x164, 0x14E, 0x60, 0x19, timeLeft);
            timeShown = false;
        }
        if (helpWanted) {
            helpWanted = false;
            drawLogo(0x26, 0x15F, 0x20A6);
            help(0x3F08);
            drawLogo(0x26, 0x15F, 0x20A5);
        }
        if (gadget) {
            gadget = false;
            music(0x19);
            monitorGadget();
        }
        if (picked >= 0) {
            const int i = picked / 4, k = picked % 4;
            picked = -1;
            Row& r = rows[i];
            if (!r.solved) {
                r.tried[k] = true;
                ++tries;
                const int cx = k * kChoiceStep + 0x118, cy = i * kRowStep + 0x5F;
                if (r.right == k) {
                    r.solved = true;
                    music(0x25);
                    drawCentred(cx, cy, 0x20A3);
                    ++solvedRows;
                    points += 0x32;
                } else {
                    music(0x26);
                    drawCentred(cx, cy, 0x20A4);
                    points -= 0x32;
                    ++misses;
                }
                intBox(0xCC, 0x14E, 0x60, 0x19, points);
            }
        }
    }
    int used = 0;  // [B794]
    if (solvedRows >= 5) {
        // 100 points, and 2 for each second left.
        points += 100;
        used = limit - timeLeft;
        while (timeLeft > 0) {
            --timeLeft;
            digitalTime(0x164, 0x14E, 0x60, 0x19, timeLeft);
            points += 2;
            intBox(0xCC, 0x14E, 0x60, 0x19, points);
        }
        digitalTime(0x164, 0x14E, 0x60, 0x19, limit - used);
    }
    // Five right first time: the colours 0x90-0x9F go round for a while.
    const bool perfect = tries == 5 && solvedRows == 5;
    int shown = perfect ? 0x20 : 10;
    ctx_.countdown[0] = shown;
    while (ctx_.countdown[0] > 0) {
        ctx_.pump();
        if (perfect && ctx_.countdown[0] != shown) {
            --shown;
            Palette& p = ctx_.screens[1].palette;
            const Rgb last = p[0x9F];
            for (int i = 0x9F; i > 0x90; --i) p[i] = p[i - 1];
            p[0x90] = last;
            ctx_.setDisplayPalette(1);
        }
    }
    panels_.clear();
    const bool won = solvedRows >= 5;
    if (won)
        puzzleResult(true, points, misses / 5, used);
    else
        puzzleResult(false, 0, 0, 0);
    ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);
    select(1);
    fill(0, 0, Screen::kWidth, Screen::kHeight, 0);
    return won;
}

}  // namespace edison
