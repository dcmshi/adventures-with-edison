// The smaller puzzles, each a segment of MALL.EXE on its own.

#include <algorithm>
#include <random>

#include "mystery/mystery.h"

namespace edison {

namespace {

int random(int n) {
    static std::mt19937 rng{std::random_device{}()};
    return std::uniform_int_distribution<int>(0, n - 1)(rng);
}

constexpr int kSecondSlot = 6;

}  // namespace

// --- 8: the Circuit Analyzer (segment 16) ---------------------------------

bool Mystery::circuitAnalyzer(int level) {
    // g16_089c: Mastermind. A hidden code of four colours; ten columns of
    // four plugs to try codes in, each checked with the button above it:
    // a black light for each plug of the right colour in the right place,
    // a white one for each right colour in the wrong place.
    level = std::min(level, 7);
    // DS:30E6: colours, whether they are all different, seconds.
    static const int kLevel[8][3] = {{2, 0, 120}, {3, 0, 240}, {4, 0, 360}, {4, 1, 360},
                                     {5, 0, 480}, {5, 1, 480}, {6, 0, 600}, {6, 1, 600}};
    const int colours = kLevel[level][0];
    const bool different = kLevel[level][1] && colours >= 4;
    const int limit = kLevel[level][2];
    // DS:2F3C: plug k (column k / 4, place k % 4), relative to (8, A0);
    // DS:3140: the check buttons, relative to (5, 65).
    auto plugX = [](int k) { return (k / 4) * 0x34; };
    auto plugY = [](int k) { return (k % 4) * 0x32; };
    auto columnX = [](int c) { return c * 0x34; };

    int plugs[40];
    bool checked[10] = {};
    int code[4];
    int state = 0;         // [3124]: 1 exit, 2 out of tries, 3 cracked
    int triesLeft = 10;    // [C126]
    int timeLeft = limit;  // [B45E]
    int shownTime = -1;    // [311A]
    int points = 0;        // [2F3A]
    bool gizmo = false;    // [31C6]
    std::fill(std::begin(plugs), std::end(plugs), -1);

    clearInput();
    ctx_.blackout();
    ctx_.showFullScreen(0x1009, 2);
    select(2);
    for (int i = 0; i < 4; ++i) drawLogo(0x234, plugY(i) + 0xA0, 0x20A9);
    panels_.clear();
    text(0x1E6, 0x17C, dataString(0x31E1), 0xE0);  // "EXIT"
    for (int c = 0; c < 10; ++c) {
        const int x = columnX(c);
        drawLogo(x + 5, 0x65, 0x20BA);
        drawLogo(x + 9, 0x37, 0x20B8);
        drawLogo(x + 0x1D, 0x37, 0x20B8);
        drawLogo(x + 9, 0x4B, 0x20B8);
        drawLogo(x + 0x1D, 0x4B, 0x20B8);
        for (int p = 0; p < 4; ++p) drawLogo(x + 8, plugY(p) + 0xA0, static_cast<uint16_t>(0x20AF + random(6)));
    }
    bool used[8] = {};
    for (int i = 0; i < 4; ++i) {
        if (!different) {
            code[i] = random(colours);
        } else {
            int c;
            do c = random(colours);
            while (used[c]);
            used[c] = true;
            code[i] = c;
        }
    }
    auto score = [&](bool final) {  // g16_0000
        points = timeLeft * 4;
        if (final) points += triesLeft * 0x32;
        intBox(0x22E, 0x4E, 0x3C, 0x1A, points);
    };
    auto clockTick = [&] {  // g16_0070
        if (timeLeft == shownTime) return;
        digitalTime(0x22E, 0x16, 0x3C, 0x1A, timeLeft);
        shownTime = timeLeft;
        score(false);
    };
    clockTick();
    select(1);
    show(2);
    computeUiColours();
    ctx_.showFullScreen(0x1009, 2);  // a clean backdrop to draw from
    select(1);

    Panels::Panel exit;  // DS:3126
    exit.x = 0x152, exit.y = 0x17C, exit.w = 0xBC, exit.h = 0x12;
    exit.buttons = {{0x8E, 0, 0x2A, 0x14}};
    exit.onPress = [&](int b) {  // g16_00b6
        if (b >= 0 && state == 0) state = 1;
    };
    panels_.add(exit);
    Panels::Panel checks;  // DS:31A4
    checks.x = 5, checks.y = 0x65, checks.w = 0x202, checks.h = 0x1C;
    for (int c = 0; c < 10; ++c) checks.buttons.push_back({columnX(c), 0, 0x2E, 0x1C});
    checks.onPress = [&](int c) {  // g16_0102
        if (c < 0 || checked[c]) return;
        for (int p = 0; p < 4; ++p)
            if (plugs[c * 4 + p] == -1) return;
        checked[c] = true;
        music(0x13);
        drawLogo(columnX(c) + 5, 0x65, 0x20BB);
        // g16_03d2: the lights.
        int right = 0, near = 0;
        bool guessUsed[4] = {}, codeUsed[4] = {};
        for (int p = 0; p < 4; ++p)
            if (plugs[c * 4 + p] == code[p]) ++right, guessUsed[p] = codeUsed[p] = true;
        for (int p = 0; p < 4; ++p) {
            if (guessUsed[p]) continue;
            for (int q = 0; q < 4; ++q)
                if (!codeUsed[q] && plugs[c * 4 + p] == code[q]) {
                    ++near;
                    codeUsed[q] = true;
                    break;
                }
        }
        if (right == 4) state = 3;
        static const int kLightX[4] = {4, 0x18, 4, 0x18}, kLightY[4] = {0x2E, 0x2E, 0x1A, 0x1A};
        for (int i = 0; i < 4; ++i) {
            uint16_t light = 0x20B5;
            if (right > 0) {
                light = 0x20B7;
                --right;
            } else if (near > 0) {
                light = 0x20B6;
                --near;
            }
            drawLogo(columnX(c) + 5 + kLightX[i], 0x65 - kLightY[i], light);
        }
        if (--triesLeft == 0 && state == 0) state = 2;
    };
    panels_.add(checks);
    Panels::Panel board;  // DS:30CC
    board.x = 8, board.y = 0xA0, board.w = 0x204, board.h = 0xB4;
    for (int k = 0; k < 40; ++k) board.buttons.push_back({plugX(k), plugY(k), 0x30, 0x1A});
    board.onPress = [&](int k) {  // g16_021e
        if (k < 0 || checked[k / 4]) return;
        plugs[k] = (plugs[k] + 1) % colours;
        music(0x12);
        const int x = plugX(k) + 8, y = plugY(k) + 0xA0;
        copyArea(2, 1, x, y, 0x36, 0x20);
        drawLogo(x, y, static_cast<uint16_t>(0x20AF + plugs[k]));
    };
    panels_.add(board);
    Panels::Panel gadget;  // DS:31C8
    gadget.x = 0x21C, gadget.y = 0x15E, gadget.w = 0x64, gadget.h = 0x2E;
    gadget.buttons = {{0, 0, 0x64, 0x2E}};
    gadget.onPress = [&](int b) {  // g16_0314
        if (b >= 0 && !gizmo) {
            gizmo = true;
            music(0x11);
        }
    };
    panels_.add(gadget);
    helpPanel(0x12, 0xC, 0x38, 0x12, 0xE3, 0xFB);

    // g16_066a: the code's lights flicker, then are covered.
    for (int t = 0; t < 10; ++t) {
        for (int i = 0; i < 4; ++i) {
            copyArea(2, 1, 0x234, plugY(i) + 0xA0, 0x36, 0x20);
            drawLogo(0x234, plugY(i) + 0xA0, static_cast<uint16_t>(0x20A9 + random(colours)));
        }
        waitCountdown(2);
    }
    for (int i = 0; i < 4; ++i) drawLogo(0x222, plugY(i) + 0x98, 0x20B9);
    ctx_.timer.setPeriodic(kSecondSlot, 1, [&] {  // g16_0050
        if (timeLeft > 0) --timeLeft;
    });
    clearInput();
    bool won = false;
    for (;;) {
        panels_.poll(ctx_.platform);
        ctx_.pump();
        clockTick();
        if (state != 0) {
            text(0x1E6, 0x17C, dataString(0x31E6), 0xE3);
            waitCountdown(4);
            // g16_034c: the code shown.
            select(2);
            for (int i = 0; i < 4; ++i) drawLogo(0x234, plugY(i) + 0xA0, static_cast<uint16_t>(0x20A9 + code[i]));
            copyArea(2, 1, 0x222, 0x98, 0x5E, plugY(3) + 0x30);
            select(1);
            if (state == 3) {
                won = true;
                clockTick();
                score(true);
            } else if (state <= 1) {
                points = 0;
            }
            break;
        }
        if (gizmo) {  // g16_073c: the meter swings
            static const uint8_t kFrames[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
            for (uint8_t f : kFrames) {
                drawOpaque(0x21C, 0x15E, static_cast<uint16_t>(0x21DA + f));
                waitCountdown(1);
            }
            gizmo = false;
        }
        if (helpPressed_) help(0x3F09);
    }
    ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);
    clearInput();
    panels_.clear();
    const int mode = triesLeft > 6 ? 0 : triesLeft > 2 ? 1 : 2;
    const bool result = puzzleResult(won, points, mode, limit - timeLeft);
    fill(0, 0, Screen::kWidth, Screen::kHeight, 0);
    return result;
}

}  // namespace edison
