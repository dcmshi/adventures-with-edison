#include "audio/opl_synth.h"

extern "C" {
#include "opl3.h"
}

namespace edison {

struct OplSynth::Chip {
    opl3_chip state;
};

OplSynth::OplSynth(uint32_t sampleRate) : chip_(std::make_unique<Chip>()), sampleRate_(sampleRate) {
    reset();
}

OplSynth::~OplSynth() = default;

void OplSynth::reset() { OPL3_Reset(&chip_->state, sampleRate_); }

void OplSynth::write(uint8_t reg, uint8_t value) {
    // Bank 0 only: with the OPL3 "NEW" bit clear the chip behaves as an OPL2.
    OPL3_WriteRegBuffered(&chip_->state, reg, value);
}

void OplSynth::render(int16_t* stereo, uint32_t frames) {
    OPL3_GenerateStream(&chip_->state, stereo, frames);
}

}  // namespace edison
