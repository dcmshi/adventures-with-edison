#pragma once

#include <cstdint>

#include "audio/artech_fm_driver.h"
#include "audio/opl_synth.h"

namespace edison {

// Drives the FM sound driver at the game's timer rate and turns its OPL
// register writes into audio.
//
// The games' timer (the *ARTDLL.DLL TIMERCALLBACK) fires every 13 ms and
// runs periodic callbacks through an accumulator: each adds its rate in Hz
// and fires when the sum reaches 1000 / 13 = 76 (integer), subtracting 76.
// UPDATE_ADLIB is registered at 72 Hz, so it runs on 72 of every 76 timer
// ticks: ~72.9 updates per second, not evenly spaced.
class FmRenderer {
public:
    static constexpr uint32_t kTimerMicroseconds = 13000;
    static constexpr uint32_t kThreshold = 1000 / 13;  // 76
    static constexpr uint32_t kMusicRate = 72;

    FmRenderer(ArtechFmDriver& driver, uint32_t sampleRate);

    // Renders interleaved stereo frames, ticking the driver on schedule.
    void render(int16_t* stereo, uint32_t frames);

    uint64_t ticks() const { return ticks_; }
    OplSynth& synth() { return synth_; }

private:
    ArtechFmDriver& driver_;
    OplSynth synth_;
    uint64_t framesUntilTimer_ = 0;
    uint64_t timerRemainder_ = 0;  // fractional frames, in units of 1/1e6
    uint32_t accumulator_ = 0;
    uint64_t ticks_ = 0;  // driver updates
};

}  // namespace edison
