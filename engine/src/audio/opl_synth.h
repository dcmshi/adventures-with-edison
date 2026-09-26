#pragma once

#include <cstdint>
#include <memory>

namespace edison {

// OPL2 synthesiser backed by Nuked-OPL3 (run in OPL2-compatible mode).
class OplSynth {
public:
    explicit OplSynth(uint32_t sampleRate);
    ~OplSynth();
    OplSynth(const OplSynth&) = delete;
    OplSynth& operator=(const OplSynth&) = delete;

    void reset();
    void write(uint8_t reg, uint8_t value);

    // Renders interleaved stereo 16-bit frames.
    void render(int16_t* stereo, uint32_t frames);

    uint32_t sampleRate() const { return sampleRate_; }

private:
    struct Chip;
    std::unique_ptr<Chip> chip_;
    uint32_t sampleRate_;
};

}  // namespace edison
