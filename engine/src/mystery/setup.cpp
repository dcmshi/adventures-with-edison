// MALL.EXE segment 8: setting up a game (player, Edison's looks, level).

#include <algorithm>
#include "formats/paths.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "mystery/mystery.h"

namespace edison {
namespace {

// The courtyard scene's frames (the table built at the start of f08_06f8):
// sprite, x, y (from the top of the scene area at y = 230), half width and
// half height (unused here), and how long it shows in tenths of a second.
struct SceneFrame {
    uint16_t sprite;
    int16_t x, y, halfW, halfH, tenths;
};
constexpr SceneFrame kScene[] = {
    {0x2306, 0, 0x44, 0x0E, 0x1C, 4},     {0x2305, 0, 0x32, 0x13, 0x1C, 12},
    {0x2306, 0, 0x44, 0x0E, 0x1C, 2},     {0x2307, 0, 0x14, 0x18, 0x3E, 2},
    {0x2308, 0x32, 0, 0x26, 0x47, 2},     {0x2309, 100, 0x14, 0x2C, 0x3E, 2},
    {0x230A, 0x82, 0x46, 0x27, 0x25, 2},  {0x230B, 0x82, 0x25, 0x1C, 0x36, 2},
    {0x230C, 0x82, 0x44, 0x23, 0x26, 2},  {0x230D, 0x82, 0x36, 0x23, 0x2D, 2},
    {0x2315, 0x82, 0x38, 0x2C, 0x2C, 2},  {0x230E, 0x82, 0x4E, 0x23, 0x21, 2},
    {0x230F, 0x82, 0x26, 0x26, 0x37, 2},  {0x2310, 0x82, 0x36, 0x26, 0x2D, 8},
    {0x2311, 0x82, 0x46, 0x23, 0x25, 2},  {0x2312, 0x82, 0x3C, 0x25, 0x2A, 12},
    {0x230E, 0x82, 0x4E, 0x23, 0x21, 2},  {0x2313, 0x82, 0x38, 0x27, 0x2C, 12},
    {0x2314, 0x82, 0x46, 0x23, 0x25, 2},  {0x2315, 0x82, 0x38, 0x2C, 0x2C, 2},
};
constexpr int kSceneTop = 0xE6;
constexpr int kSceneW = 0xDA, kSceneH = 0x96;

// Edison's mouth while he speaks sound 403E (added to sprite 2353).
constexpr int kLipSync[] = {0, 1, 2, 0, 2, 0, 3, 2, 1, 3};

}  // namespace

void Mystery::title() {
    // f08_2284 (first time only): the title screen for 10 s or until a
    // click or key, over music 29, then sound 4064.
    const int previous = current();
    select(2);
    ctx_.screens[2].clear();  // f04_0000: screen 2 cleared and shown
    ctx_.screens.copyAll(2, 1);
    ctx_.showFullScreen(0x100E, 2);
    ctx_.setDisplayPalette(2);  // f04_005c(2)
    ctx_.screens.copyAll(2, 1);
    stirRandom();
    music(0x29);
    ctx_.countdown[0] = 100;
    clearInput();
    while (ctx_.countdown[0] != 0 && !anyInput()) ctx_.pump();
    clearInput();
    select(previous);
    sound(0x4064);
}

void Mystery::setupScreen() {
    // The start of f08_232c: the courtyard backdrop on screen 2 with
    // Edison's colours, shown.
    select(1);
    ctx_.screens[2].clear();  // f04_0000
    ctx_.screens.copyAll(2, 1);
    ctx_.showFullScreen(0x1006, 2);
    applyColours(2, false);     // f09_0b88(1, 2, 0)
    show(2);
    computeUiColours();
    // Keep a clean copy of the scene area at the top left of screen 2.
    duplicateArea(2, 2, 0, kSceneTop, kSceneW, kSceneH, 0, 0);
}

void Mystery::scene(int part) {
    // Which frames each part plays (f08_06f8's switch).
    static const int kRange[5][2] = {{0, 11}, {11, 14}, {14, 18}, {18, 20}, {19, 20}};
    int i = kRange[part][0];
    const int end = kRange[part][1];
    if (part == 0) music(0x1B);
    select(2);
    int savedBackground = 0;
    if (part == 4) savedBackground = saveArea(0, kSceneTop, kSceneW, kSceneH);
    (void)savedBackground;  // (kept by the original for the high-score screen)

    for (; i < end; ++i) {
        const SceneFrame& f = kScene[i];
        if (part == 1 && i == 11) {
            // The character changer machine appears.
            select(1);
            drawLogo(300, 0x86, 0x2087);
            drawLogo(0x146, 0x70, 0x2088);
            select(2);
        }
        drawLogo(f.x, f.y + kSceneTop, f.sprite);
        copyArea(2, 1, 0, kSceneTop, kSceneW, 0x8E);

        if (part == 1 && i == 13) {
            // Edison talks: sound 403E with his mouth moving.
            sound(0x403E);
            select(1);
            for (int mouth : kLipSync) {
                drawLogo(f.x - 2, f.y + kSceneTop - 2, static_cast<uint16_t>(0x2353 + mouth));
                waitCountdown(2);
            }
            select(2);
        }
        if (part != 4) duplicateArea(2, 2, 0, 0, kSceneW, kSceneH, 0, kSceneTop);

        int bubble = 0;
        if (i == 0x11) {
            select(1);
            const std::vector<std::string> lines = {dataString(0x7C7), dataString(0x7D3)};
            bubble = speechBox(0x3C, Screen::kHeight / 2, lines, 2, true);
            sound(0x4036);
            select(2);
        }
        waitCountdown(f.tenths);
        if (bubble) {
            select(1);
            restoreArea(bubble);
            select(2);
        }
    }
    select(1);
}

bool Mystery::talk(int frames) {
    // g08_0cf8: Edison (sprite 2315) with his mouth moving for `frames`
    // steps of 0.2 s (2317/2318), then closed (2316). A click stops it.
    select(2);
    for (int i = 0; i <= frames; ++i) {
        drawLogo(0x82, 0x11E, 0x2315);
        drawLogo(0x8E, 0x11E, static_cast<uint16_t>(i < frames ? 0x2317 + (i & 1) : 0x2316));
        copyArea(2, 1, 0x8E, 0x11E, 0x34, 0x26);
        duplicateArea(2, 2, 0, 0, kSceneW, kSceneH, 0, kSceneTop);
        ctx_.countdown[0] = 2;
        bool clicked = false;
        while (ctx_.countdown[0] != 0) {
            ctx_.pump();
            int x, y;
            if (ctx_.platform.takeClick(&x, &y)) {
                clicked = true;
                drawLogo(0x82, 0x11E, 0x2315);
                drawLogo(0x8E, 0x11E, 0x2316);
                copyArea(2, 1, 0x8E, 0x11E, 0x34, 0x26);
            }
        }
        if (clicked) {
            select(1);
            return true;
        }
    }
    select(1);
    return false;
}

void Mystery::nameEntry() {
    // g08_0380: "Hi! I'm Edison. What's your name?" in a speech bubble, with
    // a typing field under it.
    const int fh = font_->height();
    std::vector<std::string> lines = {dataString(0x759), dataString(0x77C)};
    for (uint16_t blank : {0x789, 0x78D, 0x791, 0x795}) lines.push_back(dataString(blank));
    const int boxY = Screen::kHeight / 3;
    const int bubble = speechBox(4, boxY, lines, 0, true);
    sound(0x403A);
    talk(5);

    const std::string prompt = dataString(0x7AA);  // ">"
    const std::string cursor = dataString(0x7AE);  // "_"
    const int x0 = 4 + fh, y0 = fh * 3 + boxY;
    const int fieldW = font_->width(dataString(0x799));  // as wide as "Is this right?  "
    fill(x0, y0, fieldW, fh * 2, 0xFF);
    text(x0, y0, prompt, 0);

    std::string name;
    bool cursorOn = true;
    clearInput();
    for (;;) {
        // Blink the cursor until a key comes.
        int key = 0;
        int cx = 0;
        while (key == 0) {
            cx = x0 + font_->width(prompt) + font_->width(name);
            text(cx, y0, cursor, cursorOn ? 0x32 : 0xFF);
            cursorOn = !cursorOn;
            ctx_.countdown[0] = 2;
            while (ctx_.countdown[0] != 0 && key == 0) {
                ctx_.pump();
                key = ctx_.platform.takeKey();
            }
        }
        text(cx, y0, cursor, 0xFF);
        if (key == Platform::kEnter || key == Platform::kEscape) break;
        bool erase = false;
        if (key == Platform::kBackspace && !name.empty()) {
            name.pop_back();
            erase = true;
        }
        const bool alnum = (key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z') || (key >= 'a' && key <= 'z');
        if (name.size() < 8 && alnum)
            name += static_cast<char>(name.empty() && key >= 'a' && key <= 'z' ? key - 32 : key);
        if (erase) fill(x0, y0, fieldW, fh * 2, 0xFF);
        text(x0, y0, dataString(0x7B2), 0);
        text(x0 + font_->width(dataString(0x7B4)), y0, name, 0);
    }
    player_.name = name.empty() ? dataString(0x7B6) : name;  // "Player"
    select(1);
    restoreArea(bubble);
    clearInput();
}

// --- Edison's colours -------------------------------------------------

namespace {
// Colour groups: hair, shirt, trousers, shoes. Palette entries and table.
constexpr int kGroupFirst[4] = {1, 4, 7, 11};
constexpr int kGroupCount[4] = {3, 3, 4, 3};
constexpr uint16_t kGroupTable[4] = {0x824, 0x848, 0x86C, 0x89C};
}  // namespace

void Mystery::loadColourTables() {
    // Four choices per group, stored as 6-bit RGB; f06_1ce0 converts them
    // to 8-bit once, on the first setup.
    for (int g = 0; g < 4; ++g) {
        edisonColours_[g].clear();
        for (int i = 0; i < 4 * kGroupCount[g]; ++i) {
            const uint8_t* c = &data_[kGroupTable[g] + 3 * i];
            edisonColours_[g].push_back(Rgb{static_cast<uint8_t>(c[0] << 2), static_cast<uint8_t>(c[1] << 2),
                                            static_cast<uint8_t>(c[2] << 2)});
        }
    }
}

void Mystery::applyColours(int screen, bool toDisplay) {
    // f09_0b88: Edison's colours into a screen's palette, entries 1-15.
    Palette& pal = ctx_.screens[screen].palette;
    for (int g = 0; g < 4; ++g)
        for (int i = 0; i < kGroupCount[g]; ++i)
            pal[kGroupFirst[g] + i] = edisonColours_[g][player_.colours[g] * kGroupCount[g] + i];
    pal[14] = Rgb{0, 0, 0};
    pal[15] = Rgb{0xFC, 0xFC, 0xFC};
    if (!toDisplay) {
        for (int i = 0; i < 3; ++i) pal[17 + i] = edisonColours_[1][player_.colours[1] * 3 + i];
    } else {
        ctx_.setDisplayPalette(screen);
    }
}

void Mystery::setColourGroup(int group) {
    // f06_1c42: one group's colours straight into the display palette and
    // screen 2's.
    for (int i = 0; i < kGroupCount[group]; ++i) {
        const Rgb c = edisonColours_[group][player_.colours[group] * kGroupCount[group] + i];
        ctx_.displayPalette[kGroupFirst[group] + i] = c;
        ctx_.screens[1].palette[kGroupFirst[group] + i] = c;
        ctx_.screens[2].palette[kGroupFirst[group] + i] = c;
    }
}

// --- dialogs ------------------------------------------------------------

std::vector<std::string> Mystery::dataLines(uint16_t table) const {
    // A list of far pointers to strings, ending with an empty string.
    std::vector<std::string> lines;
    for (uint16_t at = table;; at = static_cast<uint16_t>(at + 4)) {
        std::string line = dataString(dataWord(at));
        if (line.empty() || lines.size() >= 30) break;
        lines.push_back(line);
    }
    return lines;
}

int Mystery::yesNo(int x, int y, int w, int h) {
    // f06_2976: two animated buttons, Yes (208A-208C) on the left and
    // No (208D-208F) on the right. Y or Enter answer yes, N no.
    static const int kCycle[6] = {0, 1, 2, 2, 1, 0};
    const int previous = current();
    select(1);
    const int saved = saveArea(x, y, w, h);
    const Bitmap& yes = ctx_.bitmap(0x208A);
    const int leftX = x + 5, rightX = x + w - yes.width - 5;
    const int by = (h - yes.height) / 2 + y;
    const int hitW = yes.width + 6;
    int answer = -1, frame = 0;
    clearInput();
    while (answer < 0) {
        int key;
        while ((key = ctx_.platform.takeKey()) != 0) {
            if (key == 'y' || key == 'Y' || key == Platform::kEnter) answer = 0;
            if (key == 'n' || key == 'N') answer = 1;
        }
        ctx_.pump();
        int mx, my;
        if (ctx_.platform.takeClick(&mx, &my)) {
            const int bx[2] = {x, rightX};
            for (int b = 0; b < 2; ++b)
                if (bx[b] < mx && y < my && mx <= bx[b] + hitW && my <= y + h) answer = b;
        }
        if (answer >= 0) break;
        drawLogo(leftX, by, static_cast<uint16_t>(0x208A + kCycle[frame]));
        drawLogo(rightX, by, static_cast<uint16_t>(0x208D + kCycle[frame]));
        waitCountdown(1);
        frame = (frame + 1) % 6;
    }
    restoreArea(saved);
    select(previous);
    clearInput();
    return answer;
}

bool Mystery::askChangeLooks() {
    // f08_14d2: "Do you want to change my looks?"
    const int fh = font_->height();
    const int y = Screen::kHeight / 2;
    const int bubble = speechBox(4, y, dataLines(0x958), 3, true);
    sound(0x4047);
    talk(5);
    const int answer = yesNo(4 + fh, y + fh + fh / 2, 0xA0, 0x1E);
    restoreArea(bubble);
    return answer == 0;
}

void Mystery::letsDoIt() {
    // f08_157e: "Let's do it."
    const int bubble = speechBox(4, Screen::kHeight / 2, dataLines(0x978), 3, true);
    sound(0x4046);
    talk(4);
    ctx_.countdown[0] = 4;
    bool down = false;
    while (ctx_.countdown[0] != 0 && !down) {
        ctx_.pump();
        int mx, my;
        ctx_.platform.mouse(&mx, &my, &down);
    }
    restoreArea(bubble);
}

void Mystery::customizer() {
    // f08_0f68: the character changer. Buttons 0-3 cycle a colour group,
    // button 4 (or Enter) is done. After 20 s idle Edison nudges the player.
    bool done = false;
    panels_.clear();
    Panels::Panel panel;
    panel.x = static_cast<int16_t>(dataWord(0x80A));
    panel.y = static_cast<int16_t>(dataWord(0x80C));
    panel.w = static_cast<int16_t>(dataWord(0x80E));
    panel.h = static_cast<int16_t>(dataWord(0x810));
    for (int b = 0; b < 5; ++b) {
        const uint16_t at = static_cast<uint16_t>(0x7D8 + 10 * b);
        panel.buttons.push_back({static_cast<int16_t>(dataWord(at)), static_cast<int16_t>(dataWord(at + 2)),
                                 static_cast<int16_t>(dataWord(at + 4)), static_cast<int16_t>(dataWord(at + 6))});
    }
    const Panels::Button okButton = panel.buttons[4];
    panel.onPress = [&](int b) {  // f08_0e24
        if (b < 0) return;
        if (b == 4) {
            drawLogo(okButton.x + 300, okButton.y + 0x86, 0x2089);
            done = true;
            return;
        }
        player_.colours[b] = static_cast<uint8_t>((player_.colours[b] + 1) & 3);
        setColourGroup(b);
    };
    panels_.add(panel);
    ctx_.countdown[3] = 200;
    clearInput();
    for (;;) {
        int mx, my;
        bool down;
        ctx_.platform.mouse(&mx, &my, &down);
        if (down) ctx_.countdown[3] = 200;
        panels_.poll(ctx_.platform);
        ctx_.pump();
        if (ctx_.countdown[3] == 0) {
            static const int kNudge[] = {0, 1, 2, 1, 2, 0, 2, 0, 3, 2, 1, 3};
            sound(0x404B);
            const int previous = current();
            select(1);
            for (int mouth : kNudge) {
                drawLogo(0x80, 0x11A, static_cast<uint16_t>(0x2353 + mouth));
                waitCountdown(2);
            }
            select(previous);
            ctx_.countdown[3] = 200;
        }
        int key = 0;
        while (int k = ctx_.platform.takeKey()) key = k;
        if (done || key == Platform::kEnter) break;
    }
    copyArea(2, 1, 300, 0x70, 0xE4, 0x120);
    panels_.clear();
    clearInput();
}

void Mystery::levelPanel(int mode, int x, int y) {
    // g08_16c2 lays out panel DS:0A24 for the mode; g08_18be answers a
    // press, lighting the button up (colour C0 to FF) for 0.2 s.
    choice_ = 0;
    panels_.clear();
    Panels::Panel panel;
    panel.x = x, panel.y = y, panel.w = 0xB0;
    if (mode == 2) {
        panel.h = 0x44;
        for (int i = 0; i < 8; ++i) panel.buttons.push_back({(i % 4) * 0x2C + 2, i < 4 ? 2 : 0x12, 0x28, 0xE});
        panel.buttons.push_back({2, 0x34, 0xAC, 0xC});
    } else {
        panel.h = mode == 0 ? 0x2C : 0x1E;
        for (int i = 0; i < (mode == 0 ? 3 : 2); ++i) panel.buttons.push_back({2, i * 0xE + 2, 0xAC, 0xC});
    }
    panel.onPress = [this, mode, x, y](int b) {
        if (b < 0) return;
        if (mode != 2) {
            recolour(x + 2, y + b * 0xE + 2, 0xAC, 0xC, 0xC0, 0xFF);
            choice_ = b + 1;
        } else if (b == 8) {
            choice_ = 2;
            recolour(x + 2, y + 0x34, 0xAC, 0xE, 0xC0, 0xFF);
        } else {
            choice_ = 1;
            player_.level = static_cast<uint8_t>(b);
            customLevel_ = false;
            recolour((b % 4) * 0x2C + 0x16, b < 4 ? 0xCA : 0xDA, 0x28, 0xE, 0xC0, 0xFF);
        }
        waitCountdown(2);
    };
    panels_.add(panel);
}

int Mystery::waitChoice() {
    while (choice_ == 0) {
        panels_.poll(ctx_.platform);
        ctx_.pump();
    }
    panels_.clear();
    return choice_;
}

void Mystery::waitSpeech() {
    // [73B6]: until the WAV has played.
    while (ctx_.platform.wavPlaying()) ctx_.pump();
}

void Mystery::pickLevel() {
    // f08_1d2a: "Please pick a level", eight level buttons (a 4 x 2 grid
    // drawn by sprite 2083) and "make a custom level" (2085).
    const int bubble = speechBox(2, 0xAC, dataLines(0xB22), 3, true);
    levelPanel(2, 0x14, 200);
    drawOpaque(0x14, 200, 0x2083);
    drawOpaque(0x14, 0xFC, 0x2085);
    for (uint16_t line : {0x404A, 0x4037}) {
        sound(line);
        if (talk(6)) break;
        waitCountdown(5);
    }
    const int choice = waitChoice();
    restoreArea(bubble);
    if (choice == 2) menuEvent_ = 1;
}

bool Mystery::askCustomLevel() {
    // g08_1a4e: "You have a custom level." Play it, change it, or not.
    const int bubble = speechBox(2, 0xAC, dataLines(0xA60), 3, true);
    levelPanel(0, 0x14, 0xDC);
    drawOpaque(0x14, 0xDC, 0x2086);
    for (uint16_t line : {0x4038, 0x404D, 0x403B, 0x4039}) {
        sound(line);
        if (talk(6)) break;
        waitCountdown(4);
    }
    const int choice = waitChoice();
    restoreArea(bubble);
    if (choice == 1) menuEvent_ = 2;
    if (choice == 2) menuEvent_ = 4;
    return choice == 1 || choice == 2;
}

bool Mystery::askLastLevel() {
    // g08_1b6e: "Your last game was at level N. Would you like to" play
    // it again or pick another? (Levels show as 6-13.)
    const int bubble = speechBox(2, 0xAC, dataLines(0xAAE), 3, true);
    text(font_->width(dataString(0xAC7)) + 2, font_->height() / 2 + 0xAC, std::to_string(player_.level + 6), 0);
    levelPanel(1, 0x14, 0xDC);
    drawOpaque(0x14, 0xDC, 0x2084);
    sound(0x4044);
    talk(6);
    sound(static_cast<uint16_t>(0x4058 + player_.level));
    talk(3);
    waitSpeech();
    sound(0x4055);
    talk(3);
    waitSpeech();
    sound(0x404E);
    talk(3);
    waitSpeech();
    sound(0x4039);
    talk(3);
    const int choice = waitChoice();
    restoreArea(bubble);
    return choice == 1;
}

bool Mystery::askSavedGame() {
    // g08_1600: "You have a saved game. You want to play that game?"
    const int fh = font_->height();
    const int y = Screen::kHeight / 2;
    const int bubble = speechBox(4, y, dataLines(0x9BA), 0, true);
    panels_.clear();
    sound(0x4054);
    talk(10);
    const bool yes = yesNo(4 + fh, y + fh * 2, 0xA0, 0x1E) == 0;
    if (yes) menuEvent_ = 3;
    restoreArea(bubble);
    return yes;
}

bool Mystery::playAgain() {
    // g08_21b6: after a game, "Do you want to play again?"; no says bye.
    const int fh = font_->height();
    const int y = Screen::kHeight / 2;
    const int bubble = speechBox(4, y, dataLines(0xB5A), 3, true);
    sound(0x404C);
    talk(5);
    panels_.clear();
    const bool yes = yesNo(4 + fh, y + fh + fh / 2, 0xA0, 0x1E) == 0;
    restoreArea(bubble);
    if (!yes) {
        sound(0x4033);
        talk(3);
    }
    return yes;
}

// --- the player's files ---------------------------------------------------

std::string Mystery::playerPath() const {
    // g08_0154: the first 8 letters of the name, then .INF.
    return options_.saveDir + "/" + player_.name.substr(0, 8) + dataString(0x70E);
}

bool Mystery::loadPlayer() {
    // g08_01ee / g08_00d8: the record DS:B465-B595 (0x131 bytes). A new
    // player gets no custom level and no saved game.
    std::ifstream in(findPath(playerPath()), std::ios::binary);
    uint8_t r[0x131];
    if (!in || !in.read(reinterpret_cast<char*>(r), sizeof r)) {
        for (int s = 0; s < 29; ++s) {
            player_.customSquares[s] = Square{0xFF, 0xFF, 0xFF, 0};
            player_.savedSquares[s] = Square{0xFF, 0xFF, 0xFF, 0};
        }
        for (Object& o : player_.savedObjects) o = Object{0xFF, 0xFF, 0xFF};
        player_.savedLevel = player_.customLevel = 0xFF;
        player_.savedTimeLeft = 0;
        player_.savedScore = 0;
        return false;
    }
    auto word = [&](int at) { return static_cast<uint16_t>(r[at] | r[at + 1] << 8); };
    player_.name.assign(reinterpret_cast<const char*>(r), strnlen(reinterpret_cast<const char*>(r), 9));
    player_.level = r[9];
    for (int g = 0; g < 4; ++g) player_.colours[g] = r[0xA + g] & 3;
    for (int s = 0; s < 29; ++s) {
        player_.customSquares[s] = Square{r[0xE + 4 * s], r[0xF + 4 * s], r[0x10 + 4 * s], r[0x11 + 4 * s]};
        player_.savedSquares[s] = Square{r[0x83 + 4 * s], r[0x84 + 4 * s], r[0x85 + 4 * s], r[0x86 + 4 * s]};
    }
    player_.customLevel = r[0x82];
    for (int k = 0; k < 16; ++k)
        player_.savedObjects[k] = Object{r[0xF7 + 3 * k], r[0xF8 + 3 * k], r[0xF9 + 3 * k]};
    player_.savedLevel = r[0x127];
    player_.savedScore = static_cast<int32_t>(word(0x128) | static_cast<uint32_t>(word(0x12A)) << 16);
    player_.savedTimeLeft = static_cast<int16_t>(word(0x12C));
    player_.savedTimeTotal = static_cast<int16_t>(word(0x12E));
    player_.savedCustom = r[0x130];
    return true;
}

void Mystery::savePlayer() const {
    // f08_0000.
    uint8_t r[0x131] = {};
    auto word = [&](int at, int v) {
        r[at] = static_cast<uint8_t>(v);
        r[at + 1] = static_cast<uint8_t>(v >> 8);
    };
    std::memcpy(r, player_.name.data(), std::min<size_t>(player_.name.size(), 8));
    r[9] = player_.level;
    for (int g = 0; g < 4; ++g) r[0xA + g] = player_.colours[g];
    for (int s = 0; s < 29; ++s) {
        const Square& c = player_.customSquares[s];
        const Square& v = player_.savedSquares[s];
        const uint8_t cb[4] = {c.puzzle, c.level, c.object, c.state}, vb[4] = {v.puzzle, v.level, v.object, v.state};
        std::memcpy(r + 0xE + 4 * s, cb, 4);
        std::memcpy(r + 0x83 + 4 * s, vb, 4);
    }
    r[0x82] = player_.customLevel;
    for (int k = 0; k < 16; ++k) {
        const Object& o = player_.savedObjects[k];
        r[0xF7 + 3 * k] = o.museum, r[0xF8 + 3 * k] = o.square, r[0xF9 + 3 * k] = o.found;
    }
    r[0x127] = player_.savedLevel;
    word(0x128, player_.savedScore & 0xFFFF);
    word(0x12A, (player_.savedScore >> 16) & 0xFFFF);
    word(0x12C, player_.savedTimeLeft);
    word(0x12E, player_.savedTimeTotal);
    r[0x130] = player_.savedCustom;
    std::error_code ec;
    std::filesystem::create_directories(options_.saveDir, ec);
    std::ofstream out(playerPath(), std::ios::binary);
    out.write(reinterpret_cast<const char*>(r), sizeof r);
    if (!out) warnOnce("could not open file for write: " + playerPath());
}

void Mystery::loadLook() {
    // f08_11ea: MEDISON.COL, Edison's last look (4 bytes); made if missing.
    std::ifstream in(findPath(options_.saveDir + "/" + dataString(0x926)), std::ios::binary);
    uint8_t c[4];
    if (!in || !in.read(reinterpret_cast<char*>(c), 4)) {
        saveLook();
        return;
    }
    for (int g = 0; g < 4; ++g) player_.colours[g] = c[g] & 3;
}

void Mystery::saveLook() const {
    // f08_1102.
    std::error_code ec;
    std::filesystem::create_directories(options_.saveDir, ec);
    std::ofstream out(options_.saveDir + "/" + dataString(0x8EF), std::ios::binary);
    out.write(reinterpret_cast<const char*>(player_.colours), 4);
}

void Mystery::saveGame() {
    // f08_1f6e: the game in progress into the record, which is written.
    player_.savedLevel = player_.level;
    player_.savedSquares = squares_;
    player_.savedObjects = objects_;
    player_.savedScore = static_cast<int32_t>(score_);
    player_.savedTimeLeft = static_cast<int16_t>(timeLeft_);
    player_.savedTimeTotal = static_cast<int16_t>(timeTotal_);
    player_.savedCustom = customLevel_ ? 1 : 0;
    savePlayer();
}

void Mystery::loadSavedGame() {
    // f08_2092: back into the game ([B76B] = 1 skips the new board).
    player_.level = player_.savedLevel;
    squares_ = player_.savedSquares;
    objects_ = player_.savedObjects;
    score_ = player_.savedScore;
    timeLeft_ = player_.savedTimeLeft;
    timeTotal_ = player_.savedTimeTotal;
    customLevel_ = player_.savedCustom != 0;
    savedGame_ = true;
    objectCount_ = 0;
    for (const Object& o : objects_)
        if (o.museum != 0xFF) ++objectCount_;
}

void Mystery::storeCustomLevel() {
    // f08_1e2a: the edited board becomes the player's custom level.
    player_.customLevel = player_.level;
    player_.customSquares = squares_;
}

void Mystery::useCustomLevel() {
    // f08_1eca.
    player_.level = player_.customLevel;
    squares_ = player_.customSquares;
    customLevel_ = true;
}

void Mystery::runOff() {
    // f08_1264: Edison runs off to start the search.
    struct Frame {
        uint16_t sprite;
        int x, y;
    };
    static const Frame kRun[10] = {
        {0x2319, 0x82, 0x46}, {0x231A, 0x82, 0x48}, {0x231B, 0x82, 0x30}, {0x231C, 0x6E, 0x1E},
        {0x231D, 0x5A, 0x2E}, {0x231E, 0x48, 0x44}, {0x231F, 0x34, 0x34}, {0x2320, 0x20, 0x29},
        {0x2321, 0x10, 0x24}, {0x2322, 0, 0x42},
    };
    select(2);
    music(0x1B);
    for (int i = 0; i < 11; ++i) {
        if (i < 10) drawLogo(kRun[i].x, kRun[i].y + kSceneTop, kRun[i].sprite);
        copyArea(2, 1, 0, kSceneTop, kSceneW, kSceneH);
        duplicateArea(2, 2, 0, 0, kSceneW, kSceneH, 0, kSceneTop);
        waitCountdown(2);
    }
    select(1);
}

// --- the setup state machine (f08_232c) ----------------------------------

int Mystery::setup(int mode) {
    // mode 1: the first time (the title, then a new player); 0 after a
    // game ("Do you want to play again?"); 0x0F after "play again" from the
    // map's quit button. Returns 1 when the player leaves.
    if (mode == 1 && !skipToLevelPick_) title();
    loadLook();
    customLevel_ = false;
    savedGame_ = false;
    idle_ = false;
    playedAgain_ = mode != 1;  // [B786]
    bool returning = playedAgain_;
    if (mode == 1) std::fill(std::begin(player_.colours), std::end(player_.colours), 0);
    setupScreen();
    menuEvent_ = 0;
    const bool skip = skipToLevelPick_;
    skipToLevelPick_ = false;
    int step = 0;
    while (step != -1) {
        switch (step) {
        case 0:
            scene(0);
            if (mode == 0) {
                if (!playAgain()) {
                    fill(0, 0, Screen::kWidth, Screen::kHeight, 0);
                    return 1;
                }
                step = 3;
            } else {
                // (Mode 0x0F also adds an empty panel, DS:0728.)
                step = skip ? 11 : mode == 0x0F ? 3 : 1;
            }
            break;
        case 1:
            nameEntry();
            step = 2;
            break;
        case 2:
            // A returning player's record, with Edison's look.
            if (loadPlayer()) {
                returning = true;
                step = 3;
            } else {
                step = 4;
            }
            break;
        case 3:
            step = askChangeLooks() ? 4 : 8;
            break;
        case 4:
            scene(1);
            step = 5;
            break;
        case 5:
            customizer();
            step = 6;
            break;
        case 6:
            scene(2);
            step = 7;
            break;
        case 7:
            scene(3);
            step = returning ? 8 : 11;
            break;
        case 8:
            if (player_.savedLevel == 0xFF || !askSavedGame()) step = 9;
            break;
        case 9:
            if (player_.customLevel == 0xFF)
                step = 10;
            else if (!askCustomLevel())
                step = 11;
            break;
        case 10:
            step = askLastLevel() ? 12 : 11;
            break;
        case 11:
            pickLevel();
            step = 12;
            break;
        case 12:
            letsDoIt();
            step = 13;
            break;
        default:
            runOff();
            step = -1;
            break;
        }
        if (menuEvent_ == 1 || menuEvent_ == 4) {
            // Make (1) or change (4) the custom level, then the courtyard
            // again and its questions from the custom level on.
            if (menuEvent_ == 4) useCustomLevel();
            customLevelEditor(menuEvent_ == 4);
            setupScreen();
            scene(4);
            ctx_.countdown[0] = 0xA;
            bool down = false;
            while (ctx_.countdown[0] != 0 && !down) {
                ctx_.pump();
                int mx, my;
                ctx_.platform.mouse(&mx, &my, &down);
            }
            step = 9;
        } else if (menuEvent_ == 2) {
            useCustomLevel();
            step = 12;
        } else if (menuEvent_ == 3) {
            loadSavedGame();
            step = 12;
        }
        menuEvent_ = 0;
    }
    savePlayer();
    saveLook();
    loadHighScores();
    select(1);
    fill(0, 0, Screen::kWidth, Screen::kHeight, 0);
    return 0;
}

}  // namespace edison
