#pragma once

#include <array>
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
    // The library's five countdowns (EDISON DS:55EE, MALL DS:92B2),
    // decremented at 10 Hz while above zero.
    std::array<int, 5> countdown{};

    // Starts the timer with the library's own callbacks (the countdowns).
    void startTimer();

    // One pass of the main loop, where the original pumped Windows
    // messages: handles input, runs the timer, shows screen 1.
    void pump();
    // A pass of a wait that only watches a countdown and takes no messages
    // (MALL f09_0dc4's, f09_10fe's): the timer runs and screen 1 is shown,
    // but the button isn't seen to go down or up (held) till the next pump.
    void spin();
    // The left button as the game's window procedure last saw it (MALL's
    // [739E] bit 0: set on WM_LBUTTONDOWN, cleared on WM_LBUTTONUP).
    bool held = false;

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
// A line to stderr and, when EDISON_LOG names a file, to that file.
void logLine(const std::string& message);

}  // namespace edison
