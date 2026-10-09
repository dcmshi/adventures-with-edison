// The 3D Ball Sculpture (segment 30): balls on a 3 x 3 x 3 frame turn in
// the middle; pick the small picture of the same sculpture. Five of them.

#include <algorithm>

#include "mystery/mystery.h"

namespace edison {

namespace {

constexpr int kSecondSlot = 6;
constexpr int kSpots = 0x1B;  // DS:63DE: the frame's 27 places
constexpr int kBoxesX = 0x1B2, kBoxesY = 0xDE;
constexpr int kBox[4][4] = {{0x10, 0xE, 0x4C, 0x38}, {0x68, 0xE, 0x4C, 0x38}, {0x10, 0x52, 0x4C, 0x38}, {0x68, 0x52, 0x4C, 0x38}};
constexpr int kArrowsX = 0x40, kArrowsY = 0x110;
constexpr int kArrow[4][4] = {{0x18, 2, 0x28, 0x19}, {0x28, 0x39, 0x28, 0x17}, {0, 0x1E, 0x28, 0x19}, {0x40, 0x19, 0x20, 0x1C}};
constexpr int kArrowMid[4][2] = {{0x14, 0xD}, {0x14, 0xC}, {0x14, 0xD}, {0x10, 0xE}};
constexpr int kViewX = 0x10E, kViewY = 0x82, kViewW = 100, kViewH = 0x5F;  // the turning one
constexpr int kShotX = 0xFC, kShotY = 0x70, kShotW = 0x98, kShotH = 0x72;  // made for the small ones

}  // namespace

bool Mystery::ballSculpture(int level) {
    // g30_136c. The time comes from the level of the game before ([9282]
    // is read before it's set).
    const int limit = dataWord(static_cast<uint16_t>(ballLevel_ * 0xF + 0x666D));
    level = std::min(std::max(level, 0), 5);
    ballLevel_ = level;
    const uint16_t row = static_cast<uint16_t>(0x6660 + level * 0xF);
    const int colours = data_[row];  // [C65D]
    int timeLeft = limit, timeShown = -1;
    int points = 0, misses = 0, left = 5;
    auto word = [&](uint16_t at) { return static_cast<int16_t>(dataWord(at)); };

    std::vector<Point3> spots, frame;
    for (int k = 0; k < kSpots; ++k)
        spots.push_back({word(static_cast<uint16_t>(0x63DE + 6 * k)), word(static_cast<uint16_t>(0x63E0 + 6 * k)),
                         word(static_cast<uint16_t>(0x63E2 + 6 * k))});
    for (int k = 0; k < 8; ++k)
        frame.push_back({word(static_cast<uint16_t>(0x6408 + 6 * k)), word(static_cast<uint16_t>(0x640A + 6 * k)),
                         word(static_cast<uint16_t>(0x640C + 6 * k))});

    std::vector<Point3> balls;       // DS:C452
    std::vector<int> ballColour;     // DS:C5B6 (DS:B32C keeps them)
    std::vector<int> shape;          // this sculpture's places
    uint16_t shapeList = 0;          // where they're listed (DS:6480...)
    int shapeCount = 0;              // [C774]
    uint16_t turnA = 0, turnC = 0;   // [9396], [939A]

    auto build = [&](int mode) {  // g30_0c26: 0 as it is, 1 one or two balls moved, 2 some added
        int a = -1, b = -1, extra = 0;
        if (mode) {
            mode += random(2);
            if (mode == 2 && shapeCount == kSpots) mode = 1;
            if (mode == 1) {
                a = random(shapeCount);
                if (random(2)) {
                    do b = random(shapeCount);
                    while (b == a);
                }
            } else {
                extra = std::min(random(4) + 1, kSpots - shapeCount);
            }
        }
        const bool first = ballColour.empty();
        balls.clear();
        for (int i = 0; i < static_cast<int>(shape.size()) && i < kSpots; ++i) {
            int at = shape[static_cast<size_t>(i)];
            if (mode == 1 && (i == a || i == b)) {
                do at = random(kSpots);
                while (at == shape[static_cast<size_t>(i)]);
            }
            balls.push_back(spots[static_cast<size_t>(at)]);
            if (first) ballColour.push_back(colours == 1 ? 0 : random(colours));
        }
        std::vector<int> colour = ballColour;
        // The added balls' places (30:0e81): each is checked against one
        // word only, the list's first at first; the loop meant to walk the
        // list moves its start on instead (30:0ea6), 27 words for each
        // ball let in. A -1 there lets any place in.
        uint16_t check = shapeList;
        for (int n = 0; n < extra; ++n) {
            int at;
            for (;;) {
                at = random(kSpots);
                const int16_t w = word(check);
                if (w == -1) break;
                if (at != w) {
                    check = static_cast<uint16_t>(check + 2 * kSpots);
                    break;
                }
            }
            balls.push_back(spots[static_cast<size_t>(at)]);
            colour.push_back(colours == 1 ? 0 : random(colours));
        }
        if (first) shapeCount = static_cast<int>(balls.size());
        return colour;
    };
    auto render = [&](bool view, const std::vector<int>& colour, bool recolour) {  // g30_0082
        select(2);
        if (view) duplicateArea(2, 2, 0, 0, kViewW, kViewH, kViewX, kViewY);
        else fill(kShotX, kShotY, kShotW, kShotH, 0x7A);
        int16_t m[9];
        rotation(turnA, 0, turnC, m);
        // The frame's edges first.
        std::vector<Point3> f = frame;
        transform(f, m);
        translate(f, 0, 0x300, 0);
        const auto corners = project(f);
        static constexpr int kEdges[12][2] = {{0, 1}, {1, 5}, {4, 5}, {0, 4}, {2, 3}, {3, 7}, {6, 7}, {2, 6}, {0, 2}, {4, 6}, {5, 7}, {1, 3}};
        if (corners.size() == 8)
            for (const auto& e : kEdges)
                line(corners[static_cast<size_t>(e[0])].first, corners[static_cast<size_t>(e[0])].second,
                     corners[static_cast<size_t>(e[1])].first, corners[static_cast<size_t>(e[1])].second, 0);
        // Then the balls, far ones first and smaller.
        std::vector<Point3> pts = balls;
        transform(pts, m);
        translate(pts, 0, 0x300, 0);
        std::vector<int> col = colour;
        const size_t n = pts.size();
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j + 1 < n - i; ++j)
                if (pts[j].y < pts[j + 1].y) {
                    std::swap(pts[j], pts[j + 1]);
                    std::swap(col[j], col[j + 1]);
                }
        if (n == 0) return;
        const int far = pts[0].y, spread = std::abs(far - pts[n - 1].y);
        const int step = spread ? 0x20 / spread : 0;
        const auto flat = project(pts);
        if (recolour && !flat.empty()) {
            for (int k = random(static_cast<int>(flat.size()) >> 1) + 1; k > 0; --k) {
                const size_t i = static_cast<size_t>(random(static_cast<int>(flat.size())));
                int c;
                do c = random(6);
                while (c == col[i]);
                col[i] = c;
            }
        }
        for (size_t i = 0; i < flat.size() && i < n; ++i) {
            const int s = std::min((far - pts[i].y) * step, 0x20) + 0xE0;
            drawScaledCentred(flat[i].first, flat[i].second, s, s, static_cast<uint16_t>(0x2077 + col[i]));
        }
        if (view) {
            copyArea(2, 1, kViewX, kViewY, kViewW, kViewH);
            select(1);
        }
    };
    auto thumbnail = [&](int slot) {  // g30_0f98: the shot at half size in a box
        std::vector<uint8_t> shot(static_cast<size_t>(kShotW) * kShotH);
        const Screen& s2 = ctx_.screens[2];
        for (int r = 0; r < kShotH; ++r)
            for (int c = 0; c < kShotW; ++c)
                shot[static_cast<size_t>(r) * kShotW + c] = s2.pixels[static_cast<size_t>(kShotY + r) * Screen::kWidth + kShotX + c];
        // The library's scaler (f41_0024).
        constexpr int kScaleX = 0x80, kScaleY = 0x7E;
        const int w = kShotW * kScaleX / 256, h = kShotH * kScaleY / 256;
        const uint32_t stepX = scaleStep(kScaleX), stepY = scaleStep(kScaleY);
        const int cx = kBoxesX + kBox[slot][0] + kBox[slot][2] / 2, cy = kBoxesY + kBox[slot][1] + kBox[slot][3] / 2;
        Screen& dst = ctx_.screens[2];
        for (int r = 0; r < h; ++r)
            for (int c = 0; c < w; ++c)
                dst.pixels[static_cast<size_t>(cy - h / 2 + r) * Screen::kWidth + cx - w / 2 + c] =
                    shot[static_cast<size_t>(r * stepY >> 16) * kShotW + (c * stepX >> 16)];
        copyArea(2, 1, cx - kShotW / 2, cy - kShotH / 2, kShotW, kShotH);
    };

    // The screen, the balls' colours (resource 219: screen 2's 0xB8-0xD3).
    clearInput();
    ctx_.blackout();
    ctx_.showFullScreen(0x1002, 2);
    {
        std::vector<uint8_t> raw;
        if (ctx_.read(0x219, raw))
            for (size_t k = 0; k + 2 < raw.size() && k / 3 < 28; k += 3)
                ctx_.screens[2].palette[0xB8 + k / 3] = Rgb{raw[k + 2], raw[k + 1], raw[k]};
    }
    select(2);
    drawOpaque(0x15E, 0x162, 0x2251);
    text(0x132, 0x168, "5", 0x9D);
    intBox(0x90, 0x3E, 0x2C, 0x14, points);
    auto showTime = [&] {
        if (timeLeft == timeShown) return;
        digitalTime(0x1AA, 0x42, 0x28, 0x14, timeLeft);
        timeShown = timeLeft;
    };
    showTime();

    bool quit = false, redraw = false, gadget = false, roundDone = false;
    int right = 0, pressed = -1;
    bool answered[4] = {};
    panels_.clear();
    Panels::Panel arrows;  // DS:6708
    arrows.x = kArrowsX, arrows.y = kArrowsY, arrows.w = 0x62, arrows.h = 0x56;
    for (const auto& a : kArrow) arrows.buttons.push_back({a[0], a[1], a[2], a[3]});
    auto arrowAt = [&](int k, uint16_t id) {
        drawCentred(kArrowsX + kArrow[k][0] + kArrowMid[k][0], kArrowsY + kArrow[k][1] + kArrowMid[k][1], id);
    };
    arrows.onPress = [&](int k) {  // g30_067e
        if (k < 0) return;
        music(0x12);
        arrowAt(k, static_cast<uint16_t>(0x2071 + k));
    };
    arrows.onRelease = [&](int k) {  // g30_0758
        if (k >= 0) arrowAt(k, static_cast<uint16_t>(0x206D + k));
    };
    arrows.whileHeld = [&](int k) {  // g30_0828
        if (k == 0) turnA = static_cast<uint16_t>(turnA - 0x180);
        else if (k == 1) turnA = static_cast<uint16_t>(turnA + 0x180);
        else if (k == 2) turnC = static_cast<uint16_t>(turnC - 0x180);
        else if (k == 3) turnC = static_cast<uint16_t>(turnC + 0x180);
        if (k >= 0) redraw = true;
    };
    panels_.add(arrows);
    Panels::Panel boxes;  // DS:674A
    boxes.x = kBoxesX, boxes.y = kBoxesY, boxes.w = 0xC6, boxes.h = 0x9C;
    for (const auto& b : kBox) boxes.buttons.push_back({b[0], b[1], b[2], b[3]});
    boxes.onPress = [&](int k) {  // g30_0988 (g30_08ae: the tick or the cross)
        if (k < 0 || answered[k]) return;
        pressed = k;
        drawCentred(kBoxesX + kBox[k][0] + kBox[k][2] / 2, kBoxesY + kBox[k][1] + kBox[k][3] / 2, k == right ? 0x2081 : 0x2082);
        if (k != right) ++misses;
    };
    boxes.onRelease = [&](int k) {  // g30_09fe
        if (k < 0 || k != pressed || answered[k]) return;
        if (k == right) {
            music(0x25);
            roundDone = true;
            points += 0x32;
        } else {
            music(0x26);
            points -= 0x32;
        }
        intBox(0x90, 0x3E, 0x2C, 0x14, points);
        answered[k] = true;
        pressed = -1;
    };
    panels_.add(boxes);
    Panels::Panel exit;  // DS:66C6
    exit.x = 0x218, exit.y = 0x34, exit.w = 0x28, exit.h = 0x20;
    exit.buttons = {{0, 0, 0x28, 0x20}};
    exit.onPress = [&](int k) {
        if (k >= 0) quit = true;
    };
    panels_.add(exit);
    Panels::Panel machine;  // DS:676E
    machine.x = 0x15E, machine.y = 0x162, machine.w = 0x2A, machine.h = 0x20;
    machine.buttons = {{0, 0, 0x2A, 0x20}};
    machine.onPress = [&](int k) {
        if (k >= 0) gadget = true;
    };
    panels_.add(machine);
    setView(0xAA, 0x54, 0x1D6, 0x110);
    helpPanel(0xC, 0x22, 0x4A, 0x2A);

    while (left > 0 && !quit) {
        // A new sculpture (one of the level's six shapes) and the four
        // pictures (g30_11aa): the right one, the others changed.
        for (bool& a : answered) a = false;
        roundDone = false;
        shape.clear();
        ballColour.clear();
        shapeList = dataWord(static_cast<uint16_t>(row + 1 + 2 * random(6)));
        for (uint16_t at = shapeList; word(at) != -1 && shape.size() < kSpots; at = static_cast<uint16_t>(at + 2))
            shape.push_back(word(at));
        right = random(4);
        select(1);
        if (left == 5) {  // f04_005c(2, 4) after the shape's and answer's draws (its stir too)
            show(2);
            // (No f06_01f6 here: the UI colours stay the previous screen's.)
            select(1);
            // The empty view is kept at the top left of screen 2 (once shown).
            duplicateArea(2, 2, kViewX, kViewY, kViewW, kViewH, 0, 0);
        }
        turnA = 0x1000, turnC = 0x1A00;
        auto colour = build(0);
        render(false, colour, false);
        thumbnail(right);
        for (int k = 0; k < 4; ++k) {
            if (k == right) continue;
            const int m = random(2);
            const auto changed = build(m);
            turnA = 0x1000, turnC = 0x1A00;
            render(false, changed, m == 0);
            thumbnail(k);
        }
        colour = build(0);
        select(1);
        turnA = static_cast<uint16_t>(random(0x10000));
        turnC = static_cast<uint16_t>(random(0x10000));
        render(true, colour, false);
        // The clock's second starts again with each sculpture (30:15FB).
        ctx_.timer.setPeriodic(kSecondSlot, 1, [&] {  // g30_0026
            if (timeLeft > 0) --timeLeft;
        });
        while (!roundDone && !quit) {
            panels_.poll(ctx_.platform);
            ctx_.pump();
            showTime();
            if (redraw) {
                redraw = false;
                render(true, colour, false);
            }
            if (gadget) {
                gadget = false;
                cubeGadget();
            }
            idleHint(3);
            if (helpPressed_) {
                select(1);
                drawCentred(0x31, 0x37, 0x207F);
                help(0x3F04);
                drawCentred(0x31, 0x37, 0x2080);
            }
        }
        if (!roundDone) break;
        --left;
        select(1);
        fill(0x132, 0x168, 0x10, 0x10, 0xB2);
        text(0x132, 0x168, std::to_string(left), 0x9D);
    }
    const bool won = left == 0;
    int used = 0;
    if (won) {
        used = limit - timeLeft;
        if (misses <= 5) {
            misses = 0;
            points += 100;
            while (timeLeft > 0) {
                --timeLeft;
                showTime();
                points += 2;
                intBox(0x90, 0x3E, 0x2C, 0x14, points);
            }
        } else {
            misses = 2;
        }
        timeLeft = limit - used;
        showTime();
    }
    drawCentred(0x22C, 0x44, 0x2075);
    if (quit) {
        points = 0;
        waitCountdown(6);
    }
    ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);
    panels_.clear();
    clearInput();
    puzzleResult(won, points, misses, used);
    select(1);
    fill(0, 0, Screen::kWidth, Screen::kHeight, 0);
    return won;
}

}  // namespace edison
