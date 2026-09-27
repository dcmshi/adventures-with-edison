#pragma once

#include <cstdint>
#include <vector>

#include "shell/screens.h"

namespace edison {

// What the game code needs from the host (window, input, sound, clock).
// The launcher is written like the original: straight-line code that polls
// in loops, calling pump() where the original pumped Windows messages.
class Platform {
public:
    virtual ~Platform() = default;

    // Handles window events; returns false when the user closed the window.
    virtual bool pumpEvents() = 0;
    // Milliseconds since start (monotonic).
    virtual uint64_t milliseconds() = 0;
    // Shows a screen with the given palette.
    virtual void present(const Screen& screen, const Palette& palette) = 0;

    // Left click latched since the last call (game coordinates 640x400).
    virtual bool takeClick(int* x, int* y) = 0;
    virtual bool escapeHeld() = 0;

    // Sound: a RIFF WAV image (replaces the one playing), FM driver calls.
    virtual void playWav(const std::vector<uint8_t>& wav) = 0;
    virtual void sendFm(uint16_t sound) = 0;
};

}  // namespace edison
