// Stackup (segment 25): four bars on the left of each row, and three
// pictures of bars stacked up on the right; pick the one that stacks the
// four in order, left to right.

#include <algorithm>
#include <utility>

#include "mystery/mystery.h"

namespace edison {

namespace {

constexpr int kSecondSlot = 6;
constexpr int kItemsX = 0x5F, kChoicesX = 0x18B, kRowY = 0x3C, kRowStep = 0x37, kItemStep = 0x3C, kChoiceStep = 0x46;
constexpr int kPanelX = 0x168, kPanelY = 0x23;

}  // namespace

void Mystery::monitorGadget() {
    // g27_0f16: the gadget at the bottom of the monitor.
    static constexpr int kFrames[] = {0, 1, 2, -1, 3, 4, 5, -1, 6, 7, -1, 6, 8, -1, 3, 2, 1, 0};
    const int previous = current();
    select(1);
    drawLogo(0x12C, 0x162, 0x21D8);
    for (int f : kFrames) {
        if (f >= 0) drawOpaque(0x1C, 0x144, static_cast<uint16_t>(0x21CF + f));
        waitCountdown(f >= 0 ? 1 : 2);
    }
    drawOpaque(0x12C, 0x162, 0x21D9);
    select(previous);
}

bool Mystery::stackup(int level) {
    // g25_1f6a / g25_1aa0.
    const int mode = std::clamp(level, 0, 2) + 1;  // [8C42]
    const int limit = 0x78;                        // [930E]
    int timeLeft = limit;                          // [8006]:2
    bool timeShown = false;                        // [8006]:4
    int points = 0;                                // [8ABC]
    int misses = 0;                                // [8C46]
    bool quit = false, helpWanted = false, gadget = false;
    int picked = -1;                               // [8CF8] when [8CFA]

    // The ten bars (DS:4BFE, 0x14 apart): three upright, three across and
    // four slanting.
    std::vector<std::pair<int, int>> bars[10];
    uint8_t barColour[10];
    for (int s = 0; s < 10; ++s) {
        uint16_t at = static_cast<uint16_t>(0x4BFE + s * 0x14);
        for (; static_cast<int16_t>(dataWord(at)) < 0x500; at = static_cast<uint16_t>(at + 4))
            bars[s].emplace_back(static_cast<int16_t>(dataWord(at)), static_cast<int16_t>(dataWord(at + 2)));
        barColour[s] = static_cast<uint8_t>(dataWord(static_cast<uint16_t>(at + 2)));
    }
    auto drawBar = [&](int x, int y, int bar, int colour) {  // g25_0190
        std::vector<std::pair<int, int>> pts = bars[bar];
        for (auto& p : pts) p.first += x, p.second += y;
        // draw_poly's lists (f32_2e46): the bar, then its outline as
        // two-point polygons of colour FF.
        fillPolygonSolid(pts, colour ? static_cast<uint8_t>(colour) : barColour[bar]);
        for (size_t k = 0; k < pts.size(); ++k) fillPolygonSolid({pts[k], pts[(k + 1) % pts.size()]}, 0xFF);
    };

    // DS:8AE6: a row.
    struct Row {
        bool solved = false;
        int bars[4] = {};                    // the four on the left
        int choice[3][4] = {}, colour[3][4] = {};  // the stacks, bottom first (colour 0: the bar's own)
        int right = 0;
    } rows[5];

    auto generate = [&](Row& r) {  // g25_0c8a
        // One bar from each group (0-2, 3-5, 6-7, 8-9), in a random order.
        struct Group { int count, base; bool used; } groups[4] = {{3, 0, false}, {3, 3, false}, {2, 6, false}, {2, 8, false}};
        for (int k = 0; k < 4; ++k) {
            int g;
            do {
                std::vector<int> free;
                for (int i = 0; i < 4; ++i)
                    if (!groups[i].used) free.push_back(i);
                g = free[random(static_cast<int>(free.size()))];
            } while (groups[g].used);
            groups[g].used = true;
            r.bars[k] = random(groups[g].count) + groups[g].base;
        }
        // g25_03b2: the 24 orders of the four (the first is as given).
        static constexpr int kOrders[24][4] = {
            {0, 1, 2, 3}, {3, 2, 0, 1}, {0, 3, 2, 1}, {3, 0, 1, 2}, {0, 1, 3, 2}, {1, 0, 3, 2}, {2, 0, 3, 1}, {3, 0, 2, 1},
            {1, 3, 2, 0}, {2, 3, 1, 0}, {3, 2, 1, 0}, {3, 1, 0, 2}, {0, 2, 3, 1}, {1, 2, 3, 0}, {2, 1, 3, 0}, {3, 1, 2, 0},
            {0, 3, 1, 2}, {1, 3, 0, 2}, {2, 3, 0, 1}, {1, 0, 2, 3}, {2, 0, 1, 3}, {0, 2, 1, 3}, {1, 2, 0, 3}, {2, 1, 0, 3},
        };
        int orders[24][4];
        for (int o = 0; o < 24; ++o)
            for (int k = 0; k < 4; ++k) orders[o][k] = r.bars[kOrders[o][k]];
        // g25_0adc: two wrong orders (1-23) and the right one (0), placed at random.
        int which[3] = {-1, -1, -1};
        int wrong[3], n = 0;
        while (n < 3) {
            const int o = random(0x17) + 1;
            if (std::find(wrong, wrong + n, o) == wrong + n) wrong[n++] = o;
        }
        which[random(3)] = 0;
        for (int c = 0, w = 0; c < 3; ++c)
            if (which[c] < 0) which[c] = wrong[w++];
        static constexpr int kShades[] = {4, 6, 8, 10, 10, 10};
        for (int c = 0; c < 3; ++c) {
            random(3);
            const int shade = random(kShades[mode]);
            for (int k = 0; k < 4; ++k) {
                r.colour[c][k] = 0;
                if (which[c] == 0) {
                    r.choice[c][k] = orders[0][k];
                } else if (mode == 1) {
                    // Wrong ones at the easiest level: any bars, all one colour.
                    r.colour[c][k] = shade + 0x90;
                    r.choice[c][k] = random(shade);
                } else if (mode == 2) {
                    r.choice[c][k] = orders[random(0x18)][k];
                } else {
                    r.choice[c][k] = orders[which[c]][k];
                }
            }
            if (which[c] == 0) r.right = c;
        }
    };
    auto drawRow = [&](int i) {  // g25_11b0
        const Row& r = rows[i];
        const int y = i * kRowStep + kRowY;
        for (int k = 0; k < 4; ++k) drawBar(k * kItemStep + kItemsX, y, r.bars[k], 0);
        for (int c = 0; c < 3; ++c)
            for (int k = 0; k < 4; ++k) drawBar(c * kChoiceStep + kChoicesX, y, r.choice[c][k], r.colour[c][k]);
    };
    auto reveal = [&](int i) {  // g25_12de: the bars slide left onto the first, stacking up
        const Row& r = rows[i];
        const int ty = i * kRowStep + kRowY, by = ty - 0x19 + 1;
        for (int k = 1; k < 4; ++k) {
            int x = k * kItemStep + kItemsX - 0x1E + 1, cx = k * kItemStep + kItemsX;
            select(2);
            fill(x, by, 0x36, 0x31, 0);
            copyArea(2, 1, x, by, 0x36, 0x31);
            select(1);
            for (int step = 0; x > kItemsX - 0x1E; ++step) {
                x -= 2, cx -= 2;
                const int saved = saveArea(x, by, 0x36, 0x31);
                drawBar(cx, ty, r.bars[k], 0);
                if (step % 3 == 0) ctx_.pump();  // the original's short busy-wait
                restoreArea(saved);
            }
            select(2);
            drawBar(kItemsX, ty, r.bars[k], 0);
            copyArea(2, 1, kItemsX - 0x1E + 1, by, 0x36, 0x31);
        }
        select(1);
    };
    auto answer = [&](int row, int c) {  // g25_1688
        Row& r = rows[row];
        select(1);
        const int bx = kPanelX + c * kChoiceStep, by = kPanelY + row * kRowStep;
        if (r.right == c) {
            music(0x25);
            points += 0x32;
            intBox(0xCC, 0x14E, 0x60, 0x19, points);
            r.solved = true;
            drawLogo(bx, by, 0x20A3);
        } else {
            music(0x26);
            points -= 0x32;
            intBox(0xCC, 0x14E, 0x60, 0x19, points);
            drawLogo(bx, by, 0x20A4);
            reveal(row);
            r.solved = true;
            ++misses;
        }
    };
    auto allSolved = [&] {  // g25_190e
        for (const Row& r : rows)
            if (!r.solved) return false;
        return true;
    };

    // g25_1954 / g25_0000 / g25_15ee: the panels.
    panels_.clear();
    Panels::Panel stacks;  // DS:8CDE
    stacks.x = kPanelX, stacks.y = kPanelY, stacks.w = 0xD2, stacks.h = 0x113;
    for (int i = 0; i < 5; ++i)
        for (int c = 0; c < 3; ++c) stacks.buttons.push_back({c * kChoiceStep, i * kRowStep, 0x46, 0x32});
    stacks.onPress = [&](int k) {  // g25_1518
        if (k >= 0) picked = k;
    };
    panels_.add(stacks);
    Panels::Panel exit;  // f06_23d8
    exit.x = 0x1FF, exit.y = 0x15F, exit.w = 0x44, exit.h = 0x1C;
    exit.buttons = {{0, 0, 0x44, 0x1C}};
    exit.onPress = [&](int k) {
        if (k >= 0) quit = true;
    };
    panels_.add(exit);
    Panels::Panel helpButton;  // f06_2436
    helpButton.x = 0x26, helpButton.y = 0x15F, helpButton.w = 0x44, helpButton.h = 0x1C;
    helpButton.buttons = {{0, 0, 0x44, 0x1C}};
    helpButton.onPress = [&](int k) {
        if (k >= 0) helpWanted = true;
    };
    panels_.add(helpButton);
    Panels::Panel machine;  // DS:B73C
    machine.x = 0x12C, machine.y = 0x162, machine.w = 0x38, machine.h = 0x24;
    machine.buttons = {{0, 0, 0x38, 0x24}};
    machine.onPress = [&](int k) {  // g06_0048
        if (k >= 0) gadget = true;
    };
    panels_.add(machine);

    clearInput();
    backdrop(0x1008);
    select(2);
    drawOpaque(0x12C, 0x162, 0x21D9);
    show(2);
    computeUiColours();
    for (int i = 0; i < 5; ++i) generate(rows[i]);
    for (int i = 0; i < 5; ++i) drawRow(i);
    // g25_17ca: the dividing lines and a box round each bar on the left.
    line(0x159, 0x1E, 0x159, 0x1E + 0x11C, 0xFF);
    line(0x161, 0x1E, 0x161, 0x1E + 0x11C, 0xFF);
    for (int i = 0; i < 5; ++i)
        for (int k = 0; k < 4; ++k) frame(k * kItemStep + kItemsX - 0x1E, i * kRowStep + kRowY - 0x19, 0x38, 0x33, 0xFF);
    copyArea(2, 1, 0, 0, Screen::kWidth, Screen::kHeight);
    intBox(0xCC, 0x14E, 0x60, 0x19, points);
    digitalTime(0x164, 0x14E, 0x60, 0x19, timeLeft);
    select(1);
    ctx_.timer.setPeriodic(kSecondSlot, 1, [&] {  // g25_00ba
        if (timeLeft > 0) --timeLeft;
        timeShown = true;
    });
    clearInput();

    bool done = false;
    while (!done && !quit) {
        panels_.poll(ctx_.platform);
        ctx_.pump();
        if (helpWanted) {
            helpWanted = false;
            select(1);
            drawLogo(0x26, 0x15F, 0x20A6);
            std::vector<std::string> text;  // g25_14e8: DS:4DC2's lines
            for (uint16_t at = 0x4CE9;;) {
                const std::string l = dataString(at);
                if (l.empty()) break;
                text.push_back(l);
                at = static_cast<uint16_t>(at + l.size() + 1);
            }
            messageBox(text);
            drawLogo(0x26, 0x15F, 0x20A5);
        }
        if (gadget) {
            gadget = false;
            music(0x19);
            monitorGadget();
        }
        if (picked >= 0) {
            const int row = picked / 3, c = picked % 3;  // g25_165a
            picked = -1;
            if (!rows[row].solved) answer(row, c);
            if (allSolved()) done = true;
        }
        if (timeShown) {
            timeShown = false;
            digitalTime(0x164, 0x14E, 0x60, 0x19, timeLeft);
        }
        if (done) {
            ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);
            const int used = limit - timeLeft;  // [B794]
            if (misses <= 2) {
                // 100 points, and 2 for each second left.
                misses = 0;
                points += 100;
                while (timeLeft > 0) {
                    --timeLeft;
                    digitalTime(0x164, 0x14E, 0x60, 0x19, timeLeft);
                    points += 2;
                    intBox(0xCC, 0x14E, 0x60, 0x19, points);
                }
            }
            digitalTime(0x164, 0x14E, 0x60, 0x19, limit - used);
            waitOrClick(10);
            panels_.clear();
            puzzleResult(true, points, misses, used);
        }
    }
    ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);
    panels_.clear();
    if (!done) puzzleResult(false, 0, 0, 0);
    return done;
}

}  // namespace edison
