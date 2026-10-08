// Liberty Planetarium (segment 28): a constellation's stars turn in 3D in
// the dome; pick the flat picture of it from the four at the bottom right.
// Five rounds.

#include <algorithm>

#include "mystery/mystery.h"

namespace edison {

namespace {

constexpr int kSecondSlot = 6;
constexpr int kConstellations = 16;
constexpr uint16_t kRecords = 0x5F08;  // 13 bytes each
constexpr int kDomeX = 0xF0, kDomeY = 0x28, kDomeW = 0xB4, kDomeH = 0xA0;
// Panel DS:909C: the four pictures.
constexpr int kBoxesX = 0x1C0, kBoxesY = 0xEB, kBoxW = 0x50, kBoxH = 0x3A;
constexpr int kBox[4][2] = {{0, 0}, {0x58, 0}, {0, 0x46}, {0x58, 0x46}};
// Panel DS:50BC: the arrows that turn the stars (DS:5094).
constexpr int kArrowsX = 0x3F, kArrowsY = 0x115;
constexpr int kArrow[4][4] = {{0x15, 5, 0x28, 0x1A}, {0x31, 0x43, 0x28, 0x18}, {5, 0x29, 0x28, 0x1A}, {0x41, 0x23, 0x20, 0x1C}};

}  // namespace

bool Mystery::planetarium(int level) {
    // g28_178e / g28_1218 / g28_13d0.
    const int mode = std::clamp(level, 0, 2);  // [903A]
    const int limit = mode == 0 ? 0x78 : mode == 1 ? 0xB4 : 0xF0;  // [930E]
    int timeLeft = limit;   // [9028]
    bool timeShown = true;  // [902A]
    int points = 0;         // [903C]
    int misses = 0;         // [9070]
    int rounds = 5;         // [9038]
    auto word = [&](uint16_t at) { return static_cast<int16_t>(dataWord(at)); };
    auto record = [](int c) { return static_cast<uint16_t>(kRecords + 13 * c); };

    // The first time, each star's x and y trade places and it gets a depth
    // of 10-14 either side (g28_1218, [504E]).
    if (stars_.empty()) {
        for (int c = 0; c < kConstellations; ++c) {
            const uint16_t r = record(c), pts = dataWord(r + 1);
            std::vector<Point3> s;
            for (int k = 0; k < data_[r]; ++k) {
                const uint16_t at = static_cast<uint16_t>(pts + 6 * k);
                const int16_t z = static_cast<int16_t>(random(10) > 5 ? -(random(5) + 10) : random(5) + 10);
                s.push_back({word(at + 2), word(at), z});
            }
            stars_.push_back(s);
        }
    }
    auto magnitude = [&](int c, int k) {
        const int m = static_cast<int8_t>(data_[static_cast<uint16_t>(dataWord(record(c) + 5) + k)]);
        return m < 0 || m > 2 ? 0 : m;
    };

    int shown = 0;         // [902E]: the constellation
    int right = 0;         // [903E]: its box
    bool tried[4] = {};    // DS:9030
    bool lines = false;    // [9072]
    uint16_t turnA = 0, turnC = 0;  // [9396], [939A]
    bool pool[3][10] = {};  // DS:9040, 9052, 9060: the ones used

    auto drawDome = [&] {  // g28_0460
        const int previous = current();
        select(2);
        fill(kDomeX, kDomeY, kDomeW, kDomeH, 0);
        int16_t m[9];
        rotation(turnA, 0, turnC, m);
        std::vector<Point3> pts = stars_[shown];
        transform(pts, m);
        translate(pts, 0, 0x280, 0);
        const auto flat = project(pts);
        for (size_t k = 0; k < flat.size(); ++k)
            drawLogo(flat[k].first, flat[k].second - 0x50, static_cast<uint16_t>(0x22D2 + magnitude(shown, static_cast<int>(k))));
        if (lines) {
            for (uint16_t at = dataWord(record(shown) + 3); word(at) != -1; at = static_cast<uint16_t>(at + 4)) {
                const size_t a = static_cast<size_t>(word(at)), b = static_cast<size_t>(word(at + 2));
                if (a < flat.size() && b < flat.size())
                    line(flat[a].first, flat[a].second - 0x50, flat[b].first, flat[b].second - 0x50, 0xFF);
            }
        }
        copyArea(2, 1, kDomeX, kDomeY, kDomeW, kDomeH);
        select(previous);
    };
    auto dots = [&](int cx, int cy, int c, bool moveOne) {  // g28_068c: the stars flat, one maybe moved
        std::vector<Point3> s = stars_[c];
        if (moveOne && !s.empty()) {
            Point3& p = s[static_cast<size_t>(random(static_cast<int>(s.size())))];
            p.x = static_cast<int16_t>(random(0x32));
            p.y = static_cast<int16_t>(random(0x32));
            p.z = static_cast<int16_t>(random(10));
        }
        for (size_t k = 0; k < s.size(); ++k) {
            const int x = s[k].x / 4 + cx, y = s[k].y / 4 + cy;
            if (mode >= 1 && magnitude(c, static_cast<int>(k)) > 1)
                fill(x, y, 2, 2, 0xFF);
            else if (x >= 0 && y >= 0 && x < Screen::kWidth && y < Screen::kHeight)
                ctx_.screens[current()].pixels[static_cast<size_t>(y) * Screen::kWidth + x] = 0xFF;
        }
    };
    auto drawBoxes = [&] {  // g28_084a
        select(2);
        std::vector<int> taken = {shown};
        auto another = [&] {  // one not shown yet (the Big Dipper and Ursa Minor go together)
            int c;
            do c = random(kConstellations);
            while (std::find(taken.begin(), taken.end(), c) != taken.end());
            taken.push_back(c);
            if (c == 3) taken.push_back(4);
            if (c == 4) taken.push_back(3);
            return c;
        };
        int wrong = 0;
        for (int b = 0; b < 4; ++b) {
            const int x = kBoxesX + kBox[b][0], y = kBoxesY + kBox[b][1];
            if (mode == 0) {
                drawLogo(x, y, dataWord(record(b == right ? shown : another()) + 0xB));
            } else {
                fill(x, y, kBoxW, kBoxH, 0);
                if (b == right) dots(x + kBoxW / 2, y + kBoxH / 2, shown, false);
                else if (mode == 1 && ++wrong <= 2) dots(x + kBoxW / 2, y + kBoxH / 2, another(), false);
                else dots(x + kBoxW / 2, y + kBoxH / 2, shown, true);
            }
            copyArea(2, 1, x, y, kBoxW, kBoxH);
        }
        select(1);
    };
    auto pickConstellation = [&] {  // g28_0e72
        static constexpr int kPools[3][10] = {{0, 1, 2, 3, 4, 8, 9, 10, 11, 6}, {5, 11, 12, 13, 0, 2, 10}, {5, 6, 7, 12, 13, 14, 15, 0}};
        static constexpr int kSizes[3] = {10, 7, 8};
        int i;
        do i = random(kSizes[mode]);
        while (pool[mode][i]);
        pool[mode][i] = true;
        shown = kPools[mode][i];
        for (bool& t : tried) t = false;
        right = random(4);
    };
    auto nameBox = [&](const std::string& name) {  // g28_1010
        select(2);
        fill(0xE2, 0x107, 0xB8, 0x46, 0);
        text((0xB8 - font_->width(name)) / 2 + 0xE2, 0x122, name, 0xFF);
        copyArea(2, 1, 0xE2, 0x107, 0xB8, 0x46);
        select(1);
    };
    auto lines_at = [&](uint16_t at) {
        std::vector<std::string> out;
        for (;;) {
            const std::string l = dataString(at);
            if (l.empty() || out.size() >= 8) break;
            out.push_back(l);
            at = static_cast<uint16_t>(at + l.size() + 1);
        }
        return out;
    };
    auto answer = [&](int b) {  // g28_10a8
        select(1);
        const int x = kBoxesX + kBox[b][0], y = kBoxesY + kBox[b][1];
        if (b == right) {
            music(0x25);
            points += 0x32;
            drawLogo(x, y, 0x20A3);
            nameBox(dataString(dataWord(record(shown) + 7)));
            messageBox(lines_at(dataWord(dataWord(record(shown) + 9))));
            select(2);
            fill(0xE2, 0x107, 0xB8, 0x46, 0);
            copyArea(2, 1, 0xE2, 0x107, 0xB8, 0x46);
            select(1);
            tried[b] = true;
            return true;
        }
        music(0x26);
        points -= 0x32;
        drawLogo(x, y, 0x20A4);
        ++misses;
        const int fact = word(static_cast<uint16_t>(0x5050 + 2 * shown));
        if (fact >= 0 && fact < 10) learned_[4][fact] = true;  // g19_0064, row [B38C] = 4
        return false;
    };

    // The panels.
    bool quit = false, helpWanted = false, gadget = false, redraw = false;
    int picked = -1;  // [90B6]
    panels_.clear();
    Panels::Panel exit;  // f06_23d8
    exit.x = 0x210, exit.y = 8, exit.w = 100, exit.h = 100;
    exit.buttons = {{0, 0, 100, 100}};
    exit.onPress = [&](int k) {
        if (k >= 0) quit = true;
    };
    panels_.add(exit);
    Panels::Panel helpButton;  // f06_2436
    helpButton.x = 0xE, helpButton.y = 0x78, helpButton.w = 100, helpButton.h = 100;
    helpButton.buttons = {{0, 0, 100, 100}};
    helpButton.onPress = [&](int k) {
        if (k >= 0) helpWanted = true;
    };
    panels_.add(helpButton);
    Panels::Panel arrows;
    arrows.x = kArrowsX, arrows.y = kArrowsY, arrows.w = 0x64, arrows.h = 0x5C;
    for (const auto& a : kArrow) arrows.buttons.push_back({a[0], a[1], a[2], a[3]});
    arrows.onPress = [&](int k) {  // g28_028c
        if (k >= 0) drawLogo(kArrowsX + kArrow[k][0], kArrowsY + kArrow[k][1], static_cast<uint16_t>(0x22D6 + k));
    };
    arrows.onRelease = [&](int k) {  // g28_02f8
        if (k >= 0) drawLogo(kArrowsX + kArrow[k][0], kArrowsY + kArrow[k][1], static_cast<uint16_t>(0x22DA + k));
    };
    arrows.whileHeld = [&](int k) {  // g28_0364
        if (k == 0) turnA = static_cast<uint16_t>(turnA - 0x180);
        else if (k == 1) turnA = static_cast<uint16_t>(turnA + 0x180);
        else if (k == 2) turnC = static_cast<uint16_t>(turnC - 0x180);
        else if (k == 3) turnC = static_cast<uint16_t>(turnC + 0x180);
        if (k >= 0) redraw = true;
    };
    panels_.add(arrows);
    Panels::Panel boxes;  // DS:909C
    boxes.x = kBoxesX, boxes.y = kBoxesY, boxes.w = 0xA8, boxes.h = 0x7F;
    for (const auto& b : kBox) boxes.buttons.push_back({b[0], b[1], kBoxW, kBoxH});
    boxes.onPress = [&](int k) {  // g28_0260
        if (k >= 0) picked = k;
    };
    panels_.add(boxes);
    Panels::Panel dome;  // DS:507A: a click shows or hides the lines
    dome.x = kDomeX, dome.y = kDomeY, dome.w = kDomeW, dome.h = kDomeH;
    dome.buttons = {{0, 0, kDomeW, kDomeH}};
    dome.onPress = [&](int k) {  // g28_03ea
        if (k >= 0 && mode != 2) {
            lines = !lines;
            redraw = true;
        }
    };
    panels_.add(dome);
    Panels::Panel machine;  // DS:5FE2
    machine.x = 0x26, machine.y = 0x2E, machine.w = 0x36, machine.h = 0x16;
    machine.buttons = {{0, 0, 0x36, 0x16}};
    machine.onPress = [&](int k) {  // g28_0026
        if (k >= 0) gadget = true;
    };
    panels_.add(machine);
    setView(0xF0, 0x28, 0x1A4, 400);
    ctx_.timer.setPeriodic(kSecondSlot, 1, [&] {  // g28_0000
        if (timeLeft > 0) --timeLeft;
        timeShown = true;
    });

    clearInput();
    backdrop(0x1007);
    select(2);
    drawLogo(0x26, 0x2E, 0x2267);
    for (int k = 0; k < 4; ++k) drawLogo(kArrowsX + kArrow[k][0], kArrowsY + kArrow[k][1], static_cast<uint16_t>(0x22DA + k));
    show(2);
    computeUiColours();
    select(1);
    intBox(0x154, 0x168, 0x3C, 0x14, points);
    music(0x1E);

    bool won = false;
    while (!quit) {
        intBox(0xE0, 0x168, 0x3C, 0x14, rounds);
        if (--rounds < 0) {
            won = true;
            break;
        }
        pickConstellation();
        lines = mode == 0;
        drawBoxes();
        turnA = static_cast<uint16_t>(random(0x8000) + 0x4000);
        turnC = static_cast<uint16_t>(random(0x8000) + 0x4000);
        drawDome();
        bool solved = false;
        while (!solved && !quit) {
            panels_.poll(ctx_.platform);
            ctx_.pump();
            if (picked >= 0) {
                if (!tried[picked]) solved = answer(picked);
                intBox(0x154, 0x168, 0x3C, 0x14, points);
                picked = -1;
            }
            if (redraw) {
                redraw = false;
                drawDome();
            }
            if (timeShown) {
                timeShown = false;
                digitalTime(0x96, 0xEC, 0x3C, 0x14, timeLeft);
            }
            if (helpWanted) {
                helpWanted = false;
                select(1);
                drawLogo(0xE, 0x78, 0x22DE);
                messageBox(lines_at(0x5FFB));  // g28_0430
                drawLogo(0xE, 0x78, 0x22DF);
            }
            if (gadget) {
                // g28_0054: the projector.
                gadget = false;
                music(0x1F);
                select(1);
                drawLogo(0x26, 0x2E, 0x2268);
                for (int f = 0; f < 0x1B; ++f) {
                    drawOpaque(0x1BC, 0x54, static_cast<uint16_t>(0x2269 + f));
                    waitCountdown(f == 6 || f == 0xB || f == 0xD || f == 0xF || f == 0x11 || f == 0x17 ? 6 : 2);
                }
                drawLogo(0x26, 0x2E, 0x2267);
                fill(0x1BC, 0x54, 0x5C, 0x42, 0);
            }
        }
    }
    ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);  // g28_174c
    panels_.clear();
    if (!won) {
        puzzleResult(false, 0, 0, 0);
        return false;
    }
    const int used = limit - timeLeft;  // [B794]
    int result = 2;
    if (misses <= 5) {
        // 100 points, and 2 for each second left.
        result = 0;
        points += 100;
        while (timeLeft > 0) {
            --timeLeft;
            digitalTime(0x96, 0xEC, 0x3C, 0x14, timeLeft);
            points += 2;
            intBox(0x154, 0x168, 0x3C, 0x14, points);
        }
    }
    digitalTime(0x96, 0xEC, 0x3C, 0x14, limit - used);
    puzzleResult(true, points, result, used);
    return true;
}

}  // namespace edison
