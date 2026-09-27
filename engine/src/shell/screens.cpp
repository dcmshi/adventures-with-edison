#include "shell/screens.h"

#include <algorithm>

namespace edison {

void Screens::copyArea(int src, int dst, int x, int y, int w, int h) {
    const int x0 = std::max(x, 0), y0 = std::max(y, 0);
    const int x1 = std::min(x + w, Screen::kWidth), y1 = std::min(y + h, Screen::kHeight);
    if (x0 >= x1 || y0 >= y1 || src == dst) return;
    const auto& from = screens_[src].pixels;
    auto& to = screens_[dst].pixels;
    for (int row = y0; row < y1; ++row) {
        const size_t at = static_cast<size_t>(row) * Screen::kWidth + x0;
        std::copy(from.begin() + at, from.begin() + at + (x1 - x0), to.begin() + at);
    }
}

void Screens::drawSprite(int dst, const Bitmap& bmp, int x, int y) {
    auto& to = screens_[dst].pixels;
    for (int row = 0; row < bmp.height; ++row) {
        const int sy = y + row;
        if (sy < 0 || sy >= Screen::kHeight) continue;
        for (int col = 0; col < bmp.width; ++col) {
            const int sx = x + col;
            if (sx < 0 || sx >= Screen::kWidth) continue;
            const uint8_t c = bmp.at(col, row);
            if (c != 0) to[static_cast<size_t>(sy) * Screen::kWidth + sx] = c;
        }
    }
}

}  // namespace edison
