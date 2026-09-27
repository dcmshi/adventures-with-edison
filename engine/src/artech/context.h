#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "artech/font.h"
#include "artech/platform.h"
#include "artech/screens.h"
#include "artech/timer.h"
#include "formats/archive.h"
#include "formats/bitmap.h"

namespace edison {

// The Artech library state a game runs on: its resource archive, the
// drawing surfaces, the display palette, the timer and the game clock.
struct GameContext {
    explicit GameContext(Platform& p) : platform(p) {}

    // Thrown out of pump() when the window is closed.
    struct Closed {};

    Platform& platform;
    Archive archive;
    Screens screens;
    Timer timer;
    Palette displayPalette{};
    // The library's game tick counter (DS:0000), advanced at 16 Hz.
    uint32_t gameTicks = 0;

    // One pass of the main loop, where the original pumped Windows
    // messages: handles input, runs the timer, shows screen 1.
    void pump();

    // Archive bitmap by id, decoded once. Missing ids give an empty bitmap.
    const Bitmap& bitmap(uint16_t id);
    const Font& font(uint16_t id);
    bool read(uint16_t id, std::vector<uint8_t>& out);
    void playWav(uint16_t id);

    // Drawing helpers from the library.
    void blackout();                                       // screen 2 cleared and shown, palette black
    void showFullScreen(uint16_t bitmap, int screen);      // show_fscreen: bitmap and its palette
    void setDisplayPalette(int screen);                    // colours 0 and 255 stay black and white
    void drawLogo(int screen, int x, int y, uint16_t id);  // show_Clogo: colour 0 transparent

    // Anims that have run to the end (the 256-entry table at DS:6DF2).
    void markFinished(uint16_t anim);
    bool finished(uint16_t anim) const;
    void clearFinished(uint16_t anim);
    void resetFinished() { finishedAnims_.assign(256, -1); }

private:
    std::map<uint16_t, std::unique_ptr<Bitmap>> bitmaps_;
    std::map<uint16_t, std::unique_ptr<Font>> fonts_;
    std::vector<int> finishedAnims_ = std::vector<int>(256, -1);
};

// Logs a problem once per distinct message (to stderr).
void warnOnce(const std::string& message);

}  // namespace edison
