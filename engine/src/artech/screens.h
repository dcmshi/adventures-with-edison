#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "formats/bitmap.h"

namespace edison {

// Inclusive rectangle, as the Artech library stores them.
struct Rect {
    int left = 0, top = 0, right = -1, bottom = -1;

    bool overlaps(const Rect& o) const {
        return !(right < o.left || o.right < left || bottom < o.top || o.bottom < top);
    }
    bool contains(const Rect& o) const {
        return left <= o.left && top <= o.top && o.right <= right && o.bottom <= bottom;
    }
};

// One of the library's numbered 640x400 8-bit drawing surfaces. The
// launcher uses 1 = what's on the display, 2 = composition, 3 = backdrop.
struct Screen {
    static constexpr int kWidth = 640, kHeight = 400;
    std::vector<uint8_t> pixels = std::vector<uint8_t>(kWidth * kHeight, 0);
    Palette palette{};

    void clear() { std::fill(pixels.begin(), pixels.end(), 0); }
};

class Screens {
public:
    static constexpr int kCount = 4;
    Screen& operator[](int n) { return screens_[n]; }
    const Screen& operator[](int n) const { return screens_[n]; }

    // copy_area(src, dst, x, y, w, h): same position in both surfaces, clipped.
    void copyArea(int src, int dst, int x, int y, int w, int h);
    void copyAll(int src, int dst) { screens_[dst].pixels = screens_[src].pixels; }
    // show_logo: draws a bitmap with colour 0 transparent, clipped.
    // Colour 0 is transparent unless `opaque` (show_logo vs show_Clogo).
    void drawSprite(int dst, const Bitmap& bmp, int x, int y, bool opaque = false);

private:
    std::array<Screen, kCount> screens_;
};

}  // namespace edison
