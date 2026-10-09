// MALL.EXE segments 22-24: the end of a game. Edison and Smitty dance when
// every object is found (f23_10aa), the final quiz follows, then the bonus
// maze (segment 22), the happy or sad ending (f23_140a) and the high
// scores (segment 24, MYSTERY.HS).

#include <algorithm>
#include "formats/paths.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "mystery/mystery.h"

namespace edison {

namespace {

constexpr int kMazeSlot = 8;  // the maze's second (its own timer, g22_0100: the countdown's runs too)

// A pose of an end-screen animation: where it's drawn and its bitmap.
struct Pose {
    int x, y;
    uint16_t id;
};

// f23_0000 and f23_0a6a build their animations from a list of frame
// numbers and a table of positions indexed by frame.
std::vector<Pose> poses(std::initializer_list<int> frames, uint16_t first, const std::pair<int, int>* at) {
    std::vector<Pose> out;
    for (int f : frames) out.push_back({at[f].first, at[f].second, static_cast<uint16_t>(first + f)});
    return out;
}

// Edison walking in from the left (the positions both endings use).
constexpr std::pair<int, int> kWalk[15] = {
    {0x4E, 0x102}, {0x4E, 0x102}, {0x56, 0x102}, {0x70, 0x102}, {0x98, 0x102},
    {0xBE, 0x102}, {0xBE, 0x102}, {0xBE, 0x102}, {0xBE, 0x102}, {0xBE, 0x102},
    {0xBE, 0x102}, {0xBE, 0x102}, {0xBE, 0x102}, {0xBE, 0x102}, {0xBE, 0x102},
};

// The bonus maze: 24 x 15 cells of 24 pixels.
constexpr int kCols = 24, kRows = 15, kCell = 0x18;
// DS:4ACC / 4AD4: the four directions (up, left, down, right), then the
// diagonals.
constexpr int kDx[8] = {0, -1, 0, 1, -1, 1, -1, 1};
constexpr int kDy[8] = {-1, 0, 1, 0, -1, -1, 1, 1};

}  // namespace

// --- the end of a game (f09_1dd8, after the map loop) --------------------

void Mystery::endOfGame() {
    // The countdown runs on through it all (the office's clock shows it
    // when the ending redraws the map): the office stops it after the
    // high scores (09:2FCA).
    if (outcome_ == 1) {
        endScreen(false, false);  // out of time
    } else {
        allFoundDance();
        if (questionPeriod(player_.level, false)) {
            // The maze by level: 2 for 0-1, 5 for 2-5, 8 for 6-7.
            const int level = player_.level;
            bonusMaze(level <= 1 ? 2 : level <= 5 ? 5 : level <= 7 ? 8 : 0);
            endScreen(true, true);
            addHighScore();
        } else {
            endScreen(false, true);
        }
    }
    showHighScores();
}

void Mystery::allFoundDance() {
    // f23_10aa: Edison runs along the bottom of the map, jumping, while
    // Smitty hops up and down; then "Hey!".
    static const int kFrame[16] = {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2};
    static const int kX[16] = {0x256, 0x234, 0x214, 0x1F0, 0x1D0, 0x1AC, 0x18C, 0x168,
                               0x148, 0x124, 0x104, 0xE0,  0xC0,  0x9C,  0x7C,  0x72};
    constexpr int kStripX = 0x64, kStripY = 0x15C, kStripW = 0x21C, kStripH = 0x34;
    constexpr int kSmittyX = 0x6C, kSmittyY = 0xF0, kSmittyW = 0x32, kSmittyH = 0x5E;
    const int previous = current();
    select(2);
    duplicateArea(1, 3, kStripX, kStripY, kStripW, kStripH, 0, 0);
    duplicateArea(1, 3, kSmittyX, kSmittyY, kSmittyW, kSmittyH, 0, 0x36);
    for (int i = 0; i < 16; ++i) {
        duplicateArea(3, 2, 0, 0, kStripW, kStripH, kStripX, kStripY);
        drawLogo(kX[i], kStripY, static_cast<uint16_t>(0x2349 + kFrame[i]));
        copyArea(2, 1, kStripX, kStripY, kStripW, kStripH);
        if (i == 7 || i == 9 || i == 11 || i == 13) {
            duplicateArea(3, 2, 0, 0x36, kSmittyW, kSmittyH, kSmittyX, kSmittyY);
            if (i != 13) drawLogo(kSmittyX, kSmittyY, i == 9 ? 0x234D : 0x234C);
            copyArea(2, 1, kSmittyX, kSmittyY, kSmittyW, kSmittyH);
        }
        waitCountdown(2);
    }
    duplicateArea(3, 2, 0, 0, kStripW, kStripH, kStripX, kStripY);
    copyArea(2, 1, kStripX, kStripY, kStripW, kStripH);
    select(1);
    speechBox(Screen::kWidth * 2 / 3, Screen::kHeight / 3, dataLines(0x4BAA), 2, false);
    sound(0x4061);
    mapTalk(1, 4);
    waitCountdown(4);
    select(previous);
}

void Mystery::endScreen(bool happy, bool redraw) {
    // f23_140a: the map again (after the quiz), then the happy ending
    // (f23_0000) or the sad one (f23_0a6a).
    const int previous = current();
    if (redraw) {
        select(2);
        ctx_.showFullScreen(0x1003, 2);
        applyColours(2, false);
        drawObjects(0);
        clock(true);
        drawLogo(0x1FC, 0x158, 0x22BB);
        drawOpaque(0x176, 0x12E, 0x21E4);
        number(0x13A, 0x32, 0x38, 0x10, score_);
        drawLogo(0, 0x4A, 0x22AD);
        show(2);
    } else {
        copyArea(2, 1, 0x1DA, 0xE8, 0xA6, 0xAC);
        select(2);
        drawLogo(0, 0x4A, 0x22AD);
        copyArea(2, 1, 0x6C, 0xF0, 0x32, 0x5E);
    }
    if (happy)
        happyEnding();
    else
        sadEnding();
    select(previous);
}

void Mystery::happyEnding() {
    // f23_0000: Smitty (A) and Edison (B, walking in; C, a pose at the
    // right) celebrate; then the "Happy faces" picture.
    static const std::pair<int, int> kSmitty[4] = {{0x1CF, 0xEC}, {0x1E7, 0xE6}, {0x1E7, 0xE6}, {0x1E7, 0xE6}};
    static const std::pair<int, int> kPose[5] = {{0x232, 0xEC}, {0x1CC, 0xE7}, {0x1B5, 0xE8}, {0x181, 0xE8}, {0x188, 0xE8}};
    const auto a = poses({0, 1, 2, 3, 1, 3, 2}, 0x2345, kSmitty);
    const auto b = poses({0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 10, 11, 12, 13, 14, 10, 7, 8},
                         0x2331, kWalk);
    const auto c = poses({0, 1, 2, 3, 4}, 0x2340, kPose);
    constexpr int kLeftX = 0x64, kLeftY = 0xD2, kLeftW = 0x106, kLeftH = 0xBE;
    constexpr int kRightX = 0x172, kRightY = 0xD3, kRightW = 0x10E, kRightH = 0xBC;
    auto draw = [&](const Pose& p) { drawLogo(p.x, p.y, p.id); };

    select(1);
    copyArea(2, 1, kRightX, kRightY, kRightW, kRightH);
    draw(a[0]);
    draw(a[1]);
    select(2);
    duplicateArea(1, 3, kLeftX, kLeftY, kLeftW, kLeftH, 0, 0);
    sound(0x4060);
    for (const Pose& p : b) {
        draw(p);
        copyArea(2, 1, kLeftX, kLeftY, kLeftW, kLeftH);
        waitCountdown(2);
        duplicateArea(3, 2, 0, 0, kLeftW, kLeftH, kLeftX, kLeftY);
    }
    duplicateArea(3, 2, 0, 0, kLeftW, kLeftH, kLeftX, kLeftY);
    draw(b.back());
    copyArea(2, 1, kLeftX, kLeftY, kLeftW, kLeftH);
    select(2);
    draw(c[0]);
    duplicateArea(2, 3, kRightX, kRightY, kRightW, kRightH, 0, 0);
    for (size_t k = 1; k < c.size(); ++k) {
        draw(c[k]);
        copyArea(2, 1, kRightX, kRightY, kRightW, kRightH);
        waitCountdown(2);
        duplicateArea(3, 2, 0, 0, kRightW, kRightH, kRightX, kRightY);
    }
    draw(c.back());
    const Bitmap& last = ctx_.bitmap(c.back().id);
    copyArea(2, 1, c.back().x, c.back().y, last.width, last.height);
    waitCountdown(5);

    select(1);
    speechBox(Screen::kWidth * 3 / 5, Screen::kHeight * 2 / 5, dataLines(0x4B50), 3, false);
    sound(0x401B);
    music(0x40);
    for (size_t k = 1; k < a.size(); ++k) {
        draw(a[k]);
        waitCountdown(2);
    }
    draw(a.back());
    waitCountdown(15);

    // The picture: "Happy faces of Edison, Smitty, and <name>".
    select(2);
    backdrop(0x100F);
    text(0x13E, 0x122, dataString(0x4B5C), 0);
    text(0x13E, 0x13A, dataString(0x4B7B), 0);
    text(font_->width(dataString(0x4B7F)) + 0x142, 0x13A, player_.name, 0);
    show(2);
    clearInput();
    ctx_.countdown[0] = 0x3C;
    while (ctx_.countdown[0] != 0) ctx_.pump();
    clearInput();
    waitOrClick(0xC8);
    clearInput();
}

void Mystery::sadEnding() {
    // f23_0a6a: "We lost!" Smitty (E) sulks while Edison (D) walks in.
    static const std::pair<int, int> kSmitty[3] = {{0x233, 0x13C}, {0x21E, 0x84}, {0x21E, 0x84}};
    const auto d = poses({0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 9}, 0x2323, kWalk);
    const auto e = poses({0, 1, 2, 1, 1, 2}, 0x232D, kSmitty);
    constexpr int kLeftX = 0x64, kLeftY = 0xD2, kLeftW = 0x106, kLeftH = 0xBE;
    auto draw = [&](const Pose& p) { drawLogo(p.x, p.y, p.id); };
    auto sulk = [&] {
        for (size_t k = 1; k < e.size(); ++k) {
            draw(e[k]);
            waitCountdown(2);
        }
        draw(e.back());
    };

    select(1);
    sound(0x401D);
    speechBox(Screen::kWidth * 2 / 3, Screen::kHeight / 3, dataLines(0x4B90), 2, false);  // 23:0d5a
    draw(e[0]);
    sulk();
    select(2);
    duplicateArea(1, 3, kLeftX, kLeftY, kLeftW, kLeftH, 0, 0);
    for (const Pose& p : d) {
        draw(p);
        copyArea(2, 1, kLeftX, kLeftY, kLeftW, kLeftH);
        waitCountdown(3);
        duplicateArea(3, 2, 0, 0, kLeftW, kLeftH, kLeftX, kLeftY);
    }
    draw(d.back());
    copyArea(2, 1, kLeftX, kLeftY, kLeftW, kLeftH);
    sound(0x401D);
    select(1);
    sulk();
    waitCountdown(0x14);
    clearInput();
}

// --- high scores (segment 24) --------------------------------------------

std::string Mystery::highScorePath() const {
    return options_.saveDir + "/" + dataString(0x4BC1);  // MYSTERY.HS
}

void Mystery::loadHighScores() {
    // f24_005a: nine tables (levels 0-7, then custom levels) of ten
    // entries: a 9-byte name and a 32-bit score. A missing file is made.
    for (auto& table : highScores_)
        for (auto& entry : table) entry = {};
    std::ifstream in(findPath(highScorePath()), std::ios::binary);
    uint8_t raw[9 * 10 * 13];
    if (!in.read(reinterpret_cast<char*>(raw), sizeof raw)) {
        saveHighScores();
        return;
    }
    for (int t = 0; t < 9; ++t)
        for (int i = 0; i < 10; ++i) {
            const uint8_t* e = raw + t * 0x82 + i * 13;
            HighScore& h = highScores_[t][i];
            h.name.assign(reinterpret_cast<const char*>(e), strnlen(reinterpret_cast<const char*>(e), 9));
            h.score = static_cast<int32_t>(e[9] | e[10] << 8 | e[11] << 16 | static_cast<uint32_t>(e[12]) << 24);
        }
}

void Mystery::saveHighScores() const {
    // f24_0000.
    uint8_t raw[9 * 10 * 13] = {};
    for (int t = 0; t < 9; ++t)
        for (int i = 0; i < 10; ++i) {
            uint8_t* e = raw + t * 0x82 + i * 13;
            const HighScore& h = highScores_[t][i];
            std::memcpy(e, h.name.data(), std::min<size_t>(h.name.size(), 8));
            const uint32_t s = static_cast<uint32_t>(h.score);
            for (int k = 0; k < 4; ++k) e[9 + k] = static_cast<uint8_t>(s >> (8 * k));
        }
    std::error_code ec;
    std::filesystem::create_directories(options_.saveDir, ec);
    std::ofstream out(highScorePath(), std::ios::binary);
    out.write(reinterpret_cast<const char*>(raw), sizeof raw);
    if (!out) warnOnce("can't write " + highScorePath());
}

bool Mystery::addHighScore() {
    // f24_03f6: the score goes in above the first entry it matches or
    // beats (an empty entry scores 0); the list is saved.
    auto& table = highScores_[customLevel_ ? 8 : player_.level];
    for (int i = 0; i < 10; ++i) {
        if (score_ < table[i].score) continue;
        for (int j = 8; j >= i; --j) table[j + 1] = table[j];
        table[i] = {player_.name.substr(0, 8), static_cast<int32_t>(score_)};
        saveHighScores();
        return true;
    }
    return false;
}

void Mystery::showHighScores() {
    // f24_0112: the level's list over backdrop 1010, with an animated
    // trophy, until a click, a key or 40 seconds.
    clearInput();
    backdrop(0x1010);
    select(2);
    const int level = customLevel_ ? 8 : player_.level;
    text(0xD0, 0x64, dataString(0x4BCC), 0);   // NAME
    text(0x15E, 0x64, dataString(0x4BD1), 0);  // SCORE
    if (level < 8) {
        text(0x10E, 0x122, dataString(0x4BD7), 0);  // LEVEL:
        text(0x14A, 0x122, std::to_string(level + 6), 0);
    } else {
        text(0xF0, 0x122, dataString(0x4BE1), 0);  // LEVEL:  CUSTOM
    }
    for (int i = 0; i < 10; ++i) {
        const HighScore& h = highScores_[level][i];
        if (h.name.empty()) {
            if (i == 0) text(0x118, 0xB8, dataString(0x4BF0), 0xB0);  // NONE YET!
            break;
        }
        text(0xD0, i * 16 + 0x78, h.name, 0xB0);
        text(0x15E, i * 16 + 0x78, std::to_string(h.score), 0xB0);
    }
    select(1);
    show(2);
    ctx_.countdown[0] = 2;
    select(2);
    constexpr int kX = 0x6B, kY = 0x84, kW = 0x3C, kH = 0x3E;
    duplicateArea(2, 3, kX, kY, kW, kH, 0, 0);
    ctx_.countdown[1] = 10;
    while (ctx_.countdown[1] != 0) ctx_.pump();
    ctx_.countdown[1] = 0x190;
    clearInput();
    for (int frame = 0; ctx_.countdown[1] != 0;) {
        ctx_.pump();
        if (anyInput()) break;
        if (ctx_.countdown[0] != 0) continue;
        drawLogo(kX, kY, static_cast<uint16_t>(0x2284 + frame));
        copyArea(2, 1, kX, kY, kW, kH);
        duplicateArea(3, 2, 0, 0, kW, kH, kX, kY);
        frame = (frame + 1) % 3;
        ctx_.countdown[0] = 2;
    }
    select(1);
    clearInput();
}

// --- the bonus maze (segment 22) -----------------------------------------
//
// Edison walks a maze to the exit (O) before the time runs out; three
// wanderers roam it but do no harm, turning back when they see him. From
// level 3 the maze starts hidden and each step shows what Edison can see;
// from level 6 it's forgotten again after each step. Reaching the exit is
// worth 100 points and 2 for each second left.

bool Mystery::Maze::seen(int x, int y) const {
    return x >= 0 && x < kCols && y >= 0 && y < kRows && visible[y][x];
}

void Mystery::Maze::see(int x, int y) {
    if (x >= 0 && x < kCols && y >= 0 && y < kRows) visible[y][x] = true;
}

void Mystery::mazeLook(int dx, int dy) {
    // f22_0172: along one direction until a wall, marking each cell and
    // its neighbours seen. A wanderer on the way coming towards Edison
    // notices him.
    Maze& m = maze_;
    int x = m.x + dx, y = m.y + dy;
    while (x >= 0 && x < kCols && y >= 0 && y < kRows && m.cells[y][x] <= 5) {
        for (int k = 0; k < 8; ++k) m.see(x + kDx[k], y + kDy[k]);
        m.see(x, y);
        for (Maze::Wanderer& w : m.wanderers)
            if (w.x == x && w.y == y && w.dir >= 0 && kDx[w.dir] == -dx && kDy[w.dir] == -dy) w.alarmed = true;
        x += dx;
        y += dy;
    }
}

void Mystery::mazeReveal() {
    // f22_04b0 / f22_02ca / f22_0370: what Edison sees now, and the cells
    // whose visibility changed redrawn (on 1, then copied to 2).
    Maze& m = maze_;
    if (m.level > 5)
        for (auto& row : m.visible) row.fill(false);
    for (int k = 0; k < 8; ++k) m.see(m.x + kDx[k], m.y + kDy[k]);
    mazeLook(-1, 0);
    mazeLook(1, 0);
    mazeLook(0, -1);
    mazeLook(0, 1);
    select(1);
    for (int y = 0; y < kRows; ++y)
        for (int x = 0; x < kCols; ++x) {
            if (m.visible[y][x] == m.shown[y][x]) continue;
            if (!m.visible[y][x])
                fill(x * kCell, y * kCell, kCell, kCell, 0);
            else
                drawOpaque(x * kCell, y * kCell, static_cast<uint16_t>(0x2138 + m.cells[y][x]));
            copyArea(1, 2, x * kCell, y * kCell, kCell, kCell);
        }
    m.shown = m.visible;
}

void Mystery::mazePace() {
    // The original runs its maze loop flat out; the port gives each step
    // of the animations 40 ms.
    ctx_.pump();
    while (ctx_.platform.milliseconds() - maze_.lastStep < 40) ctx_.pump();
    maze_.lastStep = ctx_.platform.milliseconds();
}

void Mystery::mazeWander() {
    // f22_0518: each wanderer one step (a quarter of a cell) further, and
    // at each cell a new direction: a turn to the right (a U-turn when it
    // has seen Edison), then left turns or a random one until it isn't
    // facing a wall.
    Maze& m = maze_;
    select(2);
    auto tile = [&](int x, int y) {
        if (x >= 0 && x < kCols && y >= 0 && y < kRows)
            drawOpaque(x * kCell, y * kCell, static_cast<uint16_t>(0x2138 + m.cells[y][x]));
    };
    auto show = [&](int x, int y) {
        if (m.seen(x, y)) copyArea(2, 1, x * kCell, y * kCell, kCell, kCell);
    };
    for (Maze::Wanderer& w : m.wanderers) {
        if (w.x == 0) continue;
        if (w.dir >= 0) {
            bool left = false;
            if (w.px != 0 && w.px != w.x) {
                tile(w.px, w.py);
                left = true;
            }
            tile(w.x, w.y);
            const int nx = w.x + kDx[w.dir], ny = w.y + kDy[w.dir];
            tile(nx, ny);
            const int sx = w.x * kCell + w.step * kDx[w.dir] * 6;
            const int sy = w.y * kCell + w.step * kDy[w.dir] * 6;
            const int jy = random(3), jx = random(3);
            drawLogo(sx + jx - 1, sy + jy - 1, static_cast<uint16_t>(0x2168 + 2 * w.dir));
            drawLogo(m.spriteX, m.spriteY, m.sprite);
            show(w.x, w.y);
            show(nx, ny);
            if (left) show(w.px, w.py);
            w.px = w.x, w.py = w.y;
            ++w.step;
        }
        if (w.dir >= 0 && w.step != 4) continue;
        bool turn = true;
        int tries = 0;
        const int was = w.dir;
        if (w.dir >= 0) w.x += kDx[w.dir], w.y += kDy[w.dir];
        for (;;) {
            if (turn) {
                w.dir = (w.dir + (w.alarmed ? 2 : 1)) & 3;
                turn = false;
            }
            const int nx = w.x + kDx[w.dir], ny = w.y + kDy[w.dir];
            if (++tries < 10 && (w.dir == was || (nx == m.x && ny == m.y))) continue;
            if (nx >= 0 && nx < kCols && ny >= 0 && ny < kRows && m.cells[ny][nx] < 6) break;
            w.dir = random(4) == 0 ? random(4) : (w.dir + 3) & 3;
        }
        w.step = 0;
        w.alarmed = false;
    }
}

void Mystery::mazeStep(int dx, int dy) {
    // f22_0a62: Edison one cell (in six steps), unless it's a wall; the
    // exit ends the game. A dead end gets a shrug.
    Maze& m = maze_;
    const int tx = m.x + dx, ty = m.y + dy;
    const uint8_t target = m.cells[ty][tx];
    if (target == 11) m.done = true;
    drawLogo(m.exitX, m.exitY, 0x2143);  // f22_0130 (on screen 1)
    if (target >= 6) return;
    m.moved = true;
    const uint16_t base = dx == 0 ? (dy == -1 ? 0x2144 : 0x2156) : (dx == -1 ? 0x214D : 0x215F);
    int px = m.x * kCell, py = m.y * kCell;
    select(2);
    uint16_t frame = base;
    if (dx != 0 || dy != 0) {
        music(0x24);
        for (int i = 0; i < 6; ++i) {
            drawOpaque(m.x * kCell, m.y * kCell, static_cast<uint16_t>(0x2138 + m.cells[m.y][m.x]));
            drawOpaque(tx * kCell, ty * kCell, static_cast<uint16_t>(0x2138 + target));
            m.sprite = frame, m.spriteX = px, m.spriteY = py;
            drawLogo(px, py, frame++);
            copyArea(2, 1, m.x * kCell, m.y * kCell, kCell, kCell);
            copyArea(2, 1, tx * kCell, ty * kCell, kCell, kCell);
            px += 4 * dx;
            py += 4 * dy;
            mazeWander();
            mazePace();
        }
        int k = 0;
        for (; k < 4; ++k) {
            const int nx = tx + kDx[k], ny = ty + kDy[k];
            if (nx == m.x && ny == m.y) continue;
            if (nx >= 0 && nx < kCols && ny >= 0 && ny < kRows && m.cells[ny][nx] < 6) break;
        }
        if (k == 4) {
            select(2);
            drawOpaque(tx * kCell, ty * kCell, static_cast<uint16_t>(0x2138 + target));
            drawLogo(px, py, static_cast<uint16_t>(base + 6));
            copyArea(2, 1, tx * kCell, ty * kCell, kCell, kCell);
        }
    }
    m.x = tx, m.y = ty;
    m.moved = false;
    mazeReveal();
}

void Mystery::bonusMaze(int level) {
    // f22_0ec8: one of three mazes for the level's group (0-2, 3-5, 6-8),
    // with 120, 180 or 300 seconds.
    static const uint16_t kMazes[3][3] = {{0x3F6A, 0x40D6, 0x4242}, {0x43AE, 0x451A, 0x4686}, {0x47F2, 0x495E, 0x4ACA}};
    static const int kSeconds[3] = {120, 180, 300};
    if (level < 0 || level > 8) return;
    Maze& m = maze_;
    m = Maze{};
    const int group = level / 3, r = random(10);
    const uint16_t at = dataWord(kMazes[group][r < 3 ? 0 : r < 6 ? 1 : 2]);
    m.timeLeft = kSeconds[group];

    // f22_0e72 / f22_0db2: the exit (the letter O) and the panels.
    for (int y = 0; y < kRows; ++y)
        for (int x = 0; x < kCols; ++x) {
            const size_t i = static_cast<size_t>(at) + y * kCols + x;
            const uint8_t c = i < data_.size() ? data_[i] : '#';
            m.cells[y][x] = c;
            if (c == 'O') m.exitX = x * kCell, m.exitY = y * kCell;
        }
    bool quit = false;  // [91A2]
    panels_.clear();
    Panels::Panel arrows;  // DS:4B04: left, right, up, down
    arrows.x = 0, arrows.y = 0x177, arrows.w = 0x1CC, arrows.h = 0x18;
    for (int k = 0; k < 4; ++k) arrows.buttons.push_back({0x64 + k * 0x18, 0, 0x18, 0x18});
    arrows.whileHeld = [this](int b) {  // g22_006c
        static const int kMove[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        if (b >= 0) mazeStep(kMove[b][0], kMove[b][1]);
    };
    panels_.add(arrows);
    Panels::Panel door;  // DS:B796: clicking the exit gives up
    door.x = m.exitX, door.y = m.exitY, door.w = kCell, door.h = kCell;
    door.buttons = {{0, 0, kCell, kCell}};
    door.onPress = [&quit](int b) { if (b >= 0) quit = true; };  // f06_0000
    panels_.add(door);
    int points = 0;       // [8ABA]
    bool redraw = true;   // [8AB8]
    ctx_.timer.setPeriodic(kMazeSlot, 1, [&] {  // g22_0100
        if (m.timeLeft > 0) --m.timeLeft;
        redraw = true;
    });
    music(0x2A);

    // The letters become tiles: floor 0-2 (3-5 now and then), walls 6-9,
    // the entrance 10 and the exit 11.
    for (auto& row : m.cells)
        for (uint8_t& c : row) {
            if (c == ' ') c = static_cast<uint8_t>(random(16) == 1 ? random(3) + 3 : random(3));
            else if (c == '#') c = static_cast<uint8_t>(random(4) + 6);
            else if (c == 'I') c = 10;
            else if (c == 'O') c = 11;
        }
    m.level = level;
    m.x = 1, m.y = 12;
    select(1);
    fill(0, 0, Screen::kWidth, Screen::kHeight, 0);
    {
        std::vector<uint8_t> raw;
        if (ctx_.read(0x217, raw))
            for (size_t k = 0; k + 2 < raw.size() && k / 3 < 256; k += 3)
                ctx_.screens[1].palette[k / 3] = Rgb{raw[k + 2], raw[k + 1], raw[k]};
    }
    applyColours(1, true);
    for (auto& row : m.visible) row.fill(level < 3);
    mazeReveal();
    for (int k = 0; k < 4; ++k) drawLogo(0x64 + k * 0x18, 0x177, static_cast<uint16_t>(0x2170 + k));
    const std::string timeLabel = dataString(0x4B1D), scoreLabel = dataString(0x4B29);
    text(0x12C - font_->width(timeLabel), 0x17C, timeLabel, 0xFF);
    text(0x1F4 - font_->width(scoreLabel), 0x17C, scoreLabel, 0xFF);
    drawLogo(m.exitX, m.exitY, 0x2143);
    // f22_0d12: the three wanderers, each on a random floor cell.
    for (Maze::Wanderer& w : m.wanderers) {
        for (;;) {
            const int x = random(kCols), y = random(kRows);
            if (m.cells[y][x] < 6) {
                w = {x, y, 0, 0, -1, false, 0};
                break;
            }
        }
    }
    m.sprite = 0x2144;
    sound(0x4063);

    bool first = true;
    clearInput();
    m.lastStep = ctx_.platform.milliseconds();
    while (!m.done && !quit) {
        m.moved = false;
        if (first) {
            first = false;
            mazeStep(0, -1);
        }
        panels_.poll(ctx_.platform);
        if (redraw) {
            digitalTime(0x12C, 0x17C, 0x30, 0x10, m.timeLeft);
            intBox(0x1F4, 0x17C, 0x30, 0x10, points);
            redraw = false;
        }
        // The arrow keys, held (a typed one counts once).
        bool typed[4] = {};
        for (int key; (key = ctx_.platform.takeKey()) != 0;)
            if (key >= Platform::kLeft && key <= Platform::kDown) typed[key - Platform::kLeft] = true;
        auto held = [&](int k) { return typed[k] || ctx_.platform.keyHeld(Platform::kLeft + k); };
        if (!m.done && held(0)) mazeStep(-1, 0);
        if (!m.done && held(1)) mazeStep(1, 0);
        if (!m.done && held(2)) mazeStep(0, -1);
        if (!m.done && held(3)) mazeStep(0, 1);
        if (!m.moved) mazeStep(0, 0);
        mazeWander();
        mazePace();
    }
    if (m.done) {
        ctx_.timer.setPeriodic(kMazeSlot, 0, nullptr);
        points += 100;
        while (m.timeLeft > 0) {
            --m.timeLeft;
            digitalTime(0x12C, 0x17C, 0x30, 0x10, m.timeLeft);
            points += 2;
            intBox(0x1F4, 0x17C, 0x30, 0x10, points);
            ctx_.pump();
        }
        score_ += points;
        waitCountdown(0xF);
    }
    ctx_.timer.setPeriodic(kMazeSlot, 0, nullptr);
    panels_.clear();
    music(0);
}

}  // namespace edison
