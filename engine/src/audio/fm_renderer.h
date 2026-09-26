#pragma once

#include <cstdint>

#include "audio/artech_fm_driver.h"
#include "audio/opl_synth.h"

namespace edison {

// Drives the FM sound driver at the game's timer rate and turns its OPL
// register writes into audio. The games call UPDATE_ADLIB from a 13 ms
// multimedia timer (timeSetEvent), i.e. ~76.9 ticks per second.
class FmRenderer {
public:
    static constexpr uint32_t kTickMicroseconds = 13000;

    FmRenderer(ArtechFmDriver& driver, uint32_t sampleRate);

    // Renders interleaved stereo frames, ticking the driver on schedule.
    void render(int16_t* stereo, uint32_t frames);

    uint64_t ticks() const { return ticks_; }
    OplSynth& synth() { return synth_; }

private:
    ArtechFmDriver& driver_;
    OplSynth synth_;
    uint64_t framesUntilTick_ = 0;
    uint64_t tickRemainder_ = 0;  // fractional frames, in units of 1/1e6
    uint64_t ticks_ = 0;
};

}  // namespace edison
