#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "artech/screens.h"

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
    // Mouse position (game coordinates) and whether the left button is down.
    virtual void mouse(int* x, int* y, bool* down) = 0;
    virtual bool escapeHeld() = 0;
    // Next key typed, as Windows would give it: printable ASCII, or
    // kBackspace, kTab, kEnter, kEscape; 0 when there's none.
    virtual int takeKey() = 0;
    enum Key { kBackspace = 8, kTab = 9, kEnter = 13, kEscape = 27, kLeft = 0x100, kRight, kUp, kDown };

    // Sound: a RIFF WAV image (replaces the one playing); the FM driver DLL
    // a game uses (CADLIB, MADLIB, ...) and its SENDSND.
    virtual void playWav(const std::vector<uint8_t>& wav) = 0;
    // True while a WAV is still playing (the original's [73B6]).
    virtual bool wavPlaying() { return false; }
    virtual void setFmDriver(const std::string& dllPath) = 0;
    virtual void sendFm(uint16_t sound) = 0;
};

}  // namespace edison
