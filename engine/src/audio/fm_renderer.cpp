#include "audio/fm_renderer.h"

namespace edison {

FmRenderer::FmRenderer(ArtechFmDriver& driver, uint32_t sampleRate)
    : driver_(driver), synth_(sampleRate) {
    driver_.setOplWrite([this](uint8_t reg, uint8_t value) { synth_.write(reg, value); });
}

void FmRenderer::render(int16_t* stereo, uint32_t frames) {
    while (frames > 0) {
        if (framesUntilTick_ == 0) {
            driver_.update();
            ++ticks_;
            // Exact frames per tick, carrying the fractional part forward.
            const uint64_t scaled = static_cast<uint64_t>(synth_.sampleRate()) * kTickMicroseconds + tickRemainder_;
            framesUntilTick_ = scaled / 1000000;
            tickRemainder_ = scaled % 1000000;
        }
        const uint32_t chunk =
            static_cast<uint32_t>(framesUntilTick_ < frames ? framesUntilTick_ : frames);
        synth_.render(stereo, chunk);
        stereo += 2 * chunk;
        frames -= chunk;
        framesUntilTick_ -= chunk;
    }
}

}  // namespace edison
