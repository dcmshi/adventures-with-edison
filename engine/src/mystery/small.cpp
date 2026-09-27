// The smaller puzzles, each a segment of MALL.EXE on its own.

#include <algorithm>
#include <cstdio>
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

// --- 6: Codes (segment 20) -------------------------------------------------

bool Mystery::codes(int level) {
    // g20_1474. One of three code alphabets (26 pictures from 2177, 2191
    // or 21AB) with its chart below. Level 0: a word in code; click a
    // symbol, then the chart symbol it matches (25 points up or down).
    // Higher levels: a phrase whose words have their letters mixed up,
    // shown in code with the letters under; click two letters to swap
    // them until the phrase reads right. Five minutes.
    const bool decode = level == 0;  // [88B2]
    static const uint16_t kAlphabet[3] = {0x21AB, 0x2191, 0x2177};
    const uint16_t alphabet = kAlphabet[random(3)];  // [88AE]
    const int limit = 300;
    int timeLeft = limit;         // segment 66: 0x50
    bool timeChanged = true;      // segment 66: 0x52
    int points = 0;               // [88AC]
    bool solved = false;          // [88AA]
    bool quit = false;            // [91A2]
    bool helpWanted = false;      // [B75A]
    bool gizmo = false;           // [3AE4]
    bool showLetters = false;     // [C134]
    int picked = -1;              // [88A4]: the message symbol clicked
    int chartPick = -1;           // [88B6]
    int lastPicked = -1;          // [88A8] (the letter wanted, in decode mode)
    int wantAt = -1;              // [88A6]
    bool messagePicked = false;
    int clicks = 0;               // [3D80]
    bool fixed[40] = {};          // DS:B362: spaces, not clickable
    bool done[40] = {};           // DS:C24C: decoded letters
    std::string phrase, message;  // the answer, and segment 66:0x28
    int left = 0;                 // [88B0]

    const Font* normal = font_;
    font_ = &ctx_.font(0x101);
    auto slotX = [](int i) { return 0x48 + (i % 13) * 0x26; };
    auto slotY = [](int i) { return 0x24 + (i / 13 == 0 ? 0 : (i / 13) * 0x2C + 0x10); };
    auto shifted = [&](int x, int y, uint16_t id, int add) {  // f06_19fa at 1:1: colours shifted by `add`
        const Bitmap& bmp = ctx_.bitmap(id);
        drawVia3(x, y, bmp.width, bmp.height, [&](int s) {
            Screen& scr = ctx_.screens[s];
            for (int r = 0; r < bmp.height; ++r)
                for (int c = 0; c < bmp.width; ++c) {
                    const uint8_t p = bmp.at(c, r);
                    const int px = x + c, py = y + r;
                    if (p && px >= 0 && py >= 0 && px < Screen::kWidth && py < Screen::kHeight)
                        scr.pixels[static_cast<size_t>(py) * Screen::kWidth + px] = static_cast<uint8_t>(p + add);
                }
        });
    };
    auto symbol = [&](int i, bool highlight, bool letter) {  // g20_07e8 / g20_06ee
        if (i >= static_cast<int>(message.size()) || message[i] == ' ') return;
        select(1);
        const uint16_t id = static_cast<uint16_t>(alphabet + message[i] - 'A');
        const int x = slotX(i), y = slotY(i);
        if (highlight)
            shifted(x, y, id, 3);
        else
            drawOpaque(x, y, id);
        if (letter) text(x, y + ctx_.bitmap(id).height, std::string(1, message[i]), 0);
    };
    auto drawMessage = [&] {  // g20_08e8
        for (int i = 0; i < static_cast<int>(message.size()); ++i) symbol(i, false, !decode || showLetters);
    };
    auto chart = [&] {  // g20_0a70
        select(1);
        for (int k = 0; k < 26; ++k) {
            const int x = 0x48 + (k % 13) * 0x26, y = k < 13 ? 0xDA : 0x116;
            drawOpaque(x, y, static_cast<uint16_t>(alphabet + k));
            text(x, y + ctx_.bitmap(static_cast<uint16_t>(alphabet + k % 13)).height,
                 std::string(1, static_cast<char>('A' + k)), 0);
        }
    };
    auto box = [&](int x, const std::string& s) {  // g20_0000 / g20_00c0
        select(2);
        fill(x, 0x163, 0x2A, 0x10, 0);
        text((0x2A - font_->width(s)) / 2 + x, (0x10 - font_->height()) / 2 + 0x163, s, 0xFF);
        copyArea(2, 1, x, 0x163, 0x2A, 0x10);
        select(1);
    };
    auto score = [&] { box(0xFB, std::to_string(points)); };
    auto clock = [&] {
        char s[16];
        std::snprintf(s, sizeof s, "%d:%02d", timeLeft / 60, timeLeft % 60);
        box(0x1A9, s);
    };
    auto verdict = [&](bool right) {  // g20_0d02
        const std::string s = dataString(dataWord(right ? 0x3D68 : 0x3D7E));
        const int fh = font_->height(), x = (Screen::kWidth - font_->width(s)) / 2;
        text(x, fh, s, 0);
        waitCountdown(10);
        copyArea(2, 1, x, fh, font_->width(s), fh);
    };

    // g20_0bdc: the buttons.
    panels_.clear();
    Panels::Panel exit;  // f06_23d8
    exit.x = 0x208, exit.y = 0x156, exit.w = 0x50, exit.h = 0x2A;
    exit.buttons = {{0, 0, 0x50, 0x2A}};
    exit.onPress = [&](int k) {
        if (k >= 0) quit = true;
    };
    panels_.add(exit);
    Panels::Panel helpButton;  // f06_2436
    helpButton.x = 0x28, helpButton.y = 0x156, helpButton.w = 0x50, helpButton.h = 0x2A;
    helpButton.buttons = {{0, 0, 0x50, 0x2A}};
    helpButton.onPress = [&](int k) {
        if (k >= 0) helpWanted = true;
    };
    panels_.add(helpButton);
    Panels::Panel words;  // DS:3AA8
    words.x = 0x48, words.y = 0x24, words.w = 0x1EE, words.h = 0xA8;
    for (int i = 0; i < 39; ++i) words.buttons.push_back({slotX(i) - 0x48, slotY(i) - 0x24, 0x26, 0x2C});
    words.onPress = [&](int i) {  // g20_0976
        if (i >= 0 && !fixed[i]) picked = i;
    };
    panels_.add(words);
    if (decode) {
        Panels::Panel letters;  // DS:3AC2
        letters.x = 0x48, letters.y = 0xDA, letters.w = 0x1EE, letters.h = 0x1EE;
        for (int k = 0; k < 26; ++k) letters.buttons.push_back({(k % 13) * 0x26, k < 13 ? 0 : 0x3C, 0x26, 0x2C});
        letters.onPress = [&](int k) {  // g20_09bc
            if (k >= 0) chartPick = k;
        };
        panels_.add(letters);
    }
    Panels::Panel gadget;  // DS:3AE6
    gadget.x = 0x24E, gadget.y = 0x52, gadget.w = 0x32, gadget.h = 0x72;
    gadget.buttons = {{0, 0, 0x32, 0x72}};
    gadget.onPress = [&](int k) {  // g20_0190
        if (k >= 0 && !gizmo) {
            music(0x12);
            gizmo = true;
        }
    };
    panels_.add(gadget);
    clearInput();
    ctx_.timer.setPeriodic(kSecondSlot, 1, [&] {  // g20_09e8
        if (timeLeft > 0) --timeLeft;
        timeChanged = true;
    });

    // g20_0e00
    ctx_.blackout();
    ctx_.showFullScreen(0x1005, 2);
    select(2);
    drawLogo(0x24E, 0x52, 0x2206);
    select(1);
    show(2);
    computeUiColours();
    chart();
    if (!decode) {
        // g20_04bc: the phrase, each word's letters mixed (as many random
        // swaps as half its length).
        phrase = dataString(dataWord(static_cast<uint16_t>(0x39B6 + 4 * random(10))));
        message.clear();
        for (size_t at = 0; at < phrase.size();) {
            std::string word;
            while (at < phrase.size() && phrase[at] != ' ') word += phrase[at++];
            const int n = static_cast<int>(word.size());
            for (int k = 0; k < n / 2; ++k) std::swap(word[random(n)], word[random(n)]);
            message += word;
            while (at < phrase.size() && phrase[at] == ' ') message += phrase[at++];
        }
        for (int i = 0; i < 39; ++i) fixed[i] = i >= static_cast<int>(message.size()) || message[i] == ' ';
    } else {
        phrase = message = dataString(dataWord(static_cast<uint16_t>(0x3A64 + 4 * random(17))));  // g20_0434
        left = static_cast<int>(message.size());
    }
    drawMessage();
    score();
    while (!quit && !solved) {
        panels_.poll(ctx_.platform);
        ctx_.pump();
        if (!decode) {
            if (picked >= 0) {
                symbol(picked, true, true);
                if (++clicks > 1 && picked != lastPicked) {
                    clicks = 0;
                    if (lastPicked >= 0) std::swap(message[picked], message[lastPicked]);
                    copyArea(2, 1, 0x48, 0x24, 0x1EE, 0xA8);
                    drawMessage();
                }
                lastPicked = picked;
                if (message == phrase) solved = true;
                picked = -1;
            }
        } else {
            if (picked >= 0) {
                for (int i = 0; i < static_cast<int>(message.size()); ++i) symbol(i, i == picked, done[i]);
                lastPicked = message[picked] - 'A';
                wantAt = picked;
                messagePicked = true;
            }
            if (chartPick >= 0 && messagePicked) {
                chart();
                shifted(0x48 + (chartPick % 13) * 0x26, chartPick < 13 ? 0xDA : 0x116,
                        static_cast<uint16_t>(alphabet + chartPick), 3);
                if (chartPick == lastPicked) {
                    points += 0x19;
                    verdict(true);
                    // (The original counts a letter again if it is decoded
                    // twice; kept.)
                    if (--left == 0) solved = true;
                    done[wantAt] = true;
                } else {
                    points -= 0x19;
                    verdict(false);
                }
                score();
                copyArea(2, 1, 0x48, 0x24, 0x1EE, 0x2C);
                for (int i = 0; i < static_cast<int>(message.size()); ++i) symbol(i, false, done[i]);
                chart();
                messagePicked = false;
            }
            picked = chartPick = -1;
        }
        if (gizmo) {  // g20_01c8
            for (int f = 1; f <= 0x14; ++f) {
                drawOpaque(0x24E, 0x52, static_cast<uint16_t>(0x2206 + (f == 0x14 ? 0 : f)));
                waitCountdown(2);
            }
            gizmo = false;
        }
        if (timeChanged) {
            timeChanged = false;
            clock();
        }
        if (helpWanted || helpPressed_) {
            helpWanted = helpPressed_ = false;
            shifted(0x28, 0x156, 0x2176, 0x14);
            messageBox(dataLines(decode ? 0x3C40 : 0x3D42));
            drawLogo(0x28, 0x156, 0x2176);
        }
        if (quit) shifted(0x208, 0x156, 0x2175, 0x14);
    }
    ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);
    bool won = false;
    if (solved) {
        // Every second left is worth 2 points (6 in the unscramble mode).
        const int used = limit - timeLeft;
        while (timeLeft > 0) {
            --timeLeft;
            clock();
            points += decode ? 2 : 6;
            score();
        }
        clock();
        waitCountdown(4);
        font_ = normal;
        panels_.clear();
        won = puzzleResult(true, points, 0, used);
    } else {
        // The answer is shown.
        message = phrase;
        copyArea(2, 1, 0x48, 0x24, 0x1EE, 0xA8);
        showLetters = true;
        drawMessage();
        font_ = normal;
        panels_.clear();
        puzzleResult(false, 0, 0, 0);
    }
    font_ = normal;
    clearInput();
    return won;
}

// --- 3: Binary Lights (segment 17) ---------------------------------------

bool Mystery::binaryLights(int level) {
    // g17_1256: five sums. Eight switches set two 4-bit numbers (the left
    // switch of each row is 8); the machine shows them, the operation and
    // the answer, and the answer has to match the target. The clock runs
    // (5 minutes) but only counts for the bonus.
    level = std::clamp(level, 0, 3);
    int rounds = 5;           // [823A]
    int timeLeft = 300;       // [8224]
    const int limit = 300;    // [930E]
    int points = 0;           // [8226]
    bool quit = false;        // [91A2]
    bool helpWanted = false;  // [B75A]
    bool gizmo = false;       // [32E0]
    bool changed = false;     // [822A]
    bool timeShown = false;   // [8228]
    bool on[8] = {};          // DS:31EC's buttons' state
    int target = 0, a = 0, b = 0, answer = 0;  // [822C], [8230], [8232], [822E]
    char op = '+';            // [8234]
    // DS:323C lights, 325C switches, 327C the 0/1 under each; DS:329C /
    // 32AC the boxes for A, B, the answer and the target.
    static const int kLight[8][2] = {{159, 82},  {239, 82},  {320, 82},  {399, 82},
                                     {159, 210}, {239, 210}, {320, 210}, {399, 210}};
    static const int kSwitch[8][2] = {{158, 40},  {238, 40},  {319, 40},  {398, 40},
                                      {158, 166}, {238, 166}, {319, 166}, {398, 166}};
    static const int kBit[8][2] = {{154, 132}, {235, 132}, {316, 132}, {397, 132},
                                   {154, 260}, {235, 260}, {316, 260}, {397, 260}};
    static const int kBox[4][4] = {{503, 84, 38, 28}, {502, 137, 38, 28}, {498, 194, 45, 42}, {464, 301, 112, 27}};
    static const int kSwitchX[4] = {0xB, 0x5C, 0xAB, 0xFB};

    auto box = [&](int k, const std::string& s) {  // g17_052c
        const int* r = kBox[k];
        fill(r[0], r[1], r[2], r[3], 0);
        text(r[0] + r[2] / 2 - font_->width(s) / 2, r[1] + 6, s, 0xFF);
    };
    auto machine = [&] {  // g17_05a6 (and g17_0b6e's test)
        select(1);
        a = (on[0] ? 8 : 0) + (on[1] ? 4 : 0) + (on[2] ? 2 : 0) + (on[3] ? 1 : 0);
        b = (on[4] ? 8 : 0) + (on[5] ? 4 : 0) + (on[6] ? 2 : 0) + (on[7] ? 1 : 0);
        box(0, std::to_string(a));
        box(1, std::to_string(b));
        bool valid = false;
        switch (op) {
        case '*':
            answer = a * b, valid = true;
            drawLogo(0x1D9, 0x75, 0x2103);
            break;
        case '+':
            answer = a + b, valid = true;
            drawLogo(0x1D9, 0x75, 0x2102);
            break;
        case '-':
            if (b <= a) answer = a - b, valid = true;
            drawLogo(0x1D9, 0x75, 0x2101);
            break;
        default:
            if (b <= a && b > 0 && a % b == 0) answer = a / b, valid = true;
            drawLogo(0x1D9, 0x75, 0x2104);
        }
        box(2, valid ? std::to_string(answer) : "");
        box(3, std::to_string(target));
        return answer == target;
    };
    auto newSum = [&] {  // g17_088c
        target = answer = a = b = 0;
        auto sum = [&] {
            do target = random(16) + random(16);
            while (!target);
            op = '+';
        };
        auto difference = [&] {
            do target = std::abs(random(16) - random(16));
            while (!target);
            op = '-';
        };
        if (level == 0) {
            sum();
        } else if (level == 1) {
            if (random(2) == 0) difference();
            else sum();
        } else {
            switch (random(4)) {
            case 0: sum(); break;
            case 1: difference(); break;
            case 2:
                do {
                    const int x = random(8) * 2, y = random(8) * 2;
                    if (y < x) {
                        if (y > 0) target = x / y;
                    } else if (x > 0) {
                        target = y / x;
                    }
                } while (!target);
                op = '/';
                break;
            default: {
                const int base = random(10) < 6 ? 0 : 8;
                do target = (random(8) + base) * (random(8) + base);
                while (!target);
                op = '*';
            }
            }
        }
    };
    auto drawSwitch = [&](int k) {  // g17_032e's drawing
        drawLogo(kLight[k][0], kLight[k][1], on[k] ? 0x20FF : 0x2100);
        drawLogo(kSwitch[k][0], kSwitch[k][1], on[k] ? 0x20FD : 0x20FE);
        fill(kBit[k][0], kBit[k][1], 0x24, 0x13, 0);
        text(kBit[k][0] + 10, kBit[k][1], on[k] ? "1" : "0", 0xFF);
    };

    clearInput();
    ctx_.blackout();
    ctx_.showFullScreen(0x100C, 2);
    select(2);
    drawOpaque(0x1B6, 0x158, 0x21F8);
    select(1);
    show(2);
    computeUiColours();
    duplicateArea(1, 2, 0x94, 0x26, 0x11E, 0x2C, 0x94, 0x28);
    duplicateArea(1, 2, 0x94, 0xA4, 0x11E, 0x2C, 0x94, 0xA4);
    // g17_0d60: the buttons.
    panels_.clear();
    Panels::Panel exit;  // f06_23d8
    exit.x = 0x1EE, exit.y = 0x16, exit.w = 0x44, exit.h = 0x1D;
    exit.buttons = {{0, 0, 0x44, 0x1D}};
    exit.onPress = [&](int k) {
        if (k >= 0) quit = true;
    };
    panels_.add(exit);
    Panels::Panel lesson;  // f06_2436
    lesson.x = 0x20, lesson.y = 0x9E, lesson.w = 0x44, lesson.h = 0x13;
    lesson.buttons = {{0, 0, 0x44, 0x13}};
    lesson.onPress = [&](int k) {
        if (k >= 0) helpWanted = true;
    };
    panels_.add(lesson);
    Panels::Panel switches;  // DS:32BC
    switches.x = 0x94, switches.y = 0x20, switches.w = 0x120, switches.h = 0xFE;
    for (int k = 0; k < 8; ++k) switches.buttons.push_back({kSwitchX[k % 4], k < 4 ? 5 : 0x85, 0x1E, 0x71});
    switches.onPress = [&](int k) {  // g17_032e
        if (k < 0) return;
        music(5);
        changed = true;
        on[k] = !on[k];
        drawSwitch(k);
    };
    panels_.add(switches);
    Panels::Panel gadget;  // DS:32E2
    gadget.x = 0x1B6, gadget.y = 0x158, gadget.w = 0x38, gadget.h = 0x1A;
    gadget.buttons = {{0, 0, 0x38, 0x1A}};
    gadget.onPress = [&](int k) {  // g17_0000
        if (k >= 0) gizmo = true;
    };
    panels_.add(gadget);
    ctx_.timer.setPeriodic(kSecondSlot, 1, [&] {  // g17_0190
        if (timeLeft > 0) --timeLeft;
        timeShown = true;
    });
    clearInput();

    bool won = false;
    while (!quit) {
        intBox(0x2E, 0x132, 0x30, 0x14, rounds);
        if (--rounds < 0) {
            // All five: 100 points, and 2 for each second left.
            machine();
            points += 100;
            const int used = limit - timeLeft;
            while (timeLeft > 0) {
                --timeLeft;
                digitalTime(0x2F, 0x2B, 0x30, 0x15, timeLeft);
                points += 2;
                intBox(0x2F, 0x101, 0x30, 0x15, points);
            }
            digitalTime(0x2F, 0x2B, 0x30, 0x15, limit - used);
            ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);
            panels_.clear();
            won = puzzleResult(true, points, 0, used);
            break;
        }
        // g17_0bcc: clear the machine and flick the switches about.
        select(1);
        duplicateArea(2, 1, 0x94, 0x26, 0x11E, 0x2C, 0x94, 0x28);
        duplicateArea(2, 1, 0x94, 0xA4, 0x11E, 0x2C, 0x94, 0xA4);
        for (int k = 0; k < 8; ++k) fill(kBit[k][0], kBit[k][1], 0x24, 0x13, 0);
        for (int k = 0; k < 4; ++k) fill(kBox[k][0], kBox[k][1], kBox[k][2], kBox[k][3], 0);
        for (int i = 0; i < 16; ++i) {
            const int k = random(8);
            music(4);
            drawLogo(kSwitch[k][0], kSwitch[k][1], 0x20FD);
            waitCountdown(1);
            drawLogo(kSwitch[k][0], kSwitch[k][1], 0x20FE);
        }
        for (bool& s : on) s = false;
        for (int k = 0; k < 8; ++k) drawSwitch(k);  // g17_02ba, g17_01e6
        newSum();
        machine();
        digitalTime(0x2F, 0x2B, 0x30, 0x15, timeLeft);
        intBox(0x2F, 0x101, 0x30, 0x15, points);
        bool solved = false;
        while (!solved && !quit) {
            panels_.poll(ctx_.platform);
            ctx_.pump();
            if (changed) {
                changed = false;
                if (machine()) {
                    // "You got it!" at the top; the lit switches flash.
                    const std::string msg = dataString(0x34B0);
                    const int fh = font_->height(), x = (Screen::kWidth - font_->width(msg)) / 2;
                    const int saved = saveArea(x, fh, font_->width(msg), fh);
                    text(x, fh, msg, 0);
                    if (rounds != 0) {
                        for (int t = 0; t < 3; ++t) {
                            music(4);
                            for (int k = 0; k < 8; ++k) {
                                if (!on[k]) continue;
                                drawLogo(kSwitch[k][0], kSwitch[k][1], 0x20FE);
                                waitCountdown(1);
                                drawLogo(kSwitch[k][0], kSwitch[k][1], 0x20FD);
                            }
                            waitCountdown(1);
                        }
                    }
                    waitCountdown(10);
                    restoreArea(saved);
                    solved = true;
                }
            }
            if (helpWanted || helpPressed_) {  // g17_01b6: the lesson on binary numbers
                helpWanted = helpPressed_ = false;
                messageBox(dataLines(0x3480));
            }
            if (gizmo) {  // g17_0062: the Director's screen
                music(6);
                select(1);
                drawOpaque(0x1B6, 0x158, 0x21F9);
                static const uint8_t kFrames[] = {0, 1, 2, 3, 4, 5, 6, 7, 0x80, 8, 9, 0x80, 8, 7, 0x80, 8, 9, 0x80, 10, 11, 0};
                for (uint8_t f : kFrames) {
                    if (f == 0x80) {
                        waitCountdown(4);
                    } else {
                        drawOpaque(0x238, 0, static_cast<uint16_t>(0x21FA + f));
                        waitCountdown(2);
                    }
                }
                drawOpaque(0x1B6, 0x158, 0x21F8);
                gizmo = false;
            }
            if (timeShown) {
                digitalTime(0x2F, 0x2B, 0x30, 0x15, timeLeft);
                timeShown = false;
            }
        }
    }
    ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);
    clearInput();
    panels_.clear();
    if (quit && rounds >= 0) won = puzzleResult(false, 0, 0, 0);
    return won;
}

}  // namespace edison
