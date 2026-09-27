// MALL.EXE segment 6: the game's drawing and UI helpers.

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>

#include "mystery/mystery.h"

namespace edison {

void Mystery::drawLogo(int x, int y, uint16_t id) {
    const Bitmap& bmp = ctx_.bitmap(id);
    x = std::max(0, std::min(x, Screen::kWidth - bmp.width));
    y = std::max(0, std::min(y, Screen::kHeight - bmp.height));
    drawVia3(x, y, bmp.width, bmp.height, [&](int s) { ctx_.screens.drawSprite(s, bmp, x, y); });
}

void Mystery::drawShifted(int x, int y, uint16_t id, int add) {
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
}

void Mystery::fillPolygon(const std::vector<std::pair<int, int>>& pts, uint8_t colour) {
    const int n = static_cast<int>(pts.size());
    if (n < 3) return;
    Screen& s = ctx_.screens[current_];
    int top = Screen::kHeight, bottom = -1;
    for (auto [x, y] : pts) top = std::min(top, y), bottom = std::max(bottom, y);
    for (int y = std::max(top, 0); y <= std::min(bottom, Screen::kHeight - 1); ++y) {
        std::vector<int> xs;
        for (int k = 0; k < n; ++k) {
            auto [ax, ay] = pts[k];
            auto [bx, by] = pts[(k + 1) % n];
            if (ay == by || y < std::min(ay, by) || y >= std::max(ay, by)) continue;
            xs.push_back(ax + (y - ay) * (bx - ax) / (by - ay));
        }
        std::sort(xs.begin(), xs.end());
        for (size_t k = 0; k + 1 < xs.size(); k += 2)
            for (int x = std::max(xs[k], 0); x <= std::min(xs[k + 1], Screen::kWidth - 1); ++x)
                s.pixels[static_cast<size_t>(y) * Screen::kWidth + x] = colour;
    }
}

void Mystery::frame(int x, int y, int w, int h, uint8_t colour) {
    --w, --h;
    line(x, y, x + w, y, colour);
    line(x + w, y, x + w, y + h, colour);
    line(x, y, x, y + h, colour);
    line(x, y + h, x + w, y + h, colour);
}

void Mystery::fill(int x, int y, int w, int h, uint8_t colour) {
    if (x + w >= Screen::kWidth) w = Screen::kWidth - x - 1;
    if (y + h >= Screen::kHeight) h = Screen::kHeight - y - 1;
    x = std::max(x, 0);
    y = std::max(y, 0);
    drawVia3(x, y, w, h, [&](int s) {
        Screen& scr = ctx_.screens[s];
        for (int row = y; row < y + h; ++row)
            std::fill_n(scr.pixels.begin() + static_cast<size_t>(row) * Screen::kWidth + x, w, colour);
    });
}

void Mystery::text(int x, int y, const std::string& s, int colour) {
    colour = std::clamp(colour, 0, 255);
    x = std::max(x, 0);
    y = std::max(y, 0);
    const int w = font_->width(s);
    drawVia3(x, y, w, font_->height(), [&](int scr) {
        font_->draw(ctx_.screens[scr], x, y, s, static_cast<uint8_t>(colour));
    });
}

void Mystery::duplicateArea(int src, int dst, int sx, int sy, int w, int h, int dx, int dy) {
    std::vector<uint8_t> block(static_cast<size_t>(w) * h);
    const Screen& from = ctx_.screens[src];
    for (int row = 0; row < h; ++row)
        for (int col = 0; col < w; ++col) {
            const int x = sx + col, y = sy + row;
            if (x >= 0 && x < Screen::kWidth && y >= 0 && y < Screen::kHeight)
                block[static_cast<size_t>(row) * w + col] = from.pixels[static_cast<size_t>(y) * Screen::kWidth + x];
        }
    Screen& to = ctx_.screens[dst];
    for (int row = 0; row < h; ++row)
        for (int col = 0; col < w; ++col) {
            const int x = dx + col, y = dy + row;
            if (x >= 0 && x < Screen::kWidth && y >= 0 && y < Screen::kHeight)
                to.pixels[static_cast<size_t>(y) * Screen::kWidth + x] = block[static_cast<size_t>(row) * w + col];
        }
}

int Mystery::saveArea(int x, int y, int w, int h) {
    if (x + w >= Screen::kWidth) w = Screen::kWidth - x - 1;
    if (y + h >= Screen::kHeight) h = Screen::kHeight - y - 1;
    x = std::max(x, 0);
    y = std::max(y, 0);
    SavedArea area{x, y, w, h, std::vector<uint8_t>(static_cast<size_t>(w) * h)};
    const Screen& s = ctx_.screens[current_];
    for (int row = 0; row < h; ++row)
        std::copy_n(s.pixels.begin() + static_cast<size_t>(y + row) * Screen::kWidth + x, w,
                    area.pixels.begin() + static_cast<size_t>(row) * w);
    saved_[nextHandle_] = std::move(area);
    return nextHandle_++;
}

void Mystery::restoreArea(int handle) {
    auto it = saved_.find(handle);
    if (it == saved_.end()) return;
    const SavedArea& a = it->second;
    for (int s : {3, 1}) {  // restored through screen 3, as the original does
        Screen& scr = ctx_.screens[s];
        for (int row = 0; row < a.h; ++row)
            std::copy_n(a.pixels.begin() + static_cast<size_t>(row) * a.w, a.w,
                        scr.pixels.begin() + static_cast<size_t>(a.y + row) * Screen::kWidth + a.x);
    }
    saved_.erase(it);
}

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

void Mystery::recolour(int x, int y, int w, int h, uint8_t from, uint8_t to) {
    if (x + w >= Screen::kWidth) w = Screen::kWidth - x - 1;
    if (y + h >= Screen::kHeight) h = Screen::kHeight - y - 1;
    x = std::max(x, 0);
    y = std::max(y, 0);
    drawVia3(x, y, w, h, [&](int s) {
        Screen& scr = ctx_.screens[s];
        for (int row = y; row < y + h; ++row)
            for (int col = x; col < x + w; ++col) {
                uint8_t& p = scr.pixels[static_cast<size_t>(row) * Screen::kWidth + col];
                if (p == from) p = to;
            }
    });
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
    std::ifstream in(cdRoot_ + "/MYSTERY/" + name + ".wav", std::ios::binary);
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

void Mystery::clearInput() {
    // f06_2ccc: drop pending clicks and keys.
    int x, y;
    while (ctx_.platform.takeClick(&x, &y)) {}
    while (ctx_.platform.takeKey()) {}
}

bool Mystery::anyInput() {
    int x, y;
    return ctx_.platform.takeClick(&x, &y) || ctx_.platform.takeKey() != 0;
}

void Mystery::waitCountdown(int tenths) {
    ctx_.countdown[0] = tenths;
    while (ctx_.countdown[0] != 0) ctx_.pump();
}

}  // namespace edison
