// The drawing and input helpers both games' own code has (MALL.EXE
// segment 6, WINMAIN.EXE segment 28): clamped drawing that goes through
// screen 3 when it draws on the display, saved areas, text and waits.

#include "artech/game.h"

#include <algorithm>
#include <cstdlib>

namespace edison {

void ArtechGame::drawLogo(int x, int y, uint16_t id) {
    const Bitmap& bmp = ctx_.bitmap(id);
    x = std::max(0, std::min(x, Screen::kWidth - bmp.width));
    y = std::max(0, std::min(y, Screen::kHeight - bmp.height));
    drawVia3(x, y, bmp.width, bmp.height, [&](int s) { ctx_.screens.drawSprite(s, bmp, x, y); });
}

void ArtechGame::drawShifted(int x, int y, uint16_t id, int add) {
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

void ArtechGame::fillPolygon(const std::vector<std::pair<int, int>>& pts, uint8_t colour, bool edges) {
    const int n = static_cast<int>(pts.size());
    if (edges)
        for (int k = 0; k < n; ++k)
            line(pts[k].first, pts[k].second, pts[(k + 1) % n].first, pts[(k + 1) % n].second, colour);
    Screen& s = ctx_.screens[current_];
    scanPolygon(pts, [&](int x, int y) { s.pixels[static_cast<size_t>(y) * Screen::kWidth + x] = colour; });
}

void ArtechGame::fillPolygonWith(const std::vector<std::pair<int, int>>& pts, uint16_t bitmap) {
    const Bitmap& bmp = ctx_.bitmap(bitmap);
    if (bmp.width <= 0 || bmp.height <= 0 || current_ == 1) return;
    Screen& s = ctx_.screens[current_];
    scanPolygon(pts, [&](int x, int y) {
        s.pixels[static_cast<size_t>(y) * Screen::kWidth + x] = bmp.at(x % bmp.width, y % bmp.height);
    });
}

template <class Plot>
void ArtechGame::scanPolygon(const std::vector<std::pair<int, int>>& pts, Plot plot) {
    const int n = static_cast<int>(pts.size());
    if (n < 3) return;
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
            for (int x = std::max(xs[k], 0); x <= std::min(xs[k + 1], Screen::kWidth - 1); ++x) plot(x, y);
    }
}

void ArtechGame::frame(int x, int y, int w, int h, uint8_t colour) {
    --w, --h;
    line(x, y, x + w, y, colour);
    line(x + w, y, x + w, y + h, colour);
    line(x, y, x, y + h, colour);
    line(x, y + h, x + w, y + h, colour);
}

void ArtechGame::fill(int x, int y, int w, int h, uint8_t colour) {
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

void ArtechGame::text(int x, int y, const std::string& s, int colour) {
    colour = std::clamp(colour, 0, 255);
    x = std::max(x, 0);
    y = std::max(y, 0);
    const int w = font_->width(s);
    drawVia3(x, y, w, font_->height(), [&](int scr) {
        font_->draw(ctx_.screens[scr], x, y, s, static_cast<uint8_t>(colour));
    });
}

void ArtechGame::duplicateArea(int src, int dst, int sx, int sy, int w, int h, int dx, int dy) {
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

int ArtechGame::saveArea(int x, int y, int w, int h) {
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

void ArtechGame::restoreArea(int handle, bool onlyCurrent) {
    auto it = saved_.find(handle);
    if (it == saved_.end()) return;
    const SavedArea& a = it->second;
    // f06_14fc: onto the display through screen 3, or onto the current
    // screen when that isn't the display.
    for (int s : current_ == 1 && !onlyCurrent ? std::vector<int>{3, 1} : std::vector<int>{current_}) {
        Screen& scr = ctx_.screens[s];
        for (int row = 0; row < a.h; ++row)
            std::copy_n(a.pixels.begin() + static_cast<size_t>(row) * a.w, a.w,
                        scr.pixels.begin() + static_cast<size_t>(a.y + row) * Screen::kWidth + a.x);
    }
    saved_.erase(it);
}

void ArtechGame::recolour(int x, int y, int w, int h, uint8_t from, uint8_t to) {
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

void ArtechGame::clearInput() {
    // f06_2ccc: drop pending clicks and keys.
    int x, y;
    while (ctx_.platform.takeClick(&x, &y)) {}
    while (ctx_.platform.takeKey()) {}
}

bool ArtechGame::anyInput() {
    int x, y;
    return ctx_.platform.takeClick(&x, &y) || ctx_.platform.takeKey() != 0;
}

void ArtechGame::waitCountdown(int tenths) {
    ctx_.countdown[0] = tenths;
    while (ctx_.countdown[0] != 0) ctx_.pump();
}

void ArtechGame::drawOpaque(int x, int y, uint16_t id) {
    const Bitmap& bmp = ctx_.bitmap(id);
    if (x + bmp.width > Screen::kWidth) x = Screen::kWidth - bmp.width - 1;
    if (y + bmp.height > Screen::kHeight) y = Screen::kHeight - bmp.height - 1;
    x = std::max(x, 0);
    y = std::max(y, 0);
    drawVia3(x, y, bmp.width, bmp.height, [&](int s) { ctx_.screens.drawSprite(s, bmp, x, y, true); });
}

void ArtechGame::line(int x0, int y0, int x1, int y1, uint8_t colour) {
    x1 = std::min(x1, Screen::kWidth - 1);
    y1 = std::min(y1, Screen::kHeight - 1);
    x0 = std::max(x0, 0);
    y0 = std::max(y0, 0);
    auto plot = [&](int s) {
        Screen& scr = ctx_.screens[s];
        const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy, x = x0, y = y0;
        for (;;) {
            if (x >= 0 && x < Screen::kWidth && y >= 0 && y < Screen::kHeight)
                scr.pixels[static_cast<size_t>(y) * Screen::kWidth + x] = colour;
            if (x == x1 && y == y1) break;
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x += sx; }
            if (e2 <= dx) { err += dx; y += sy; }
        }
    };
    const int x = std::min(x0, x1), y = std::min(y0, y1);
    drawVia3(x, y, std::abs(x1 - x0) + 1, std::abs(y1 - y0) + 1, plot);
}

bool ArtechGame::waitOrClick(int tenths) {
    ctx_.countdown[0] = tenths;
    while (ctx_.countdown[0] != 0) {
        ctx_.pump();
        if (anyInput()) return true;
    }
    return false;
}

void ArtechGame::drawCentred(int x, int y, uint16_t id) {
    // f06_189a at 1:1.
    const Bitmap& bmp = ctx_.bitmap(id);
    drawLogo(x - bmp.width / 2, y - bmp.height / 2, id);
}

void ArtechGame::drawScaledCentred(int x, int y, int scaleX, int scaleY, uint16_t id) {
    // f06_189a: a sprite scaled (256 = 1:1) about its centre, colour 0 clear.
    const Bitmap& bmp = ctx_.bitmap(id);
    const int w = std::max(1, bmp.width * scaleX / 256), h = std::max(1, bmp.height * scaleY / 256);
    const int left = x - w / 2, top = y - h / 2;
    drawVia3(left, top, w, h, [&](int s) {
        Screen& scr = ctx_.screens[s];
        for (int r = 0; r < h; ++r)
            for (int c = 0; c < w; ++c) {
                const uint8_t p = bmp.at(c * bmp.width / w, r * bmp.height / h);
                const int px = left + c, py = top + r;
                if (p && px >= 0 && py >= 0 && px < Screen::kWidth && py < Screen::kHeight)
                    scr.pixels[static_cast<size_t>(py) * Screen::kWidth + px] = p;
            }
    });
}

}  // namespace edison
