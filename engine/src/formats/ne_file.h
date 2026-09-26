#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace edison {

// Minimal reader for 16-bit Windows NE executables: just enough to pull
// segment contents out of the game's DLLs.
class NeFile {
public:
    bool load(const std::string& path, std::string* error = nullptr);

    int segmentCount() const { return static_cast<int>(segments_.size()); }

    // Raw file bytes of a segment (1-based index, as NE numbers them).
    std::vector<uint8_t> segment(int index) const;

    // The whole file (e.g. for the CodeView debug info appended to it).
    const std::vector<uint8_t>& bytes() const { return data_; }

private:
    struct Segment {
        uint32_t offset;
        uint32_t size;
    };

    std::vector<uint8_t> data_;
    std::vector<Segment> segments_;
};

}  // namespace edison
