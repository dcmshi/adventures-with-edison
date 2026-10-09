// The drawing and input helpers both games' own code has (MALL.EXE
// segment 6, WINMAIN.EXE segment 28): clamped drawing that goes through
// screen 3 when it draws on the display, saved areas, text and waits.

#include "artech/game.h"

#include <algorithm>
#include <cstdlib>

namespace edison {

void ArtechGame::drawLogo(int x, int y, uint16_t id) {  // MALL f06_120c
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

bool ArtechGame::librarySpans(const std::vector<std::pair<int, int>>& pts, int& top, std::vector<std::pair<int, int>>& span, bool needArea,
                               size_t maxPoints) const {
    // WMAIN.EXE's library polygon (f81_0280 / f63_20e4): cut to the clip,
    // an edge of it at a time (f83_0065); then each row's leftmost and
    // rightmost x along the edges (f81_0000). Two points are a line (its
    // one edge both ways: MALL's Folded Cube draws its edges so).
    if (pts.size() < 2) return false;
    std::vector<std::pair<int, int>> poly(pts);
    auto cut = [&](auto in, auto at) {
        std::vector<std::pair<int, int>> out;
        for (size_t k = 0; k < poly.size(); ++k) {
            const auto p = poly[k], q = poly[(k + 1) % poly.size()];
            if (in(p)) out.push_back(p);
            if (in(p) != in(q)) out.push_back(at(p, q));
        }
        poly = std::move(out);
    };
    auto atX = [](int x) {
        return [x](std::pair<int, int> p, std::pair<int, int> q) {
            return std::pair<int, int>{x, p.second + (x - p.first) * (q.second - p.second) / (q.first - p.first)};
        };
    };
    auto atY = [](int y) {
        return [y](std::pair<int, int> p, std::pair<int, int> q) {
            return std::pair<int, int>{p.first + (y - p.second) * (q.first - p.first) / (q.second - p.second), y};
        };
    };
    cut([&](auto p) { return p.first >= clip_.x0; }, atX(clip_.x0));
    if (!poly.empty()) cut([&](auto p) { return p.first <= clip_.x1; }, atX(clip_.x1));
    if (!poly.empty()) cut([&](auto p) { return p.second >= clip_.y0; }, atY(clip_.y0));
    if (!poly.empty()) cut([&](auto p) { return p.second <= clip_.y1; }, atY(clip_.y1));
    if (poly.size() < 2) return false;
    if (maxPoints && poly.size() > maxPoints) return false;
    if (needArea) {
        bool wide = false, tall = false;
        for (auto& p : poly) wide |= p.first != poly[0].first, tall |= p.second != poly[0].second;
        if (!wide || !tall) return false;
    }
    top = poly[0].second;
    int bottom = top;
    for (auto [x, y] : poly) top = std::min(top, y), bottom = std::max(bottom, y);
    const int rows = bottom - top + 1;
    span.assign(static_cast<size_t>(rows), {0x7FFF, 0});
    auto widen = [&](int y, int x0, int x1) {
        auto& s = span[static_cast<size_t>(y - top)];
        s.first = std::min(s.first, x0), s.second = std::max(s.second, x1);
    };
    for (size_t k = 0; k < poly.size(); ++k) {
        // Each edge from its top end down, as the library's line: P = 2 x +
        // (2k + 1) dx / dy in 16.16, the next row's x ceil(int(P) / 2).
        const auto p = poly[(k + poly.size() - 1) % poly.size()], q = poly[k];
        const auto& hi = p.second <= q.second ? p : q;
        const auto& lo = p.second <= q.second ? q : p;
        const int dx = std::abs(lo.first - hi.first), dy = lo.second - hi.second;
        if (dx == 0) {
            for (int y = hi.second; y <= lo.second; ++y) widen(y, hi.first, hi.first);
            continue;
        }
        if (dy == 0) {
            widen(hi.second, std::min(p.first, q.first), std::max(p.first, q.first));
            continue;
        }
        const uint32_t step = (static_cast<uint32_t>(dx / dy) << 16) | ((static_cast<uint32_t>(dx % dy) << 16) / static_cast<uint32_t>(dy));
        int x = hi.first, y = hi.second;
        if (lo.first > hi.first) {
            uint32_t at = (static_cast<uint32_t>(2 * x) << 16) + step;
            for (int r = 0; r < dy; ++r, ++y, at += 2 * step) {
                const int i = static_cast<int>(at >> 16), next = (i >> 1) + (i & 1);
                widen(y, x, next != x ? next - 1 : next);
                x = next;
            }
            widen(y, x, lo.first);
        } else {
            uint32_t at = (static_cast<uint32_t>(2 * x) << 16) - step;
            for (int r = 0; r < dy; ++r, ++y, at -= 2 * step) {
                const int i = static_cast<int>(at >> 16), next = (i >> 1) + (i & 1);
                widen(y, next != x ? next + 1 : next, x);
                x = next;
            }
            widen(y, lo.first, x);
        }
    }
    return true;
}

void ArtechGame::fillPolygonSolid(const std::vector<std::pair<int, int>>& pts, uint8_t colour, bool needArea, size_t maxPoints) {
    // draw_poly as WMAIN.EXE's library has it (f63_1fbf → f81_0280): the
    // spans above, each filled with the colour.
    int top = 0;
    std::vector<std::pair<int, int>> span;
    if (current_ == 1 || !librarySpans(pts, top, span, needArea, maxPoints)) return;
    Screen& s = ctx_.screens[current_];
    for (size_t r = 0; r < span.size(); ++r) {
        const auto [left, right] = span[r];
        const int y = top + static_cast<int>(r);
        if (left > right || y < 0 || y >= Screen::kHeight) continue;
        for (int x = std::max(left, 0); x <= std::min(right, Screen::kWidth - 1); ++x) s.pixels[static_cast<size_t>(y) * Screen::kWidth + x] = colour;
    }
}

void ArtechGame::fillPolygonStretched(const std::vector<std::pair<int, int>>& pts, uint16_t bitmap) {
    const Bitmap& bmp = ctx_.bitmap(bitmap);
    if (bmp.width <= 0 || bmp.height <= 0 || current_ == 1) return;
    int top = 0;
    std::vector<std::pair<int, int>> span;
    if (!librarySpans(pts, top, span)) return;
    const int rows = static_cast<int>(span.size());
    // f82_02f0: the stretch.
    Screen& s = ctx_.screens[current_];
    const uint32_t rowStep = (static_cast<uint32_t>(bmp.height / rows) << 16) |
                             ((static_cast<uint32_t>(bmp.height % rows) << 16) / static_cast<uint32_t>(rows));
    uint32_t row = 0;  // 16.16
    for (int r = 0; r < rows; ++r, row += rowStep) {
        const auto [left, right] = span[static_cast<size_t>(r)];
        const int y = top + r;
        if (left > right || y < 0 || y >= Screen::kHeight) continue;
        const int len = right - left + 1;
        const uint32_t colStep = (static_cast<uint32_t>(bmp.width / len) << 16) |
                                 ((static_cast<uint32_t>(bmp.width % len) << 16) / static_cast<uint32_t>(len));
        uint32_t col = 0;
        const int srcRow = std::min(static_cast<int>(row >> 16), bmp.height - 1);
        for (int x = left; x <= right; ++x, col += colStep)
            if (x >= 0 && x < Screen::kWidth)
                s.pixels[static_cast<size_t>(y) * Screen::kWidth + x] =
                    bmp.at(std::min(static_cast<int>(col >> 16), bmp.width - 1), srcRow);
    }
}

template <class Plot>
void ArtechGame::scanPolygon(const std::vector<std::pair<int, int>>& pts, Plot plot) {
    const int n = static_cast<int>(pts.size());
    if (n < 3) return;
    int top = Screen::kHeight, bottom = -1;
    for (auto [x, y] : pts) top = std::min(top, y), bottom = std::max(bottom, y);
    for (int y = std::max(top, clip_.y0); y <= std::min(bottom, clip_.y1); ++y) {
        std::vector<int> xs;
        for (int k = 0; k < n; ++k) {
            auto [ax, ay] = pts[k];
            auto [bx, by] = pts[(k + 1) % n];
            if (ay == by || y < std::min(ay, by) || y >= std::max(ay, by)) continue;
            xs.push_back(ax + (y - ay) * (bx - ax) / (by - ay));
        }
        std::sort(xs.begin(), xs.end());
        for (size_t k = 0; k + 1 < xs.size(); k += 2)
            for (int x = std::max(xs[k], clip_.x0); x <= std::min(xs[k + 1], clip_.x1); ++x) plot(x, y);
    }
}

void ArtechGame::displayLine(int x0, int y0, int x1, int y1, uint8_t colour) {
    x1 = std::min(x1, Screen::kWidth - 1);
    x0 = std::max(x0, 0);
    y1 = std::min(y1, Screen::kHeight - 1);
    y0 = std::max(y0, 0);
    if (current_ != 1) {
        line(x0, y0, x1, y1, colour);
        return;
    }
    current_ = 3;
    line(x0, y0, x1, y1, colour);
    current_ = 1;
    if (x0 == x1) copyArea(3, 1, x0, y0, 1, std::max(std::abs(y1 - y0), 1));
    else if (y0 == y1) copyArea(3, 1, x0, y0, std::max(std::abs(x1 - x0), 1), 1);
}

void ArtechGame::frame(int x, int y, int w, int h, uint8_t colour) {
    // MALL's f06_08c0: in its line (f06_1af8), so on the display the
    // bottom right corner, both lines' far end, stays off.
    --w, --h;
    displayLine(x, y, x + w, y, colour);
    displayLine(x + w, y, x + w, y + h, colour);
    displayLine(x, y, x, y + h, colour);
    displayLine(x, y + h, x + w, y + h, colour);
}

void ArtechGame::fill(int x, int y, int w, int h, uint8_t colour) {  // MALL f06_17c8
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

void ArtechGame::text(int x, int y, const std::string& s, int colour) {  // MALL f06_15b8
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

void ArtechGame::recolour(int x, int y, int w, int h, uint8_t from, uint8_t to) {  // MALL f06_16d8
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

void ArtechGame::spinCountdown(int tenths) {
    ctx_.countdown[0] = tenths;
    while (ctx_.countdown[0] != 0) ctx_.spin();
}

void ArtechGame::drawOpaque(int x, int y, uint16_t id) {  // MALL f06_1326
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
    // The library's line (MALL f57_0024, WINMAIN f59, EDISON f31: 32-bit
    // code): one run of pixels a row from the top end down, each row's run
    // ending where the line is half a row further on (16.16, rounded).
    struct Run {
        int start, length;
    };
    std::vector<Run> runs;
    int dx = x1 - x0, dy = y1 - y0, flags = 0;  // 1: x falls going down, 2: from (x1, y1)
    if (dx == 0) {
        if (dy < 0) dy = -dy, flags ^= 3;
        runs.assign(static_cast<size_t>(dy) + 1, Run{x0, 1});
    } else {
        if (dx < 0) dx = -dx, flags |= 1;
        if (dy == 0) {
            runs.push_back({(flags & 1) ? x1 : x0, dx + 1});
        } else {
            if (dy < 0) dy = -dy, flags ^= 3;
            const uint32_t slope = (static_cast<uint32_t>(dx) << 16) / static_cast<uint32_t>(dy);
            const int from = (flags & 2) ? x1 : x0, to = (flags & 2) ? x0 : x1;
            auto half = [](uint32_t pos) {  // shr 1, adc 0
                const int i = static_cast<int>((pos >> 16) & 0xFFFF);
                return (i >> 1) + (i & 1);
            };
            int x = from;
            if (!(flags & 1)) {
                uint32_t pos = (static_cast<uint32_t>(from) << 17) + slope;
                for (int k = 0; k < dy; ++k, pos += 2 * slope) {
                    const int m = half(pos);
                    runs.push_back({x, m == x ? 1 : m - x});
                    x = m;
                }
                runs.push_back({x, to - x + 1});
            } else {
                uint32_t pos = (static_cast<uint32_t>(from) << 17) - slope;
                for (int k = 0; k < dy; ++k, pos -= 2 * slope) {
                    const int m = half(pos);
                    runs.push_back(m == x ? Run{m, 1} : Run{m + 1, x - m});
                    x = m;
                }
                runs.push_back({to, x - to + 1});
            }
        }
    }
    const int top = (flags & 2) ? y1 : y0;
    auto plot = [&](int s) {
        Screen& scr = ctx_.screens[s];
        for (size_t r = 0; r < runs.size(); ++r) {
            const int y = top + static_cast<int>(r);
            if (y < 0 || y >= Screen::kHeight) continue;
            for (int x = runs[r].start; x < runs[r].start + runs[r].length; ++x)
                if (x >= 0 && x < Screen::kWidth) scr.pixels[static_cast<size_t>(y) * Screen::kWidth + x] = colour;
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
    // f06_189a: a sprite scaled (256 = 1:1) about its centre, colour 0 clear:
    // f40_03d2 → f41_0330 (32-bit code, WMAIN's f73_0324 again): (w *
    // scale) / 256 wide, each pixel the source's at a 16.16 step of 256 /
    // scale on each axis.
    const Bitmap& bmp = ctx_.bitmap(id);
    if (scaleX <= 0 || scaleY <= 0) return;
    const int w = bmp.width * scaleX / 256, h = bmp.height * scaleY / 256;
    if (w <= 0 || h <= 0) return;
    const uint32_t stepX = scaleStep(scaleX), stepY = scaleStep(scaleY);
    const int left = x - (w >> 1), top = y - (h >> 1);
    drawVia3(left, top, w, h, [&](int s) {
        Screen& scr = ctx_.screens[s];
        for (int r = 0; r < h; ++r)
            for (int c = 0; c < w; ++c) {
                const uint8_t p = bmp.at(static_cast<int>(c * stepX >> 16), static_cast<int>(r * stepY >> 16));
                const int px = left + c, py = top + r;
                if (p && px >= 0 && py >= 0 && px < Screen::kWidth && py < Screen::kHeight)
                    scr.pixels[static_cast<size_t>(py) * Screen::kWidth + px] = p;
            }
    });
}

}  // namespace edison
