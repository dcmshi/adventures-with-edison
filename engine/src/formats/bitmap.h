#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace edison {

struct Rgb {
    uint8_t r, g, b;
};
using Palette = std::array<Rgb, 256>;

// An 8-bit indexed image, rows top-down.
struct Bitmap {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;
    Palette palette{};

    uint8_t at(int x, int y) const { return pixels[static_cast<size_t>(y) * width + x]; }
};

// Decodes the uncompressed 8-bit Windows BMPs the archives hold.
bool decodeBmp(const std::vector<uint8_t>& file, Bitmap& out, std::string* error);

}  // namespace edison
