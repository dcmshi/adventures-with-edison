// WMAIN.EXE segment 19: the laboratory, room 501 (f38_06dd, f19_0a59):
// Edison walks in, asks the player's name, and the Character Enhancer.

#include "science/science.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace edison {

namespace {

// DS:1C5A: Edison's walk, {picture, x, y, w, h, delay}; drawn at
// (x, y + E6), each shown delay / 2 countdown ticks.
struct WalkFrame {
    uint16_t picture;
    int x, y, w, h, delay;
};
constexpr WalkFrame kWalk[19] = {
    {0x5001, 0, 68, 14, 28, 4},    {0x5000, 0, 50, 19, 28, 12},  {0x5001, 0, 68, 14, 28, 2},
    {0x5002, 0, 20, 24, 62, 2},    {0x5003, 50, 0, 38, 71, 2},   {0x5004, 100, 20, 44, 62, 2},
    {0x5005, 130, 70, 39, 37, 2},  {0x5006, 130, 37, 28, 54, 2}, {0x5007, 130, 68, 35, 38, 2},
    {0x5008, 130, 54, 35, 45, 8},  {0x5009, 130, 78, 35, 33, 2}, {0x500A, 130, 38, 38, 55, 2},
    {0x500B, 130, 54, 38, 45, 12}, {0x500C, 130, 70, 35, 37, 2}, {0x500D, 130, 60, 36, 42, 12},
    {0x5009, 130, 78, 35, 33, 2},  {0x500E, 130, 56, 39, 44, 32}, {0x500F, 130, 70, 35, 37, 2},
    {0x5010, 130, 56, 44, 44, 2},
};

// seg95:0000: the Character Enhancer's buttons, relative to its panel
// (seg95:0032: 300, 134, 228 x 220): the four parts, then DONE.
struct Button {
    int x, y, w, h;
};
constexpr int kPanelX = 300, kPanelY = 134;
constexpr Button kButtons[5] = {{48, 36, 108, 30}, {48, 66, 108, 30}, {48, 96, 108, 30}, {48, 126, 108, 30}, {57, 176, 110, 42}};

// The parts' colour tables (seg95, 6-bit R, G, B, 8 choices) and where
// they go: hair 3 at E1, face 3 at E4, shirt 4 at E7, trousers 3 at EB.
struct Part {
    uint16_t table;
    int count, first;
};
constexpr Part kParts[4] = {{0x004F, 3, 0xE1}, {0x0097, 3, 0xE4}, {0x00DF, 4, 0xE7}, {0x013F, 3, 0xEB}};

}  // namespace

// --- the lab's helpers ------------------------------------------------------------

void Science::waitCountdown(int ticks) {
    // A busy wait on the countdown [95F2] (10 a second).
    ctx_.countdown[0] = ticks;
    while (ctx_.countdown[0] > 0) ctx_.pump();
}

void Science::textAt(int x, int y, const std::string& s, int colour) {
    // f76_0021: on the current screen, as it is (no copy through 3).
    font_->draw(ctx_.screens[current()], x, y, s, static_cast<uint8_t>(colour));
}

void Science::sound(uint16_t id) {
    // f32_13f2 / f36_007e: a WAV from GRAFX.DAT by id (sounds on).
    std::vector<uint8_t> wav;
    if (!ctx_.read(id, wav) || wav.empty() || wav.size() > 120000) return;
    ctx_.platform.playWav(wav);
}

Rgb Science::lookColour(int part, int choice, int k) const {
    // After f19_0614 (once, the first time f19_06bc runs): 8-bit colours.
    const size_t at = kParts[part].table + 3u * (static_cast<size_t>(choice) * kParts[part].count + k);
    if (at + 2 >= looks_.size()) return Rgb{0, 0, 0};
    return Rgb{static_cast<uint8_t>(looks_[at] << 2), static_cast<uint8_t>(looks_[at + 1] << 2),
               static_cast<uint8_t>(looks_[at + 2] << 2)};
}

void Science::applyLook() {
    // f19_06bc: the look into screen 1's palette (E1-ED), and that to the
    // display.
    Palette& pal = ctx_.screens[1].palette;
    for (int t = 0; t < 4; ++t)
        for (int k = 0; k < kParts[t].count; ++k) pal[kParts[t].first + k] = lookColour(t, look_[t], k);
    ctx_.setDisplayPalette(1);
}

void Science::mouth(int talks) {
    // f19_0541: Edison's mouth (5010 and 5011-5013 at (82, 11E) and
    // (8E, 11E)) on screen 2, copied to the display; 2 ticks a step.
    select(2);
    copyArea(2, 3, 0, 0, Screen::kWidth, Screen::kHeight);
    for (int i = 0; i <= talks; ++i) {
        drawLogo(0x82, 0x11E, 0x5010);
        drawLogo(0x8E, 0x11E, static_cast<uint16_t>(i < talks ? 0x5012 + (i & 1) : 0x5011));
        copyArea(2, 1, 0x8E, 0x11E, 0x34, 0x26);
        copyArea(3, 2, 0x8E, 0x11E, 0x34, 0x26);
        waitCountdown(2);
    }
    select(1);
}

void Science::labBackground() {
    // f19_0f6f: the burner (13A0 / 13A1 at (E0, 95)) and the bubbles
    // (13A2-13A4 at (1EC, 7C)), every 7/50 s.
    const int previous = current();
    select(1);
    waitTicks(7, false);  // f32_07aa(7)
    const unsigned n = labFrame_++;
    drawLogo(0xE0, 0x95, static_cast<uint16_t>((n & 1) == 0 ? 0x13A1 : 0x13A0));
    unsigned k = labFrame_ % 20;
    if (k < 18) k = std::min(k, 2u);
    else k = 19 - k;
    drawLogo(0x1EC, 0x7C, static_cast<uint16_t>(0x13A2 + k));
    select(2);
    drawLogo(0x1EC, 0x7C, static_cast<uint16_t>(0x13A2 + k));
    select(previous);
}

void Science::walk(int mode) {
    // f19_0335: 0 walks in (frames 0-9), 2 a turn (17-18), 3 to the
    // machine (10-12, the machine drawn), 1 away with "Cool!" (13-16).
    static const int kFrom[4] = {0, 13, 17, 10}, kTo[4] = {10, 17, 19, 13};
    select(2);
    showScreen(0x2001, 2);
    applyLook();
    copyArea(2, 3, 0, 0, Screen::kWidth, Screen::kHeight);
    for (int i = kFrom[mode]; i < kTo[mode]; ++i) {
        if (mode == 3 && i > 9) {
            copyArea(2, 1, 0x12, 0x80, 300, 0x8E);
            drawLogo(300, 0x86, 0x1428);
            drawLogo(0x146, 0x70, 0x1429);
            copyArea(2, 1, 300, 0x70, 0xE4, 0x120);
        }
        const WalkFrame& f = kWalk[i];
        drawLogo(f.x, f.y + 0xE6, f.picture);
        copyArea(2, 1, 0, 0xE6, 0xDA, 0x8E);
        copyArea(3, 2, 0, 0xE6, 0xDA, 0x8E);
        if (i == 16) {
            select(1);
            drawLogo(0x24, 0xC0, 0x142C);
            textAt(0x54, 0xD4, dataString(0x1D76), 0x0E);  // "Cool!"
            sound(0x600C);
            select(2);
        }
        ctx_.countdown[0] = f.delay / 2;
        while (ctx_.countdown[0] > 0) {
            if (i != 16) labBackground();
            else ctx_.pump();
        }
    }
    select(1);
    if (mode == 1) copyArea(2, 1, 0x24, 0xC0, 0x96, 0x47);
}

std::string Science::enterName(int x, int y, int maxLength, int width, int colour) {
    // f19_0003: typed letters with a caret ("|"), Backspace, Enter; the
    // field (x, y, width x 20) is saved and restored around each key.
    std::string name;
    for (;;) {
        const int area = saveArea(x, y, width + 0x14, 0x14);
        textAt(x, y, name + "|", colour);
        int key = 0;
        while ((key = ctx_.platform.takeKey()) == 0) ctx_.pump();
        restoreArea(area);
        if (key == Platform::kEnter) break;
        if (key == Platform::kBackspace) {
            if (!name.empty()) name.pop_back();
            continue;
        }
        if (key < 0x20 || key > 0xFF) continue;
        if (static_cast<int>(name.size()) < maxLength - 1 && font_->width(name) < width) name += static_cast<char>(key);
    }
    return name;
}

void Science::askName() {
    // f19_0249: the bubble (142B at (12, 80)), the question, the name
    // (8 letters; "Player" if none), then shown as "Name".
    drawLogo(0x12, 0x80, 0x142B);
    ctx_.platform.stopWav();
    textAt(0x16, 0xA0, dataString(0x1D4C), 0x0E);  // "Hi! I'm Edison.  What's your name?"
    sound(0x600E);
    mouth(8);
    std::string name = enterName(0x16, 0xB4, 9, 0x60, 0x0E);
    if (name.empty()) name = dataString(0x1D6F);  // "Player"
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    playerName_ = name;
    textAt(0x20, 0xBC, name, 0x0E);
    waitCountdown(10);
}

void Science::characterEnhancer() {
    // f19_0976 (the buttons, segment 18): a click on a part takes its next
    // choice (f19_07fa, the colours at once: f20_0126); DONE (its picture
    // 142A pressed) or Enter ends it.
    for (;;) {
        ctx_.pump();
        if (ctx_.platform.takeKey() == Platform::kEnter) break;
        int mx, my;
        if (!ctx_.platform.takeClick(&mx, &my)) continue;
        const int px = mx - kPanelX, py = my - kPanelY;
        int hit = -1;
        for (int b = 0; b < 5; ++b)
            if (px >= kButtons[b].x && py >= kButtons[b].y && px < kButtons[b].x + kButtons[b].w &&
                py < kButtons[b].y + kButtons[b].h)
                hit = b;
        if (hit == 4) {
            select(1);
            drawLogo(kButtons[4].x + kPanelX, kButtons[4].y + kPanelY, 0x142A);
            waitCountdown(4);
            break;
        }
        if (hit >= 0) {
            look_[hit] = static_cast<uint8_t>(look_[hit] + 1 > 7 ? 0 : look_[hit] + 1);
            Palette& pal = ctx_.screens[1].palette;
            for (int k = 0; k < kParts[hit].count; ++k) pal[kParts[hit].first + k] = lookColour(hit, look_[hit], k);
            ctx_.setDisplayPalette(1);
        }
    }
    copyArea(2, 1, 300, 0x70, 0xE4, 0x120);
}

// --- the players' files ---------------------------------------------------------

void Science::loadPlayers() {
    // f19_115a ("HSFILE"): up to 50 lines of name, score, two numbers and
    // the look; from the game's folder, else the CD's first table.
    players_.clear();
    std::ifstream in(options_.saveDir + "/wscience.hs");
    if (!in) in.open(options_.cdDir + "/WSCIENCE.HS");
    std::string tag;
    if (!(in >> tag) || tag != "HSFILE") return;
    PlayerEntry p;
    while (players_.size() < 50 && in >> p.name >> p.score >> p.a >> p.b >> p.look[0] >> p.look[1] >> p.look[2] >> p.look[3])
        players_.push_back(p);
}

void Science::savePlayers() const {
    // f21_0447: "HSFILE " and " name score a b l0 l1 l2 l3 " lines.
    std::ofstream out(options_.saveDir + "/wscience.hs", std::ios::binary);
    if (!out) return;
    out << "HSFILE ";
    for (const PlayerEntry& p : players_)
        out << ' ' << p.name << ' ' << p.score << ' ' << p.a << ' ' << p.b << ' ' << p.look[0] << ' ' << p.look[1] << ' '
            << p.look[2] << ' ' << p.look[3] << " \r\n";
}

void Science::loadLook() {
    // wscience.edi: the last look, four numbers (each kept to 0-7).
    std::ifstream in(options_.saveDir + "/wscience.edi");
    int v[4] = {};
    if (in >> v[0] >> v[1] >> v[2] >> v[3])
        for (int t = 0; t < 4; ++t) look_[t] = static_cast<uint8_t>(v[t] < 0 || v[t] > 7 ? 0 : v[t]);
}

void Science::saveLook() const {
    // Each number followed by " \n" (DS:1DB9).
    std::ofstream out(options_.saveDir + "/wscience.edi", std::ios::binary);
    if (out) out << int{look_[0]} << " \n" << int{look_[1]} << " \n" << int{look_[2]} << " \n" << int{look_[3]} << " \n";
}

// --- room 501 -------------------------------------------------------------------

void Science::lab() {
    // f19_0a59.
    loadLook();
    loadPlayers();
    clearDisplay();
    showScreen(0x2001, 2);
    playerName_.clear();
    toDisplay(2);
    applyLook();
    int found = -1;
    if (!escapePressed()) {
        showScreen(0x2001, 2);
        applyLook();
        walk(0);
        walk(2);
        askName();
        copyArea(2, 1, 0x12, 0x80, 300, 0x8E);
        // A returning player (the first 8 letters, any case) gets their look.
        auto lower8 = [](const std::string& s) {
            std::string r = s.substr(0, 8);
            for (char& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return r;
        };
        for (size_t i = 0; i < players_.size(); ++i)
            if (lower8(players_[i].name) == lower8(playerName_)) {
                for (int t = 0; t < 4; ++t) look_[t] = static_cast<uint8_t>(players_[i].look[t] & 7);
                applyLook();
                found = static_cast<int>(i);
                break;
            }
        waitCountdown(8);
        drawLogo(0x12, 0x90, 0x142D);
        textAt(0x2C, 0x9A, dataString(0x1D95), 0x0E);  // "Do you wanna change the way"
        textAt(0x2C, 0xAE, dataString(0x1DB1), 0x0E);  // "I look?"
        sound(0x6014);
        mouth(6);
        waitCountdown(4);
        walk(3);
        if (!escapePressed()) {
            characterEnhancer();
            applyLook();
            if (found >= 0)  // f21_0000
                for (int t = 0; t < 4; ++t) players_[static_cast<size_t>(found)].look[t] = look_[t];
            if (!escapePressed()) walk(1);
        }
    }
    saveLook();
    waitCountdown(10);
    if (playerName_.empty()) playerName_ = dataString(0x1D6F);
    clearDisplay();
    fill(0, 0, Screen::kWidth, Screen::kHeight, 2);
    savePlayers();
}

}  // namespace edison
