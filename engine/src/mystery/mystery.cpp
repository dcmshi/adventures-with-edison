#include "mystery/mystery.h"

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
    if (options.music) ctx_.platform.setFmDriver(options.cdDir + "/MADLIB.DLL");
    font_ = &ctx_.font(0x0100);  // the default font (set up in f01_00f6's init)
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
    ctx_.startTimer();
    title();
    setupScreen();
    scene(0);
    // Not ported further yet: the name entry and the rest of the setup
    // come next. Hold the last frame briefly, then return.
    waitCountdown(20);
    ctx_.blackout();
    ctx_.pump();
}

}  // namespace edison
