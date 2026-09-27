#include "mystery/mystery.h"

#include <algorithm>

#include "formats/ne_file.h"

namespace edison {

bool Mystery::load(const Options& options, std::string* error) {
    options_ = options;
    // The CD root holds the MYSTERY folder of speech files, next to DSK3.
    cdRoot_ = options.cdDir + "/..";
    if (!ctx_.archive.open(options.cdDir + "/MYSTERY.D01", error)) return false;
    NeFile exe;
    if (!exe.load(options.cdDir + "/MALL.EXE", error)) return false;
    data_ = exe.segment(exe.segmentCount());  // DGROUP, the last segment
    if (data_.size() < 0x2000 || dataString(0x10) != "Mystery at the Museums") {
        if (error) *error = "MALL.EXE: unexpected data segment";
        return false;
    }
    sine_ = exe.segment(51);
    puzzles_ = exe.segment(62);
    if (sine_.size() < 0x1000 || puzzles_.size() < 0x198) {
        if (error) *error = "MALL.EXE: unexpected segments 51/62";
        return false;
    }
    if (options.music) ctx_.platform.setFmDriver(options.cdDir + "/MADLIB.DLL");
    font_ = &ctx_.font(0x0100);  // the default font (set up in f01_00f6's init)
    loadColourTables();
    return true;
}

std::string Mystery::dataString(uint16_t offset) const {
    std::string s;
    for (size_t i = offset; i < data_.size() && data_[i]; ++i) s += static_cast<char>(data_[i]);
    return s;
}

uint16_t Mystery::dataWord(uint16_t offset) const {
    return offset + 1u < data_.size() ? static_cast<uint16_t>(data_[offset] | data_[offset + 1] << 8) : 0;
}

void Mystery::run() {
    // f02_00ba's loop: setup, then games until the player leaves.
    ctx_.startTimer();
    if (options_.startPuzzle >= 0) {
        player_.name = "Test";
        square_ = 0;
        for (;;) puzzle(options_.startPuzzle, std::max(options_.startLevel, 0));
    }
    int mode = 1;
    if (options_.startLevel >= 0) {
        player_.name = "Test";
        player_.level = static_cast<uint8_t>(std::min(options_.startLevel, 7));
        mode = -1;
    }
    for (;;) {
        if (mode >= 0 && setup(mode) == 1) break;
        const int r = play();
        if (r == 1) break;
        mode = r == 2 ? 0x0F : 0;
    }
    ctx_.blackout();
    ctx_.pump();
}

}  // namespace edison
