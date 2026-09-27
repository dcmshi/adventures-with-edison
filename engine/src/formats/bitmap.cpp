#include "formats/bitmap.h"

#include <algorithm>
#include <cstdlib>

namespace edison {
namespace {

uint32_t u32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24; }
uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }

bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

}  // namespace

bool decodeBmp(const std::vector<uint8_t>& file, Bitmap& out, std::string* error) {
    if (file.size() < 54 || file[0] != 'B' || file[1] != 'M') return fail(error, "not a BMP");
    const uint8_t* d = file.data();
    const uint32_t dataOffset = u32(d + 10);
    const uint32_t headerSize = u32(d + 14);
    const int32_t width = static_cast<int32_t>(u32(d + 18));
    const int32_t rawHeight = static_cast<int32_t>(u32(d + 22));
    if (u16(d + 28) != 8) return fail(error, "not an 8-bit BMP");
    if (u32(d + 30) != 0) return fail(error, "compressed BMP");
    // 0x0 bitmaps occur: palette carriers (RB.D01 group 32).
    if (width < 0) return fail(error, "bad BMP size");
    uint32_t colours = u32(d + 46);
    if (colours == 0 || colours > 256) colours = 256;

    const bool bottomUp = rawHeight > 0;
    const int height = std::abs(rawHeight);
    const size_t stride = (static_cast<size_t>(width) + 3) & ~size_t{3};
    const size_t paletteAt = 14 + headerSize;
    if (paletteAt + 4 * colours > file.size() || dataOffset + stride * height > file.size())
        return fail(error, "truncated BMP");

    out.width = width;
    out.height = height;
    out.palette = {};
    for (uint32_t i = 0; i < colours; ++i) {
        const uint8_t* q = d + paletteAt + 4 * i;
        out.palette[i] = Rgb{q[2], q[1], q[0]};
    }
    out.pixels.resize(static_cast<size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
        const uint8_t* row = d + dataOffset + stride * (bottomUp ? height - 1 - y : y);
        std::copy(row, row + width, out.pixels.begin() + static_cast<size_t>(y) * width);
    }
    return true;
}

}  // namespace edison
