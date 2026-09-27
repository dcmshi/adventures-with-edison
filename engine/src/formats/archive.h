#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace edison {

// Artech resource archive (SHELL.D01, RB.D01, MYSTERY.D01, GRAFX.DAT):
//     [ASCII title] 0x1A, u16 count,
//     count x { u16 id, u32 offset, u32 packed_size, u32 unpacked_size },
//     entries compressed with PKWARE DCL implode (a few are stored raw).
// The high byte of an id is the resource group (see docs/FORMATS.md).
class Archive {
public:
    bool open(const std::string& path, std::string* error);

    bool has(uint16_t id) const { return entries_.count(id) != 0; }
    // Decompressed contents of an entry.
    bool read(uint16_t id, std::vector<uint8_t>& out, std::string* error) const;
    std::vector<uint16_t> ids() const;

private:
    struct Entry {
        uint32_t offset, packed, unpacked;
    };
    std::vector<uint8_t> data_;
    std::map<uint16_t, Entry> entries_;
};

}  // namespace edison
