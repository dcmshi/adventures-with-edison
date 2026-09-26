#include "formats/ne_file.h"

#include <fstream>
#include <iterator>

namespace edison {

namespace {

uint16_t u16(const std::vector<uint8_t>& d, size_t at) {
    return static_cast<uint16_t>(d[at] | (d[at + 1] << 8));
}

uint32_t u32(const std::vector<uint8_t>& d, size_t at) {
    return u16(d, at) | (static_cast<uint32_t>(u16(d, at + 2)) << 16);
}

bool fail(std::string* error, const std::string& message) {
    if (error) *error = message;
    return false;
}

}  // namespace

bool NeFile::load(const std::string& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return fail(error, "cannot open " + path);
    data_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    segments_.clear();

    if (data_.size() < 0x40 || data_[0] != 'M' || data_[1] != 'Z')
        return fail(error, path + ": not an MZ executable");
    const uint32_t ne = u32(data_, 0x3C);
    if (ne + 0x40 > data_.size() || data_[ne] != 'N' || data_[ne + 1] != 'E')
        return fail(error, path + ": not an NE executable");

    const uint16_t count = u16(data_, ne + 0x1C);
    const uint16_t table = u16(data_, ne + 0x22);
    const uint16_t align = u16(data_, ne + 0x32);
    for (uint16_t i = 0; i < count; ++i) {
        const size_t entry = ne + table + i * 8u;
        if (entry + 8 > data_.size()) return fail(error, path + ": truncated segment table");
        const uint32_t offset = static_cast<uint32_t>(u16(data_, entry)) << align;
        uint32_t size = u16(data_, entry + 2);
        if (size == 0 && offset != 0) size = 0x10000;
        if (offset + size > data_.size()) return fail(error, path + ": segment past end of file");
        segments_.push_back({offset, size});
    }
    return true;
}

std::vector<uint8_t> NeFile::segment(int index) const {
    if (index < 1 || index > segmentCount()) return {};
    const Segment& s = segments_[index - 1];
    return std::vector<uint8_t>(data_.begin() + s.offset, data_.begin() + s.offset + s.size);
}

}  // namespace edison
