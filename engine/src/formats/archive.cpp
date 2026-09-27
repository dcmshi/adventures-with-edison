#include "formats/archive.h"

#include <cstdio>
#include <fstream>
#include <iterator>

#include "formats/dcl.h"

namespace edison {
namespace {

uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }
uint32_t u32(const uint8_t* p) { return u16(p) | static_cast<uint32_t>(u16(p + 2)) << 16; }

}  // namespace

bool Archive::open(const std::string& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot open " + path;
        return false;
    }
    data_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    entries_.clear();
    size_t pos = 0;
    while (pos < data_.size() && data_[pos] != 0x1A) ++pos;
    if (pos + 3 > data_.size()) {
        if (error) *error = path + ": not an archive";
        return false;
    }
    const uint16_t count = u16(&data_[pos + 1]);
    pos += 3;
    if (pos + 14u * count > data_.size()) {
        if (error) *error = path + ": truncated directory";
        return false;
    }
    for (uint16_t i = 0; i < count; ++i, pos += 14) {
        const uint8_t* p = &data_[pos];
        const Entry e{u32(p + 2), u32(p + 6), u32(p + 10)};
        if (static_cast<uint64_t>(e.offset) + e.packed > data_.size()) {
            if (error) *error = path + ": entry past end of file";
            return false;
        }
        entries_[u16(p)] = e;
    }
    return true;
}

bool Archive::read(uint16_t id, std::vector<uint8_t>& out, std::string* error) const {
    out.clear();
    const auto it = entries_.find(id);
    if (it == entries_.end()) {
        if (error) {
            char buf[48];
            std::snprintf(buf, sizeof buf, "no archive entry %04x", id);
            *error = buf;
        }
        return false;
    }
    const Entry& e = it->second;
    const uint8_t* packed = data_.data() + e.offset;
    std::string dclError;
    if (!dclExplode(packed, e.packed, out, &dclError) || out.size() != e.unpacked) {
        // A few entries are stored uncompressed.
        if (e.packed != e.unpacked) {
            if (error) *error = "entry decompression failed: " + dclError;
            return false;
        }
        out.assign(packed, packed + e.packed);
    }
    return true;
}

std::vector<uint16_t> Archive::ids() const {
    std::vector<uint16_t> out;
    for (const auto& [id, e] : entries_) out.push_back(id);
    return out;
}

}  // namespace edison
