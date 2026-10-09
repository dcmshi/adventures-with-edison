// MALL.EXE segments 12-14: the picture puzzles. A picture (one of 17,
// drawn scaled into the top left of screen 2) is cut into N x N tiles that
// the player puts back together on a board of size x size cells.

#include <algorithm>

#include "mystery/mystery.h"

namespace edison {

namespace {

constexpr int kPictures = 17;
constexpr int kGap = 2;  // between the board's cells
constexpr int kPuzzleSecondSlot = 6;

}  // namespace

// --- the end of any game (f06_1d76) and the score box --------------------

void Mystery::intBox(int x, int y, int w, int h, int value) {
    const int previous = current();
    select(2);
    const std::string s = std::to_string(value);
    fill(x, y, w, h, 0);
    text((w - font_->width(s)) / 2 + x, (h - font_->height()) / 2 + y, s, 0xFF);
    copyArea(2, 1, x, y, w, h);
    select(previous);
}

bool Mystery::puzzleResult(bool won, int points, int mode, int seconds) {
    // Edison pops up at the left and says how it went. (The original also
    // writes the time taken into the "Time taken:" string at DS:0232.)
    (void)seconds;
    if (won) {
        const Square& sq = squares_[square_];
        if (sq.object != 0xFF && objects_[sq.object].museum != 0xFF && !objects_[sq.object].found) {
            music(8);
            waitCountdown(1);
        }
        sound(0x4010);
    } else {
        sound(0x4011);
    }
    applyColours(1, true);
    const int previous = current();
    select(2);
    const int x = 0, y = Screen::kHeight / 3;
    constexpr int kW = 0x37, kH = 0x69;
    duplicateArea(1, 2, x, y, kW, kH, 0, 0);
    uint16_t frame = 0x21C5;
    for (int i = 0; i < 5; ++i) {
        duplicateArea(2, 2, 0, 0, kW, kH, x, y);
        drawLogo(x, y, frame++);
        waitCountdown(2);
        copyArea(2, 1, x, y, kW, kH);
    }
    drawLogo(x, y, static_cast<uint16_t>(frame - 1));
    copyArea(2, 1, x, y, kW, kH);
    select(1);
    const int bx = x + 0x4B, by = y - 0x23;
    if (won) {
        static const uint16_t kSpeech[6] = {0x4041, 0x4034, 0x4057, 0x403F, 0x4049, 0x4049};
        int which = random(4);
        if (mode >= 2) which = points <= 0 ? 5 : 4;
        const std::vector<std::string> lines = {dataString(dataWord(static_cast<uint16_t>(0x1E4 + 4 * which))),
                                                dataString(dataWord(0x1FC))};
        speechBox(bx, by, lines, 1, false);
        sound(kSpeech[which]);
        while (ctx_.platform.wavPlaying()) ctx_.pump();
        waitCountdown(14);
        sound(0x404F);
    } else {
        speechBox(bx, by, dataLines(0x226), 1, false);
        sound(0x4048);
        waitCountdown(10);
        while (ctx_.platform.wavPlaying()) ctx_.pump();
        sound(0x404F);
    }
    clearInput();
    waitOrClick(0x28);
    clearInput();
    select(previous);
    score_ += points;
    return won;
}

// --- the shared picture code (segment 12) ---------------------------------

void Mystery::loadPicture(int index, const int size[4]) {
    // g12_1438 / g13_058a: picture `index` (bitmap 22F0+i) with its 64
    // colours (a whole B,G,R palette resource, 205, 204, 206, 207... ;
    // entries 16-79 are used), scaled into the top left of screen 2, and
    // the board laid out to fit.
    pic_.index = index;
    fill(0x8A, 0x90, 0x16E, 0xE0, 0xF3);
    std::vector<uint8_t> raw;
    const uint16_t palette = static_cast<uint16_t>(index == 0 ? 0x205 : index == 1 ? 0x204 : 0x204 + index);
    if (ctx_.read(palette, raw) && raw.size() >= 0x30 + 0xC0) {
        for (int k = 0; k < 64; ++k) {
            const Rgb c{raw[0x30 + 3 * k + 2], raw[0x30 + 3 * k + 1], raw[0x30 + 3 * k]};
            ctx_.screens[1].palette[16 + k] = ctx_.screens[2].palette[16 + k] = ctx_.displayPalette[16 + k] = c;
        }
    }
    const int n = pic_.divisions, b = pic_.size;
    pic_.tileW = size[0] / n;
    pic_.tileH = size[1] / n;
    pic_.left = 0x140 - (pic_.tileW * b + (b - 1) * kGap) / 2;
    pic_.top = 0x100 - (pic_.tileH * b + (b - 1) * kGap) / 2;
    pic_.arrowsX = pic_.left - 20;
    pic_.arrowsY = pic_.top - 20;
    pic_.arrowsW = b * pic_.tileW + (b - 1) * kGap + 0x29;
    pic_.arrowsH = b * pic_.tileH + (b - 1) * kGap + 0x29;
    pic_.arrowDX = (pic_.tileW - 8) >> 1;
    pic_.arrowDY = (pic_.tileH - 8) >> 1;
    // g40_0030: scaled (the library's f41_0024), centred on (w/2, h/2).
    const Bitmap& bmp = ctx_.bitmap(static_cast<uint16_t>(0x22F0 + index));
    Screen& s2 = ctx_.screens[2];
    for (int y = 0; y < 0xC8; ++y) std::fill_n(s2.pixels.begin() + static_cast<size_t>(y) * Screen::kWidth, 0x140, 0);
    const int dw = bmp.width * size[2] / 256, dh = bmp.height * size[3] / 256;
    const int x0 = size[0] / 2 - dw / 2, y0 = size[1] / 2 - dh / 2;
    const uint32_t stepX = scaleStep(size[2]), stepY = scaleStep(size[3]);
    for (int y = 0; y < dh; ++y)
        for (int x = 0; x < dw; ++x) {
            const int px = x0 + x, py = y0 + y;
            if (px < 0 || py < 0 || px >= Screen::kWidth || py >= Screen::kHeight) continue;
            s2.pixels[static_cast<size_t>(py) * Screen::kWidth + px] = bmp.at(static_cast<int>(x * stepX >> 16), static_cast<int>(y * stepY >> 16));
        }
    select(1);
}

void Mystery::pickPicture(const int size[4]) {
    // g12_1ab0: a random picture (g12_1976 works one out from the square,
    // then draws a random one anyway); on a custom level the player looks
    // through the 17 (Space next, Tab back) and picks one (Enter or Esc).
    // g12_1438 shows the whole picture on a custom level.
    if (!customLevel_) {
        loadPicture(random(kPictures), size);
        return;
    }
    int index = 0;
    loadPicture(index, size);
    drawSolution();
    drawOpaque(0xB8, 0x10, 0x2059);
    text(0xDC, 0x12, dataString(0x23DF), 0xFF);  // SPACE/TAB to view pics
    text(0xDC, 0x21, dataString(0x23F6), 0xFF);  // ENTER to select pic
    for (bool chosen = false; !chosen;) {
        ctx_.pump();
        for (int key; (key = ctx_.platform.takeKey()) != 0;) {
            if (key == Platform::kEnter || key == Platform::kEscape) {
                chosen = true;
                break;
            }
            if (key != ' ' && key != Platform::kTab) continue;
            index = key == ' ' ? (index + 1) % kPictures : (index + kPictures - 1) % kPictures;
            loadPicture(index, size);
            drawSolution();
        }
    }
    drawOpaque(0xB8, 0x10, 0x2058);
}

void Mystery::drawBoard() {
    // g12_03b2: every cell, a tile cut from the picture or black.
    for (int r = 0; r < pic_.size; ++r)
        for (int c = 0; c < pic_.size; ++c) {
            const int x = pic_.left + c * (pic_.tileW + kGap), y = pic_.top + r * (pic_.tileH + kGap);
            const int v = pic_.board[r][c];
            if (v == kBlank) {
                fill(x, y, pic_.tileW, pic_.tileH, 0);
            } else {
                duplicateArea(2, 1, (v % pic_.divisions) * pic_.tileW, (v / pic_.divisions) * pic_.tileH, pic_.tileW,
                              pic_.tileH, x, y);
            }
        }
}

void Mystery::drawSolution() {
    // g12_06b8: the finished picture on the board (inside the blank ring
    // of the arrow puzzle; without the last tile in the slide puzzle).
    for (int r = 0; r < pic_.size; ++r)
        for (int c = 0; c < pic_.size; ++c) {
            const int x = pic_.left + c * (pic_.tileW + kGap), y = pic_.top + r * (pic_.tileH + kGap);
            int tr = r, tc = c;
            bool blank = false;
            if (pic_.mode == 1) {
                blank = r == 0 || c == 0 || r == pic_.divisions + 1 || c == pic_.divisions + 1;
                tr = r - 1, tc = c - 1;
            } else if (pic_.mode == 2) {
                blank = r == pic_.size - 1 && c == pic_.size - 1;
            }
            if (blank)
                fill(x, y, pic_.tileW, pic_.tileH, 0);
            else
                duplicateArea(2, 1, tc * pic_.tileW, tr * pic_.tileH, pic_.tileW, pic_.tileH, x, y);
        }
}

void Mystery::checkPicture() {
    // g12_024a: solved when the tiles 0, 1, 2... lie in order in an N x N
    // block that starts at the first non-blank cell (in the slide puzzle
    // the blank may stand in for the last tile, bottom right).
    int r0 = -1, c0 = -1;
    for (int r = 0; r < pic_.size && r0 < 0; ++r)
        for (int c = 0; c < pic_.size; ++c)
            if (pic_.board[r][c] != kBlank) {
                r0 = r, c0 = c;
                break;
            }
    bool ok = r0 >= 0;
    int want = 0;
    for (int r = r0; ok && r < r0 + pic_.divisions; ++r)
        for (int c = c0; c < c0 + pic_.divisions; ++c) {
            const int v = r < 8 && c < 8 ? pic_.board[r][c] : -1;
            if (v == want) {
                ++want;
                continue;
            }
            if (pic_.mode == 2 && r == pic_.size - 1 && c == pic_.size - 1 && v == kBlank) break;
            ok = false;
            break;
        }
    if (ok) {
        pic_.solved = true;
        pic_.state = 2;
        drawOpaque(0xB8, 0x10, 0x2059);
        text(0xEE, 0x18, dataString(0x2223), 0xFF);
    } else if (pic_.solved) {
        drawOpaque(0xB8, 0x10, 0x2058);
        pic_.solved = false;
    }
}

void Mystery::redrawLine(int row, int col) {
    // g12_04b6: one row (or column) of the board after a move; the first
    // move starts the clock.
    if (pic_.mode < 0) return;
    int x = row != -1 ? pic_.left : pic_.left + col * (pic_.tileW + kGap);
    int y = row != -1 ? pic_.top + row * (pic_.tileH + kGap) : pic_.top;
    for (int i = 0; i < pic_.size; ++i) {
        const int v = row != -1 ? pic_.board[row][i] : pic_.board[i][col];
        if (v == kBlank)
            fill(x, y, pic_.tileW, pic_.tileH, 0);
        else
            duplicateArea(2, 1, (v % pic_.divisions) * pic_.tileW, (v / pic_.divisions) * pic_.tileH, pic_.tileW,
                          pic_.tileH, x, y);
        if (row != -1)
            x += pic_.tileW + kGap;
        else
            y += pic_.tileH + kGap;
    }
    checkPicture();
    ++pic_.moves;
    if (pic_.timerIdle) {
        pic_.timerIdle = false;
        ctx_.timer.setPeriodic(kPuzzleSecondSlot, 1, [this] {  // g12_13ae
            if (pic_.timeLeft > 0) --pic_.timeLeft;
        });
    }
}

void Mystery::shove(int arrow) {
    // g12_0846: an arrow pushes its column (0-7 down, 8-15 up) or row
    // (16-23 right, 24-31 left) one cell, into the blank nearest the far
    // end; the cell it leaves becomes blank.
    auto& b = pic_.board;
    const int last = pic_.size - 1;
    if (arrow < 8) {
        const int c = arrow;
        int r = last;
        while (r >= 1 && b[r][c] != kBlank) --r;
        if (r < 1) return;
        for (; r > 0; --r) b[r][c] = b[r - 1][c];
        b[0][c] = kBlank;
        redrawLine(-1, c);
    } else if (arrow < 16) {
        const int c = arrow - 8;
        for (int r = 0; r < last; ++r)
            if (b[r][c] == kBlank) {
                for (; r < last; ++r) b[r][c] = b[r + 1][c];
                b[last][c] = kBlank;
                redrawLine(-1, c);
                return;
            }
    } else if (arrow < 24) {
        const int r = arrow - 16;
        int c = last;
        while (c >= 1 && b[r][c] != kBlank) --c;
        if (c < 1) return;
        for (; c > 0; --c) b[r][c] = b[r][c - 1];
        b[r][0] = kBlank;
        redrawLine(r, -1);
    } else {
        const int r = arrow - 24;
        for (int c = 0; c < last; ++c)
            if (b[r][c] == kBlank) {
                for (; c < last; ++c) b[r][c] = b[r][c + 1];
                b[r][last] = kBlank;
                redrawLine(r, -1);
                return;
            }
    }
}

void Mystery::addArrows() {
    // g12_0f8c: an arrow above and below each column and beside each row,
    // as buttons of the panel at DS:2394.
    Panels::Panel panel;
    panel.x = pic_.arrowsX, panel.y = pic_.arrowsY, panel.w = pic_.arrowsW, panel.h = pic_.arrowsH;
    panel.buttons.resize(32, {0, 0, 0, 0});
    const int n = pic_.size;
    const int right = pic_.left + n * pic_.tileW + (n - 1) * kGap;
    const int bottom = pic_.top + n * pic_.tileH + (n - 1) * kGap;
    for (int i = 0; i < n; ++i) {
        const int cx = pic_.left + i * (pic_.tileW + kGap) - pic_.arrowsX;
        const int ry = pic_.top + i * (pic_.tileH + kGap) - pic_.arrowsY;
        panel.buttons[i] = {cx, 0, pic_.tileW, 0x12};
        panel.buttons[i + 8] = {cx, bottom - pic_.arrowsY, pic_.tileW, 0x12};
        panel.buttons[i + 16] = {0, ry, 0x12, pic_.tileH};
        panel.buttons[i + 24] = {right - pic_.arrowsX, ry, 0x12, pic_.tileH};
    }
    // Where each arrow's sprite goes, and its up / pressed pictures.
    auto face = [this, buttons = panel.buttons](int i, bool down) {
        const Panels::Button& b = buttons[i];
        const int x = pic_.arrowsX + b.x, y = pic_.arrowsY + b.y;
        if (i < 8)
            drawLogo(x + pic_.arrowDX, y + 8, down ? 0x2053 : 0x2052);
        else if (i < 16)
            drawLogo(x + pic_.arrowDX, y + 2, down ? 0x2051 : 0x2050);
        else if (i < 24)
            drawLogo(x + 8, y + pic_.arrowDY, down ? 0x2057 : 0x2056);
        else
            drawLogo(x + 2, y + pic_.arrowDY, down ? 0x2055 : 0x2054);
    };
    for (int i = 0; i < n; ++i)
        for (int group = 0; group < 4; ++group) face(i + group * 8, false);
    panel.onPress = [this, face, n](int b) {  // g12_0d16
        if (b < 0 || b % 8 >= n) return;
        music(0x12);
        face(b, true);
        shove(b);
    };
    panel.onRelease = [face, n](int b) {  // g12_0e5c
        if (b >= 0 && b % 8 < n) face(b, false);
    };
    panels_.add(panel);
}

void Mystery::countBonus() {
    // g12_12d6: the score is 4 points a second left; once solved, the clock
    // stops, 100 points are added, and the seconds left count down into
    // the score at 2 points each.
    int points = pic_.timeLeft * 4;
    const int fh = font_->height();
    if (pic_.solved) {
        ctx_.timer.setPeriodic(kPuzzleSecondSlot, 0, nullptr);
        pic_.used = pic_.timeLimit - pic_.timeLeft;
        points += 100;
        const int left = pic_.timeLeft;
        while (pic_.timeLeft > 0) {
            --pic_.timeLeft;
            digitalTime(0x1EE, 0x2F, 0x2E, fh + 2, pic_.timeLeft);
            points += 2;
            intBox(0x1FE, 0x11, 0x2C, fh + 2, points);
        }
        digitalTime(0x1EE, 0x2F, 0x2E, fh + 2, left);
    } else {
        intBox(0x1FE, 0x11, 0x2C, fh + 2, points);
    }
    pic_.points = points;
}

void Mystery::puzzleClock() {
    // g12_13ce: the time left and the score, when a second has gone.
    if (pic_.timeLeft == pic_.shownTime) return;
    digitalTime(0x1EE, 0x2F, 0x2E, font_->height() + 2, pic_.timeLeft);
    pic_.shownTime = pic_.timeLeft;
    countBonus();
}

void Mystery::pictureGizmo() {
    // g12_00d6: the gadget at the bottom right blinks, then (in the main
    // loop) pictureShow runs.
    if (pic_.gizmo) return;
    pic_.gizmo = true;
    music(0x1A);
    for (int i = 0; i < 8; ++i) {
        drawOpaque(0x232, 0x10C, static_cast<uint16_t>(0x2222 + (i & 1)));
        waitCountdown(2);
    }
    drawOpaque(0x232, 0x10C, 0x2221);
}

void Mystery::pictureShow() {
    // g12_0168: a little film at the top left.
    static const uint8_t kFrames[] = {0, 1, 2, 0x80, 3, 4, 0x80, 3, 5, 0x80, 3, 2, 1, 0, 6};
    select(1);
    music(0x1B);
    for (uint8_t f : kFrames) {
        if (f == 0x80) {
            waitCountdown(4);
        } else {
            drawOpaque(0x10, 0xB8, static_cast<uint16_t>(0x221A + f));
            waitCountdown(1);
        }
    }
    pic_.gizmo = false;
}

void Mystery::addPicturePanels(uint16_t help) {
    // DS:223A: hold to see the whole picture (and its name); DS:21E6: give
    // up; DS:220A: the gadget.
    Panels::Panel view;
    view.x = 0x118, view.y = 0x54, view.w = 0x6A, view.h = 0x18;
    view.buttons = {{0, 0, 0x6A, 0x18}};
    view.onPress = [this](int b) {  // g12_0b74
        if (b < 0) return;
        music(0x1C);
        drawOpaque(0x130, 0x53, 0x204F);
        drawOpaque(0xB8, 0x10, 0x2059);
        const std::string name = dataString(dataWord(static_cast<uint16_t>(0x2194 + 4 * pic_.index)));
        if (font_->width(name) < 0xDC) {
            text(0xCA, 0x12, name, 0xFF);
        } else {
            size_t at = 0;
            for (int line = 0; line < 2; ++line) {
                std::string part;
                while (at < name.size() && part.size() <= 0x14) part += name[at++];
                while (at < name.size() && name[at] != ' ' && part.size() < 0x1B) part += name[at++];
                text(0xCA, 0x12 + 16 * line, part, 0xFF);
            }
        }
        drawSolution();
    };
    view.onRelease = [this](int b) {  // g12_0cd4
        if (b < 0) return;
        drawOpaque(0x130, 0x53, 0x204E);
        drawOpaque(0xB8, 0x10, 0x2058);
        drawBoard();
    };
    panels_.add(view);
    Panels::Panel quit;
    quit.x = 0x258, quit.y = 0xBA, quit.w = 0x1E, quit.h = 0x40;
    quit.buttons = {{0, 0, 0x1E, 0x40}};
    quit.onPress = [this](int b) {  // g12_008a
        if (b >= 0 && pic_.state == 0) pic_.state = 1;
    };
    panels_.add(quit);
    Panels::Panel gizmo;
    gizmo.x = 0x232, gizmo.y = 0x10C, gizmo.w = 0x38, gizmo.h = 0x1A;
    gizmo.buttons = {{0, 0, 0x38, 0x1A}};
    gizmo.onPress = [this](int b) {
        if (b >= 0) pictureGizmo();
    };
    panels_.add(gizmo);
    (void)help;
}

// --- 13: the Arrow Puzzle (with many blanks) -------------------------------

bool Mystery::arrowPuzzle(int level) {
    // g12_1c60. The board has a ring of extra cells; N x N tiles and
    // 4N + 4 blanks are dealt at random, and the arrows shove rows and
    // columns until the picture is whole anywhere on the board.
    level = std::min(level, 2);
    static const int kN[3] = {2, 3, 4}, kSeconds[3] = {0xF0, 0x1E0, 0x2D0};  // DS:1FD4
    clearInput();
    ctx_.blackout();
    ctx_.showFullScreen(0x1012, 2);
    pic_ = Picture{};
    pic_.mode = 1;
    pic_.timeLeft = pic_.timeLimit = kSeconds[level];
    pic_.divisions = kN[level];
    pic_.size = pic_.divisions + 2;
    const int blanks = pic_.divisions * 4 + 4;
    for (auto& row : pic_.board) std::fill(std::begin(row), std::end(row), -1);
    panels_.clear();
    select(2);
    drawOpaque(0x232, 0x10C, 0x2221);
    drawOpaque(0x6C, 0x12, 0x205C);
    fill(0x8A, 0x90, 0x16E, 0xE0, 0xF3);
    select(1);
    show(2);
    computeUiColours();
    const int cells = pic_.size * pic_.size;
    for (int i = 0; i < pic_.divisions * pic_.divisions + blanks; ++i) {
        int k;
        do k = random(cells);
        while (pic_.board[k / pic_.size][k % pic_.size] != -1);
        pic_.board[k / pic_.size][k % pic_.size] = i < blanks ? kBlank : i - blanks;
    }
    // DS:1F2C, entries 6-8 (every picture uses them for N = 2-4).
    static const int kSize[3][4] = {{172, 100, 137, 128}, {204, 120, 163, 153}, {224, 128, 179, 163}};
    pickPicture(kSize[level]);
    addPicturePanels(0x3F01);
    drawBoard();
    addArrows();
    helpPanel(0x6C, 0x12, 0x48, 0x22);
    return pictureLoop(0x3F01);
}

// --- the loop they share ------------------------------------------------

bool Mystery::pictureLoop(uint16_t helpText) {
    // g12_1c60 / g13_0d44 / g14_028a's main loop: until the picture is
    // whole (and the bonus counted) or the player pulls the exit lever.
    clearInput();
    for (;;) {
        // The slide puzzle's loop first: Esc held ([929C]) ends the game as
        // it stands, without the lever (14:0552).
        if (pic_.mode == 2 && ctx_.platform.escapeHeld()) break;
        panels_.poll(ctx_.platform);
        ctx_.pump();
        puzzleClock();
        if (pic_.state != 0) {
            drawOpaque(0x25C, 0xBB, 0x205A);
            waitCountdown(4);
            if (pic_.state == 2) {
                puzzleClock();
                countBonus();
            } else {
                pic_.points = 0;
            }
            break;
        }
        if (pic_.gizmo) pictureShow();
        idleHint(0);
        if (helpPressed_) {
            drawOpaque(0x6C, 0x12, 0x205B);
            help(helpText);
            drawOpaque(0x6C, 0x12, 0x205C);
        }
    }
    ctx_.timer.setPeriodic(kPuzzleSecondSlot, 0, nullptr);
    clearInput();
    panels_.clear();
    return puzzleResult(pic_.solved, pic_.points, 0, pic_.used);
}

// --- 10: the Slide Puzzle (with 1 blank) --------------------------------

void Mystery::addBoardPanel() {
    // g13_0458: a button over each cell (numbered row * 6 + col here; the
    // switch puzzle numbers them row * 8 + col).
    Panels::Panel panel;
    const int n = pic_.divisions, stride = pic_.mode == 0 ? 8 : 6;
    panel.x = pic_.left, panel.y = pic_.top;
    panel.w = n * pic_.tileW + (n - 1) * kGap;
    panel.h = n * pic_.tileH + (n - 1) * kGap;
    panel.buttons.resize(64, {0, 0, 0, 0});
    for (int r = 0; r < stride; ++r)
        for (int c = 0; c < stride; ++c)
            panel.buttons[r * stride + c] = {c * (pic_.tileW + kGap), r * (pic_.tileH + kGap), pic_.tileW, pic_.tileH};
    panel.onPress = [this](int b) {  // g13_0000: the switch puzzle marks a tile
        if (b < 0 || pic_.mode != 0) return;
        const int r = b / 8, c = b % 8;
        if (r >= pic_.size || c >= pic_.size) return;
        music(0x12);
        const int x = pic_.left + c * (pic_.tileW + kGap), y = pic_.top + r * (pic_.tileH + kGap);
        // MALL's line (f06_1af8) on the display: each line's far end left
        // off, so the bottom right corner stays.
        displayLine(x, y, x + pic_.tileW - 1, y, 0xC2);
        displayLine(x, y + pic_.tileH - 1, x + pic_.tileW - 1, y + pic_.tileH - 1, 0xC2);
        displayLine(x, y, x, y + pic_.tileH - 1, 0xC2);
        displayLine(x + pic_.tileW - 1, y, x + pic_.tileW - 1, y + pic_.tileH - 1, 0xC2);
        if (pic_.pickedCount == 0) pic_.picked = b;
        ++pic_.pickedCount;
    };
    panel.onRelease = [this](int b) {  // g13_01ec
        if (b < 0) return;
        if (pic_.mode != 0) {
            slideTo(b);
            return;
        }
        // The second tile marked: the two change places.
        if (pic_.pickedCount < 2) return;
        if (pic_.timerIdle) {
            pic_.timerIdle = false;
            ctx_.timer.setPeriodic(kPuzzleSecondSlot, 1, [this] {
                if (pic_.timeLeft > 0) --pic_.timeLeft;
            });
        }
        int& a = pic_.board[b / 8][b % 8];
        int& o = pic_.board[pic_.picked / 8][pic_.picked % 8];
        std::swap(a, o);
        for (int cell : {b, pic_.picked}) {
            const int r = cell / 8, c = cell % 8, v = pic_.board[r][c];
            duplicateArea(2, 1, (v % pic_.divisions) * pic_.tileW, (v / pic_.divisions) * pic_.tileH, pic_.tileW,
                          pic_.tileH, pic_.left + c * (pic_.tileW + kGap), pic_.top + r * (pic_.tileH + kGap));
        }
        pic_.pickedCount = 0;
        ++pic_.moves;
        checkPicture();
    };
    panels_.add(panel);
}

void Mystery::slideTo(int cell) {
    // g14_0000: clicking a tile in the blank's row or column slides the
    // tiles between them one step, and the blank moves to the click.
    const int r = cell / 6, c = cell % 6, br = pic_.blank / 6, bc = pic_.blank % 6;
    if (r >= pic_.size || c >= pic_.size || pic_.board[r][c] == kBlank) return;
    auto& b = pic_.board;
    if (r == br) {
        if (c < bc)
            for (int k = bc; k > c; --k) b[r][k] = b[r][k - 1];
        else
            for (int k = bc; k < c; ++k) b[r][k] = b[r][k + 1];
        b[r][c] = kBlank;
        pic_.blank = cell;
        if (pic_.mode > 0) redrawLine(r, -1);
    } else if (c == bc) {
        if (r < br)
            for (int k = br; k > r; --k) b[k][c] = b[k - 1][c];
        else
            for (int k = br; k < r; ++k) b[k][c] = b[k + 1][c];
        b[r][c] = kBlank;
        pic_.blank = cell;
        if (pic_.mode > 0) redrawLine(-1, c);
    }
}

bool Mystery::slidePuzzle(int level) {
    // g14_028a: N x N tiles (N = 2-6 by level) with the bottom right one
    // taken out; 20-39 random slides mix them up.
    level = std::min(level, 4);
    static const int kN[5] = {2, 3, 4, 5, 6}, kSeconds[5] = {0xF0, 0x168, 0x1E0, 0x258, 0x2D0};  // DS:2850
    clearInput();
    ctx_.blackout();
    ctx_.showFullScreen(0x1014, 2);
    pic_ = Picture{};
    pic_.timeLeft = pic_.timeLimit = kSeconds[level];
    pic_.divisions = pic_.size = kN[level];
    for (auto& row : pic_.board) std::fill(std::begin(row), std::end(row), -1);
    panels_.clear();
    select(2);
    drawOpaque(0x232, 0x10C, 0x2221);
    drawOpaque(0x6C, 0x12, 0x205C);
    fill(0x8A, 0x90, 0x16E, 0xE0, 0xF3);
    select(1);
    show(2);
    computeUiColours();
    const int n = pic_.size;
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c) pic_.board[r][c] = r == n - 1 && c == n - 1 ? kBlank : r * n + c;
    pic_.blank = n * 7 - 7;
    const int slides = random(20) + 20;
    for (int i = 0; i < slides; ++i) {
        const int k = random(n);
        if (random(500) < 250)
            slideTo(pic_.blank / 6 * 6 + k);
        else
            slideTo(k * 6 + pic_.blank % 6);
    }
    pic_.mode = 2;
    // g13_0bfe / g13_058a: a random picture; DS:242A's size for each N.
    static const int kSize[4][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {320, 200, 256, 256}, {300, 180, 240, 230}};
    static const int kSizeFor[5] = {2, 3, 2, 2, 3};
    loadPicture(random(kPictures), kSize[kSizeFor[level]]);
    addPicturePanels(0x3F03);
    addBoardPanel();
    drawBoard();
    helpPanel(0x6C, 0x12, 0x48, 0x22);
    return pictureLoop(0x3F03);
}

// --- 12: the Switch Puzzle ----------------------------------------------

bool Mystery::switchPuzzle(int level) {
    // g13_0d44: N x N tiles (N = 2-8 by level) dealt at random; clicking
    // two tiles swaps them.
    level = std::min(level, 6);
    clearInput();
    ctx_.blackout();
    ctx_.showFullScreen(0x1013, 2);
    pic_ = Picture{};
    pic_.mode = 0;
    pic_.divisions = pic_.size = level + 2;  // DS:2522: N, then N * 120 seconds
    pic_.timeLeft = pic_.timeLimit = pic_.divisions * 120;
    for (auto& row : pic_.board) std::fill(std::begin(row), std::end(row), -1);
    panels_.clear();
    select(2);
    drawOpaque(0x232, 0x10C, 0x2221);
    drawOpaque(0x6C, 0x12, 0x205C);
    fill(0x8A, 0x90, 0x16E, 0xE0, 0xF3);
    select(1);
    show(2);
    computeUiColours();
    const int n = pic_.size;
    for (int i = 0; i < n * n; ++i) {
        int k;
        do k = random(n * n);
        while (pic_.board[k / n][k % n] != -1);
        pic_.board[k / n][k % n] = i;
    }
    // DS:242A's size for each N (picture entries 2-8 of g13_058a's table).
    static const int kSize[6][4] = {{0, 0, 0, 0},       {0, 0, 0, 0},       {320, 200, 256, 256},
                                    {300, 180, 240, 230}, {308, 196, 246, 250}, {304, 192, 243, 245}};
    static const int kSizeFor[7] = {2, 3, 2, 2, 3, 4, 5};
    loadPicture(random(kPictures), kSize[kSizeFor[level]]);
    addPicturePanels(0x3F02);
    addBoardPanel();
    drawBoard();
    helpPanel(0x6C, 0x12, 0x48, 0x22);
    return pictureLoop(0x3F02);
}

}  // namespace edison
