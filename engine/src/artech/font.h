#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "artech/screens.h"

namespace edison {

// Bitmap font (archive group 01), drawn by the library's text routines
// (MALL.EXE segment 44):
//     u8 bytes_per_row, u8 height, u8 first_char, u8 last_char,
//     u8 width[last - first + 1],
//     glyphs: height rows of bytes_per_row bytes each, 1 bit per pixel,
//     most significant bit leftmost.
// Text is drawn in one colour; clear bits are transparent.
class Font {
public:
    bool load(const std::vector<uint8_t>& data, std::string* error);
    bool loaded() const { return !data_.empty(); }

    int height() const { return height_; }
    int width(const std::string& text) const;  // text_width (f44_0478)
    // Draws text with its top-left at (x, y), clipped to the screen.
    void draw(Screen& screen, int x, int y, const std::string& text, uint8_t colour) const;

private:
    int glyphIndex(char c) const;

    std::vector<uint8_t> data_;
    int bytesPerRow_ = 0, height_ = 0, first_ = 0, last_ = -1;
};

}  // namespace edison
