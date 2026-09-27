#include "artech/font.h"

namespace edison {

bool Font::load(const std::vector<uint8_t>& data, std::string* error) {
    if (data.size() < 4 || data[3] < data[2]) {
        if (error) *error = "bad font header";
        return false;
    }
    const int count = data[3] - data[2] + 1;
    if (data.size() < 4u + count + static_cast<size_t>(count) * data[0] * data[1]) {
        if (error) *error = "truncated font";
        return false;
    }
    data_ = data;
    bytesPerRow_ = data[0];
    height_ = data[1];
    first_ = data[2];
    last_ = data[3];
    return true;
}

int Font::glyphIndex(char c) const {
    const int code = static_cast<unsigned char>(c);
    return code >= first_ && code <= last_ ? code - first_ : -1;
}

int Font::width(const std::string& text) const {
    int w = 0;
    for (char c : text) {
        const int g = glyphIndex(c);
        if (g >= 0) w += data_[4 + g];
    }
    return w;
}

void Font::draw(Screen& screen, int x, int y, const std::string& text, uint8_t colour) const {
    const size_t glyphs = 4u + static_cast<size_t>(last_ - first_ + 1);
    for (char c : text) {
        const int g = glyphIndex(c);
        if (g < 0) continue;
        const int w = data_[4 + g];
        const uint8_t* bits = &data_[glyphs + static_cast<size_t>(g) * height_ * bytesPerRow_];
        for (int row = 0; row < height_; ++row) {
            const int sy = y + row;
            if (sy < 0 || sy >= Screen::kHeight) continue;
            for (int col = 0; col < w && col < 8 * bytesPerRow_; ++col) {
                const int sx = x + col;
                if (sx < 0 || sx >= Screen::kWidth) continue;
                if (bits[row * bytesPerRow_ + col / 8] >> (7 - col % 8) & 1)
                    screen.pixels[static_cast<size_t>(sy) * Screen::kWidth + sx] = colour;
            }
        }
        x += w;
    }
}

}  // namespace edison
