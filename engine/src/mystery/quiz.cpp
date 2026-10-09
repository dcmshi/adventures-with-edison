// The Question and Answer Period (segment 19): puzzle 4, and the quiz at
// the end of a won game.

#include <algorithm>

#include "mystery/mystery.h"

namespace edison {

namespace {

constexpr int kSecondSlot = 6;

// Panel DS:372E's buttons (DS:368E), relative to (0xB0, 0x4E): the blocks.
constexpr int kBlock[16][2] = {
    {0x20, 0x00}, {0x68, 0x00}, {0xB0, 0x00}, {0x00, 0x27}, {0x48, 0x27}, {0x90, 0x27},
    {0xD8, 0x27}, {0x20, 0x4E}, {0x68, 0x4E}, {0xB0, 0x4E}, {0x00, 0x75}, {0x48, 0x75},
    {0x90, 0x75}, {0xD8, 0x75}, {0x20, 0x9C}, {0xB0, 0x9C},
};
constexpr int kBlocksX = 0xB0, kBlocksY = 0x4E;
// Panel DS:3770: the four answer lines; DS:37B2: the pads 1-4 below.
constexpr int kAnswersX = 0xAA, kAnswersY = 0xAA, kAnswerH = 0x14;
constexpr int kPadsX = 0xC2, kPadsY = 0x142, kPadStep = 0x48;
// The question card (f06_1424 saves it on the display).
constexpr int kCardX = 0x7C, kCardY = 0x66, kCardW = 0x188, kCardH = 0x12A;

}  // namespace

bool Mystery::questionPeriod(int level, bool asPuzzle) {
    // f19_16a2. As the final quiz (not a puzzle) the easy levels have none.
    stirRandom();
    if (!asPuzzle && level < 3) return true;
    int count = 16, total = 300;  // [8582], [B772]
    if (level == 3) count = 9, total = 0xF0;
    else if (level == 4 || level == 5) count = 10;
    if (asPuzzle) count = 16, total = 300;
    int timeLeft = total;          // [7FFC]:0
    bool timeShown = false;        // [7FFC]:2
    int shownElapsed = -1;         // [3800]
    int points = 0;                // [889A]
    int misses = 0;                // [889C]
    bool quit = false;             // [91A2]
    bool helpWanted = false;       // [B75A]
    bool gadget = false;           // [37D4]
    int question = -1;             // [88A0]: the block picked
    int answer = -1;               // [8580]
    bool open = false;             // [9394]: a question is showing
    int state[16] = {};            // DS:368E + 8: 1 answered

    // f19_0632: resource 3F0B holds the questions, each the question, four
    // answers, an empty line and a byte with the right answer (1-4).
    // (Both come after the board is shown, its stir among the draws:
    // 19:129a.)
    std::vector<std::string> lines;
    std::vector<int> right;
    std::vector<int> asked;
    auto loadQuestions = [&] {
        std::vector<uint8_t> raw;
        ctx_.read(0x3F0B, raw);
        size_t at = 0;
        while (at < raw.size() && raw[at]) {
            for (int k = 0; k < 6; ++k) {
                std::string l;
                while (at < raw.size() && raw[at]) l += static_cast<char>(raw[at++]);
                ++at;
                lines.push_back(l);
                if (l.empty()) break;
            }
            right.push_back(at < raw.size() ? raw[at++] - 1 : 0);
        }
    };
    // f19_0d60: the facts learned in Concentration come first; if there
    // are too few, questions 0, 1, 2... make up the rest.
    auto shuffle = [&] {
        std::vector<int> known;
        for (int t = 0; t < 5; ++t)
            for (int c = 0; c < 10; ++c)
                if (learned_[t][c]) known.push_back(t * 10 + c);
        if (static_cast<int>(known.size()) < count) {
            for (int q = 0; static_cast<int>(known.size()) < count; ++q) known.push_back(q);
            asked = known;
        } else {
            for (int i = 0; i < count; ++i) {
                int tries = 0, pick = 0;
                do {
                    pick = known[random(static_cast<int>(known.size()))];
                    if (++tries > 0x1D) tries = 0;
                } while (std::find(asked.begin(), asked.end(), pick) != asked.end() && tries != 0);
                asked.push_back(tries == 0 ? random(10) : pick);
            }
        }
        for (int i = 0; i < count / 2; ++i) std::swap(asked[random(count)], asked[random(count)]);
        const int questions = static_cast<int>(right.size());
        for (int& q : asked) q = questions ? q % questions : 0;
    };

    auto blockAt = [&](int k, uint16_t id) {
        drawLogo(kBlocksX + kBlock[k][0], kBlocksY + kBlock[k][1], id);
    };
    auto stopwatch = [&] {  // f19_0222
        const int elapsed = total - timeLeft;
        if (shownElapsed == elapsed) return;
        const int previous = current();
        select(2);
        copyArea(3, 2, 0x205, 0x82, 0x6E, 0x58);
        clockHand(0x23D, 0xAD, 0x37F8, elapsed % 60, 0);
        if (total % 60 == 0) clockHand(0x23D, 0xAD, 0x37F0, elapsed, 0);
        copyArea(2, 1, 0x205, 0x82, 0x6E, 0x58);
        select(previous);
        shownElapsed = elapsed;
    };
    auto showTime = [&] {
        digitalTime(0x21A, 0x62, 0x28, 0x14, timeLeft);
        stopwatch();
    };
    auto answerLine = [&](int k, const std::string& s, int colour) {
        const std::string number = std::string(1, static_cast<char>('1' + k)) + ". ";
        const int x = kAnswersX, y = kAnswersY + k * kAnswerH;
        text(x, y, number, colour);
        text(x + font_->width(number), y, s, colour);
    };
    auto pick = [&](int k) {  // f19_03a6 / f19_04ec
        if (k < 0 || !open) return;
        answer = k;
        const int previous = current();
        select(1);
        answerLine(k, lines[asked[question] * 6 + 1 + k], 0x60);
        // f06_19fa's x, y is the sprite's centre.
        const uint16_t id = static_cast<uint16_t>(0x209D + k);
        drawShifted(0xD5 + k * kPadStep - ctx_.bitmap(id).width / 2, 0x14C - ctx_.bitmap(id).height / 2, id, 0xA0);
        select(previous);
    };
    auto showQuestion = [&](int k) {  // f19_06ec
        select(1);
        for (int f = 0; f < 5; ++f) {
            music(0x22);
            blockAt(k, static_cast<uint16_t>(0x2091 + f));
            waitCountdown(1);
        }
        blockAt(k, 0x2091);
        select(2);
        drawLogo(kCardX, kCardY, 0x209A);
        const int w = ctx_.bitmap(0x209A).width;
        drawLogo(kCardX + w, kCardY, 0x209B);
        drawLogo((w * 2 - ctx_.bitmap(0x209C).width) / 2 + kCardX, ctx_.bitmap(0x209A).height + kCardY, 0x209C);
        copyArea(2, 1, kCardX, kCardY, kCardW, kCardH);
        select(1);
        const int base = asked[k] * 6;
        const std::string& q = lines[base];
        if (font_->width(q) <= 0x12C) {
            text(0xA4, 0x7A, q, 0xFF);
        } else {
            // Two rows: 28 characters, then on to a space (at most 36).
            size_t at = 0;
            for (int row = 0; row < 2; ++row) {
                std::string part = q.substr(std::min(at, q.size()), 28);
                at += 28;
                while (at < q.size() && q[at] != ' ' && part.size() < 0x24) part += q[at++];
                text(0xA4, 0x7A + row * 0x10, part, 0xFF);
            }
        }
        for (int a = 0; a < 4; ++a) answerLine(a, lines[base + 1 + a], 0xFF);
        open = true;
    };
    auto check = [&](int k, int a) {  // f19_0a0e
        select(1);
        const int y = kAnswersY + 3 * kAnswerH + 0x1E;
        if (right[asked[k]] == a) {
            state[k] = 1;
            points += 0x32;
            const std::string s = dataString(0x3825);  // "That is Correct"
            text((kCardW - font_->width(s)) / 2 + kCardX, y, s, 0x60);
            return;
        }
        state[k] = 2;
        points -= 0x32;
        const std::string s = dataString(0x3835);  // "That is Incorrect"
        text((kCardW - font_->width(s)) / 2 + kCardX, y, s, 0x6B);
        ++misses;
        const int base = asked[k] * 6 + 1;
        answerLine(a, lines[base + a], 0xFF);
        const int r = right[asked[k]];
        for (int i = 0; i < 6; ++i) {
            answerLine(r, lines[base + r], i % 2 ? 0x60 : 0x6B);
            waitCountdown(2);
        }
    };
    auto allAnswered = [&] {  // f19_0d1a
        for (int k = 0; k < count; ++k)
            if (state[k] != 1) return false;
        return true;
    };
    auto help = [&] {  // f19_0cde
        const int previous = current();
        select(1);
        helpWanted = false;
        drawLogo(0x218, 0xF8, 0x2098);
        std::vector<std::string> text;
        for (uint16_t at = 0x3847;;) {
            std::string l = dataString(at);
            if (l.empty()) break;
            at = static_cast<uint16_t>(at + l.size() + 1);
            text.push_back(l);
        }
        messageBox(text);
        drawLogo(0x218, 0xF8, 0x2099);
        select(previous);
    };
    auto runGadget = [&] {  // f19_00da
        select(1);
        drawOpaque(0x58, 0xF4, 0x2249);
        static constexpr int kFrames[] = {0, 1, 2, 3, 4, 5, 4, 5, 4, 5, 4, 3, 2, 1, 0};
        for (int f : kFrames) {
            drawOpaque(0, 0x8E, static_cast<uint16_t>(0x224A + f));
            music(0x21);
            waitCountdown(1);
        }
        drawOpaque(0, 0x8E, 0x2250);
        drawOpaque(0x58, 0xF4, 0x2248);
        gadget = false;
    };
    auto chores = [&] {  // the checks both loops share
        if (timeShown) {
            showTime();
            timeShown = false;
        }
        if (gadget) runGadget();
    };

    // f19_1080: the panels.
    panels_.clear();
    Panels::Panel exit;  // f06_23d8
    exit.x = 0x228, exit.y = 0x16B, exit.w = 0x50, exit.h = 0x16;
    exit.buttons = {{0, 0, 0x50, 0x16}};
    exit.onPress = [&](int k) {
        if (k >= 0) quit = true;
    };
    panels_.add(exit);
    Panels::Panel helpButton;  // f06_2436
    helpButton.x = 0x218, helpButton.y = 0xF8, helpButton.w = 0x50, helpButton.h = 0x16;
    helpButton.buttons = {{0, 0, 0x50, 0x16}};
    helpButton.onPress = [&](int k) {
        if (k >= 0) helpWanted = true;
    };
    panels_.add(helpButton);
    Panels::Panel blocks;  // DS:372E, swapped for DS:3770 while a question shows
    blocks.x = kBlocksX, blocks.y = kBlocksY, blocks.w = 0x120, blocks.h = 0xB9;
    for (const auto& b : kBlock) blocks.buttons.push_back({b[0], b[1], 0x48, 0x2E});
    blocks.onPress = [&](int k) {  // f19_035a
        if (k >= 0 && k < count && state[k] != 1) question = k;
    };
    const int blocksSlot = panels_.add(blocks);
    Panels::Panel answers;  // DS:3770
    answers.x = kAnswersX, answers.y = kAnswersY, answers.w = 0x150, answers.h = 0x50;
    for (int k = 0; k < 4; ++k) answers.buttons.push_back({0, k * kAnswerH, 0x14A, kAnswerH});
    answers.onPress = pick;
    Panels::Panel pads;  // DS:37B2
    pads.x = kPadsX, pads.y = kPadsY, pads.w = 0x1C0, pads.h = 0x15;
    for (int k = 0; k < 4; ++k) pads.buttons.push_back({k * kPadStep, 0, 0x26, 0x15});
    pads.onPress = pick;
    panels_.add(pads);
    Panels::Panel machine;  // DS:37D6
    machine.x = 0x58, machine.y = 0xF4, machine.w = 0x48, machine.h = 0x26;
    machine.buttons = {{0, 0, 0x48, 0x26}};
    machine.onPress = [&](int k) {  // f19_00ac
        if (k >= 0) gadget = true;
    };
    panels_.add(machine);
    clearInput();
    ctx_.timer.setPeriodic(kSecondSlot, 1, [&] {  // f19_02ee
        if (timeLeft > 0) --timeLeft;
        timeShown = true;
    });

    // f19_1166: the board.
    clearInput();
    backdrop(0x1004);
    select(2);
    ctx_.screens.copyAll(2, 3);
    for (int k = 0; k < 16; ++k) {
        blockAt(k, 0x20A1);
        if (k < count) blockAt(k, 0x2091);
    }
    intBox(0x18, 0x67, 0x50, 0x14, points);
    digitalTime(0x21A, 0x62, 0x28, 0x14, timeLeft);
    stopwatch();
    drawLogo(0x228, 0x16B, 0x2097);
    drawLogo(0x218, 0xF8, 0x2099);
    drawOpaque(0x58, 0xF4, 0x2248);
    show(2);
    // (No f06_01f6 here: the UI colours stay the previous screen's.)
    loadQuestions();
    shuffle();
    if (!asPuzzle) {
        // f19_0fb8: Edison turns to face the player.
        select(1);
        static constexpr int kFrames[] = {0, 1, 2, 3, 0};
        for (int f : kFrames) {
            drawLogo(0x1A8, 0xEA, static_cast<uint16_t>(0x2301 + f));
            waitCountdown(f == 3 ? 7 : 3);
        }
    }

    bool done = false;
    int used = 0;  // [B794]
    while (!quit && !done) {
        panels_.poll(ctx_.platform);
        ctx_.pump();
        if (question >= 0) {
            select(1);
            const int saved = saveArea(kCardX, kCardY, kCardW, kCardH);
            showQuestion(question);
            answer = -1;
            panels_[blocksSlot] = answers;  // f07_017e
            clearInput();
            while (answer < 0 && !quit) {
                panels_.poll(ctx_.platform);
                ctx_.pump();
                if (answer >= 0) {
                    check(question, answer);
                    clearInput();
                    waitCountdown(0xC);
                    clearInput();
                }
                if (helpWanted) help();
                chores();
            }
            clearInput();
            restoreArea(saved);
            panels_[blocksSlot] = blocks;
            open = false;
            select(1);
            if (state[question] == 1) {
                blockAt(question, 0x2090);
            } else {
                blockAt(question, 0x20A2);
                state[question] = 1;
            }
            intBox(0x18, 0x67, 0x50, 0x14, points);
            answer = question = -1;
        }
        if (helpWanted) help();
        if (allAnswered()) {
            // 100 points, and 2 for each second left.
            done = true;
            points += 100;
            music(0x42);
            used = total - timeLeft;
            while (timeLeft > 0) {
                --timeLeft;
                showTime();
                points += 2;
                intBox(0x18, 0x67, 0x50, 0x14, points);
            }
            digitalTime(0x21A, 0x62, 0x28, 0x14, total - used);
            if (asPuzzle) {
                ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);
                misses /= 4;
                panels_.clear();
                puzzleResult(true, points, misses, used);
            }
        }
        chores();
    }
    ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);  // f19_167e
    panels_.clear();
    if (asPuzzle) {
        if (!done) puzzleResult(false, 0, 0, 0);
        return done;
    }
    return done && misses <= (count + 1) / 2;
}

}  // namespace edison
