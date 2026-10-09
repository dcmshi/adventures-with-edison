// What Comes Next (segment 26): five rows of four shapes, the last one a
// question mark; pick the one of four on the right that comes next.

#include <algorithm>
#include <utility>

#include "mystery/mystery.h"

namespace edison {

namespace {

constexpr int kSecondSlot = 6;
constexpr int kRowX = 0x5F, kAnswersX = 0x16D, kRowY = 0x3C, kRowStep = 0x37, kItemStep = 0x3C;
// Panel DS:9006: the answers, four to a row.
constexpr int kPanelX = 0x14F, kPanelY = 0x23;

// g26_0926: four values from 0..n-1 (plus base) in a pattern: 0 all the
// same, 1 alternating, 2 any, 3 all the previous call's last, 4 all
// different. Other modes leave the previous values.
void pattern(int v[4], int n, int base, int mode) {
    switch (mode) {
    case 0:
        v[0] = v[1] = v[2] = v[3] = random(n) + base;
        break;
    case 1:
        v[0] = v[2] = random(n) + base;
        do v[1] = v[3] = random(n) + base;
        while (v[1] == v[0]);
        break;
    case 2:
        for (int k = 0; k < 4; ++k) v[k] = random(n) + base;
        break;
    case 3:
        v[0] = v[1] = v[2] = v[3];
        break;
    case 4:
        v[0] = random(n) + base;
        do v[1] = random(n) + base;
        while (v[1] == v[0]);
        do v[2] = random(n) + base;
        while (v[2] == v[0] || v[2] == v[1]);
        do v[3] = random(n) + base;
        while (v[3] == v[0] || v[3] == v[1] || v[3] == v[2]);
        break;
    default:
        break;
    }
}

}  // namespace

bool Mystery::whatComesNext(int level) {
    // g26_19d8 / g26_153a.
    const int kind = std::clamp(level, 0, 5) + 1;  // [8F32]
    const int limit = 0x78;                        // [930E]
    int timeLeft = limit;                          // [8008]:0
    bool timeShown = false;                        // [8008]:2
    int points = 0;                                // [8CFC]
    int misses = 0;                                // [8F34]
    bool quit = false;                             // [91A2]
    bool helpWanted = false;                       // [B75A]
    bool gadget = false;                           // [92AE]
    int picked = -1;                               // [9020] when [9022]

    // The eight shapes (DS:4DDA...): big ones 0-3, small ones 4-7, points
    // around the centre.
    static constexpr uint16_t kShapes[8] = {0x4DDA, 0x4DEE, 0x4E12, 0x4E22, 0x4E36, 0x4E4A, 0x4E6E, 0x4E7E};
    std::vector<std::pair<int, int>> shapes[8];
    for (int s = 0; s < 8; ++s)
        for (uint16_t at = kShapes[s]; static_cast<int16_t>(dataWord(at)) < 0x500; at = static_cast<uint16_t>(at + 4))
            shapes[s].emplace_back(static_cast<int16_t>(dataWord(at)), static_cast<int16_t>(dataWord(at + 2)));

    // DS:8D6E: a row. Items 0-3 are the sequence (3 is the one asked for),
    // 4-7 the small shapes inside when there are two parts.
    struct Row {
        bool solved = false;
        int parts = 1;
        int shape[8] = {}, colour[8] = {};    // the sequence
        int ashape[8] = {}, acolour[8] = {};  // the four answers
        int right = -1;
    } rows[5];

    auto generate = [&](Row& r) {  // g26_0d50
        const int pick = random(4);
        int parts = 1, colours = 0, shapesMode = 0, shapes2 = 0, colours2 = 0, answerShapes = 0, answerColours = 0,
            answerShapes2 = 0, answerColours2 = 0;
        switch (kind) {
        case 1: parts = 1, colours = 0, shapesMode = 0, answerColours = 0, answerShapes = 0; break;
        case 2: parts = 1, colours = 0, shapesMode = 0, answerColours = 1, answerShapes = 1; break;
        case 3: parts = 1, colours = 1, shapesMode = 1, answerColours = 2, answerShapes = 2; break;
        case 4:
            parts = 2, colours = 0, shapesMode = 1, colours2 = 0, shapes2 = 1;
            answerColours = answerShapes = answerColours2 = 3, answerShapes2 = 4;
            break;
        case 5:
            parts = 2, colours = shapesMode = colours2 = 4, shapes2 = 1;
            answerColours = answerShapes = answerColours2 = answerShapes2 = 4;
            break;
        default:
            parts = 2, colours = shapesMode = colours2 = shapes2 = 4;
            answerColours = answerShapes = answerColours2 = answerShapes2 = 4;
            if (pick == 0) shapes2 = 1;
            else if (pick == 1) shapesMode = 1;
            else if (pick == 2) colours2 = 1;
            else colours = 1;
            break;
        }
        r.parts = parts;
        int v[4] = {};  // DS:8F36, kept between calls (modes 3 and above)
        pattern(v, 8, 0, colours);
        for (int k = 0; k < 4; ++k) r.colour[k] = v[k] + 0x90;
        pattern(v, 8, 0, answerColours);
        for (int k = 0; k < 4; ++k) r.acolour[k] = v[k] + 0x90;
        pattern(v, 4, 0, shapesMode);
        for (int k = 0; k < 4; ++k) r.shape[k] = v[k];
        pattern(v, 4, 0, answerShapes);
        for (int k = 0; k < 4; ++k) {
            if (v[k] == r.shape[3] && parts == 1) {  // none of them the answer yet
                pattern(v, 4, 0, answerShapes);
                k = -1;
            } else {
                r.ashape[k] = v[k];
            }
        }
        if (parts > 1) {
            pattern(v, 4, 4, shapes2);
            for (int k = 4; k < 8; ++k) r.shape[k] = v[k - 4];
            pattern(v, 4, 4, answerShapes2);
            for (int k = 4; k < 8; ++k) r.ashape[k] = v[k - 4];
            pattern(v, 8, 0, colours2);
            for (int k = 4; k < 8; ++k) {
                if (v[k - 4] + 0x90 == r.colour[k - 4]) {  // not the colour it sits on
                    pattern(v, 8, 0, colours2);
                    k = 3;
                } else {
                    r.colour[k] = v[k - 4] + 0x90;
                }
            }
            pattern(v, 8, 0, answerColours2);
            for (int k = 4; k < 8; ++k) {
                r.acolour[k] = v[k - 4] + 0x90;
                if ((r.acolour[k] == r.colour[3] && r.ashape[k] == r.shape[3]) || r.acolour[k] == r.acolour[k - 4]) {
                    pattern(v, 8, 0, answerColours2);
                    k = 3;
                }
            }
        }
        // Where the answer goes: an answer that already matches it.
        r.right = -1;
        for (int k = 0; k < 4; ++k) {
            if (kind == 5) {
                if (r.shape[7] != r.ashape[k + 4]) continue;
                r.right = k;
            }
            if (kind == 6) {
                const bool same = pick == 0   ? r.shape[7] == r.ashape[k + 4]
                                  : pick == 1 ? r.shape[3] == r.ashape[k]
                                  : pick == 2 ? r.colour[7] == r.acolour[k + 4]
                                              : r.colour[3] == r.acolour[k];
                if (!same) continue;
                r.right = k;
                break;
            }
            if (r.shape[3] != r.ashape[k] || r.colour[3] != r.acolour[k]) continue;
            if (parts < 2 || (r.shape[7] == r.ashape[k + 4] && r.colour[7] == r.acolour[k + 4])) {
                r.right = k;
                break;
            }
        }
        if (r.right < 0) r.right = random(4);
        r.ashape[r.right] = r.shape[3];
        r.acolour[r.right] = r.colour[3];
        if (parts > 1) {
            r.ashape[r.right + 4] = r.shape[7];
            r.acolour[r.right + 4] = r.colour[7];
        }
    };
    auto drawShape = [&](int shape, int colour, int x, int y) {  // g26_01e2
        std::vector<std::pair<int, int>> pts = shapes[shape];
        for (auto& p : pts) p.first += x, p.second += y;
        // draw_poly's lists (f32_2e46): the shape, then its outline as
        // two-point polygons of colour FF.
        fillPolygonSolid(pts, static_cast<uint8_t>(colour));
        for (size_t k = 0; k < pts.size(); ++k) fillPolygonSolid({pts[k], pts[(k + 1) % pts.size()]}, 0xFF);
    };
    auto drawRow = [&](int i) {  // g26_06ca
        const Row& r = rows[i];
        const int y = i * kRowStep + kRowY;
        for (int k = 0; k < r.parts * 4; ++k) {
            const int col = k % 4;
            if (col == 3)
                drawLogo(kRowX - 0x1E + col * kItemStep, y - 0x19, 0x2105);
            else
                drawShape(r.shape[k], r.colour[k], kRowX + col * kItemStep, y);
        }
        for (int k = 0; k < r.parts * 4; ++k)
            drawShape(r.ashape[k], r.acolour[k], kAnswersX + (k % 4) * kItemStep, y);
    };
    auto showTime = [&] {
        if (!timeShown) return;
        timeShown = false;
        digitalTime(0x164, 0x14E, 0x60, 0x19, timeLeft);
    };
    auto answer = [&](int row, int a) {  // g26_0aa8
        Row& r = rows[row];
        select(1);
        const int bx = kPanelX + a * kItemStep, by = kPanelY + row * kRowStep;
        const int y = row * kRowStep + kRowY;
        if (r.right == a) {
            music(0x25);
            points += 0x32;
            intBox(0xCC, 0x14E, 0x60, 0x19, points);
            r.solved = true;
            drawLogo(bx, by, 0x20A3);
            // The answer, from screen 2, in place of the question mark.
            fill(kRowX + 0x99, y - 0x18, 0x36, 0x31, 0);
            duplicateArea(2, 1, kAnswersX - 0x1B + a * kItemStep, y - 0x18, 0x36, 0x31, kRowX + 0x99, y - 0x18);
        } else {
            music(0x26);
            points -= 0x32;
            intBox(0xCC, 0x14E, 0x60, 0x19, points);
            drawLogo(bx, by, 0x20A4);
            ++misses;
        }
    };
    auto allSolved = [&] {  // g26_0c3c
        for (const Row& r : rows)
            if (!r.solved) return false;
        return true;
    };

    // g26_0cac / g26_0000 / g26_04dc: the panels.
    panels_.clear();
    Panels::Panel answers;  // DS:9006
    answers.x = kPanelX, answers.y = kPanelY, answers.w = 0xF0, answers.h = 0x113;
    for (int i = 0; i < 5; ++i)
        for (int k = 0; k < 4; ++k) answers.buttons.push_back({k * kItemStep, i * kRowStep, 0x3C, 0x32});
    answers.onPress = [&](int k) {  // g26_0406
        if (k >= 0) picked = k;
    };
    panels_.add(answers);
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
    intBox(0xCC, 0x14E, 0x60, 0x19, points);
    // A new set of rows (g26_153a's [8F30]).
    fill(0x3C, 0x1E, 0x20E, 0x11C, 0);
    for (int i = 0; i < 5; ++i) {
        generate(rows[i]);
        drawRow(i);
    }
    // g26_05f8: the dividing lines and a box round each item in the rows.
    line(0x13B, 0x1E, 0x13B, 0x1E + 0x11C, 0xFF);
    line(0x143, 0x1E, 0x143, 0x1E + 0x11C, 0xFF);
    for (int i = 0; i < 5; ++i)
        for (int k = 0; k < 4; ++k) frame(kRowX - 0x1C + k * kItemStep, i * kRowStep + kRowY - 0x19, 0x38, 0x33, 0xFF);
    copyArea(2, 1, 0x3C, 0x1E, 0x20E, 0x11C);
    intBox(0xCC, 0x14E, 0x60, 0x19, points);
    digitalTime(0x164, 0x14E, 0x60, 0x19, timeLeft);
    select(1);
    ctx_.timer.setPeriodic(kSecondSlot, 1, [&] {  // g26_00ba
        if (timeLeft > 0) --timeLeft;
        timeShown = true;
    });
    clearInput();

    bool done = false;
    while (!done && !quit) {
        panels_.poll(ctx_.platform);
        ctx_.pump();
        if (ctx_.platform.takeKeyIf('h') || ctx_.platform.takeKeyIf('H')) helpWanted = true;  // f26_0c82: H, as the help button ([B75A])
        if (helpWanted) {
            helpWanted = false;
            select(1);
            drawLogo(0x26, 0x15F, 0x20A6);
            messageBox({dataString(0x4ED6), dataString(0x4F0F)});  // g26_03d6
            drawLogo(0x26, 0x15F, 0x20A5);
        }
        if (gadget) {
            gadget = false;
            music(0x19);
            monitorGadget();
        }
        if (picked >= 0) {
            const int row = picked / 4, a = picked % 4;
            picked = -1;
            if (!rows[row].solved) answer(row, a);
            if (allSolved()) done = true;
        }
        showTime();
        if (done) {
            ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);
            const int used = limit - timeLeft;  // [B794]
            int mode = 2;
            if (misses <= 2) {
                // 100 points, and 2 for each second left.
                mode = 0;
                points += 100;
                while (timeLeft > 0) {
                    --timeLeft;
                    digitalTime(0x164, 0x14E, 0x60, 0x19, timeLeft);
                    points += 2;
                    intBox(0xCC, 0x14E, 0x60, 0x19, points);
                }
            }
            digitalTime(0x164, 0x14E, 0x60, 0x19, limit - used);
            waitOrClick(0x14);
            panels_.clear();
            puzzleResult(true, points, mode, used);
        }
    }
    ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);
    panels_.clear();
    if (!done) puzzleResult(false, 0, 0, 0);
    return done;
}

}  // namespace edison
