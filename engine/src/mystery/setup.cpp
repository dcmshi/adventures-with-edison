// MALL.EXE segment 8: setting up a game (player, Edison's looks, level).

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
    {0x2314, 0x82, 0x46, 0, 0, 0},
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
    ctx_.setDisplayPalette(2);
    ctx_.screens.copyAll(2, 1);
    music(0x29);
    ctx_.countdown[0] = 100;
    clearInput();
    while (ctx_.countdown[0] != 0 && !anyInput()) ctx_.pump();
    clearInput();
    select(previous);
    sound(0x4064);
}

void Mystery::setupScreen() {
    // The start of f08_232c: the courtyard backdrop on screen 2, shown.
    select(1);
    ctx_.blackout();
    ctx_.showFullScreen(0x1006, 2);
    ctx_.setDisplayPalette(2);  // f04_005c(2) (Edison's default colours are in the bitmap)
    ctx_.screens.copyAll(2, 1);
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

void Mystery::talk(int frames) {
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
        if (clicked) break;
    }
    select(1);
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

}  // namespace edison
