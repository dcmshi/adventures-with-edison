// MALL.EXE segment 6: the game's drawing and UI helpers.

#include <algorithm>
#include "formats/paths.h"
#include <cstdio>
#include <fstream>
#include <iterator>

#include "mystery/mystery.h"

namespace edison {

int Mystery::speechBox(int x, int y, const std::vector<std::string>& lines, int side, bool save) {
    const int fh = font_->height();
    int widest = 0;
    for (const auto& l : lines) widest = std::max(widest, font_->width(l));
    const int w = std::min(widest + 2 * fh, Screen::kWidth - 1);
    const int h = std::min(static_cast<int>(lines.size() + 1) * fh, Screen::kHeight - 1);
    const Bitmap& tl = ctx_.bitmap(0x210E);
    const Bitmap& tr = ctx_.bitmap(0x210F);
    const Bitmap& bl = ctx_.bitmap(0x210C);
    const Bitmap& br = ctx_.bitmap(0x210D);
    const Bitmap& tailRight = ctx_.bitmap(0x2106);
    const Bitmap& tailLeft = ctx_.bitmap(0x2107);
    const int handle = save ? saveArea(x, y, w + tl.width, h + tailLeft.height) : 0;

    // The body: three filled bands, then the rounded corners.
    fill(x + tl.width, y, w - 2 * tr.height, h, 15);
    fill(x, y + tl.height, tr.width, h - 2 * tl.height, 15);
    fill(x + w - tl.width, y + tl.height, tr.width, h - 2 * tl.height, 15);
    drawLogo(x, y, 0x210E);
    drawLogo(x + w - tr.width, y, 0x210F);
    drawLogo(x, y + h - bl.height, 0x210C);
    drawLogo(x + w - tl.width, y + h - br.height, 0x210D);
    switch (side) {
    case 0: drawLogo(x + w - 2 * tailRight.width, y + h, 0x2106); break;
    case 1: drawLogo(x + 2 * tailRight.width, y + h, 0x2106); break;
    case 2: drawLogo(x + w - 2 * tailLeft.width, y + h, 0x2107); break;
    case 3: drawLogo(x + 2 * tailLeft.width, y + h, 0x2107); break;
    default: break;
    }
    for (size_t i = 0; i < lines.size(); ++i)
        text(x + (w - font_->width(lines[i])) / 2, static_cast<int>(i) * fh + y + fh / 2, lines[i], ui_[12]);
    return handle;
}

void Mystery::pauseGame() {
    // f06_229a, from a panel poll with P held (f06_219c): "Game Paused!"
    // in a box in the middle of the display until Space or a mouse button
    // is held. Its flag [7576] is the timer DLL's (SETUPTIMERDLL, 45:0160):
    // no ticks while paused, so the clocks stand still.
    const int previous = current();
    select(1);
    const std::string first = dataString(0x269), second = dataString(0x29A);
    const int w = std::max(font_->width(first), font_->width(second)) + 0x20, h = 3 * 0x10;  // f06_0956
    const int x = (Screen::kWidth - w) >> 1, y = (Screen::kHeight - h) >> 1;
    const int saved = saveArea(x, y, w, h);
    fill(x, y, w, h, 0xF4);
    // f06_08c0: the outline in MALL's line (on the display the bottom
    // right corner, both lines' far end, stays off).
    displayLine(x, y, x + w - 1, y, 0xFF);
    displayLine(x + w - 1, y, x + w - 1, y + h - 1, 0xFF);
    displayLine(x, y, x, y + h - 1, 0xFF);
    displayLine(x, y + h - 1, x + w - 1, y + h - 1, 0xFF);
    text(x + 0x10, y + 8, first, 0xFF);
    text(x + 0x10, y + 0x18, second, 0xFF);
    clearInput();
    for (;;) {
        int mx, my;
        bool down;
        ctx_.platform.mouse(&mx, &my, &down);
        if (down || ctx_.platform.takeKeyIf(' ')) break;
        if (!ctx_.platform.pumpEvents()) throw GameContext::Closed{};
        ctx_.platform.present(ctx_.screens[1], ctx_.displayPalette);
    }
    ctx_.timer.reset(ctx_.platform.milliseconds());
    clearInput();
    restoreArea(saved);
    select(previous);
}

void Mystery::computeUiColours() {
    // Nearest match in the display palette (entries 1-253). The original
    // reads the table as signed chars, so components above 127 count as
    // negative; kept, since it decides which colours the UI really uses.
    for (int k = 0; k < 15; ++k) {
        const auto* want = &data_[0xBE + 3 * k];
        int best = 0x7FFF, bestIndex = 0;
        for (int a = 1; a < 0xFE; ++a) {
            const Rgb& c = ctx_.displayPalette[a];
            // The palette is kept in B, G, R order, and the table with it.
            const int db = (static_cast<int8_t>(want[0]) - c.b) / 8;
            const int dg = (static_cast<int8_t>(want[1]) - c.g) / 8;
            const int dr = (static_cast<int8_t>(want[2]) - c.r) / 8;
            const int d = dr * dr + dg * dg + db * db;
            if (d < best) {
                best = d;
                bestIndex = a;
            }
        }
        ui_[k] = static_cast<uint8_t>(bestIndex);
    }
}

void Mystery::sound(uint16_t id) {
    // Name table at DS:0566 (far pointers), indexed by id - 0x4010.
    const int index = id - 0x4010;
    if (index < 0 || 0x566 + 4 * index + 1 >= static_cast<int>(data_.size())) return;
    const std::string name = dataString(dataWord(static_cast<uint16_t>(0x566 + 4 * index)));
    std::ifstream in(findPath(cdRoot_ + "/MYSTERY/" + name + ".wav"), std::ios::binary);
    if (!in) {
        warnOnce("missing sound " + name + ".wav");
        return;
    }
    const std::vector<uint8_t> wav((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ctx_.platform.playWav(wav);
}

void Mystery::music(uint16_t id) {
    if (options_.music) ctx_.platform.sendFm(id);
}

}  // namespace edison
