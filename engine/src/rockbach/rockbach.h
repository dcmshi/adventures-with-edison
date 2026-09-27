#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "artech/game.h"

namespace edison {

// Rock and Bach Studio (WINMAIN.EXE). See docs/ROCKBACH.md for the map of
// the original; functions here name the one they port (fSS_OOOO).
class RockBach : public ArtechGame {
public:
    struct Options {
        std::string cdDir;              // the CD's DSK3 folder (WINMAIN.EXE, RB.D01, ADLIB*.DLL)
        bool music = true;              // not -A
        std::string saveDir = "save";   // user.yyy, ed.yyy and the player's files
        int startActivity = -1;         // for testing: go straight to a hallway result (2-9)
    };

    explicit RockBach(Platform& platform) : ArtechGame(platform) {}

    bool load(const Options& options, std::string* error);
    // Plays until the player leaves (the original then runs "edison.exe -O").
    void run();

private:
    // --- the library's screen helpers as the game uses them (segment 19) ---
    void blackout();                                            // f19_0000
    void show(int screen);                                      // f19_0082: palette and pixels to the display
    void backdrop(uint16_t id) { ctx_.showFullScreen(id, 2); }  // f37_0d48(id, 2)
    // f19_0116: palette entries [first, first + count) of the display.
    void setColours(const std::vector<Rgb>& colours, int first);
    void sound(uint16_t id);                                    // f27_020e: <CD>\RB\<name>.wav
    void setDriver(int driver);                                 // f02_006e: -1 none, 0-4 ADLIB, ADLIB1-4

    // --- the intro ---
    void corelPresents();                                       // f25_0016
    void logo();                                                // f05_04d8

    // --- the hallway (segment 24) ---
    // Returns the hot spot clicked (the colour of mask 1006 under the
    // mouse): 1 leave, 2-4 and 6-9 an activity.
    int hallway(bool again);                                    // f24_1d4a
    void activity(int which);                                   // f33_0422's switch

    // --- data from WINMAIN.EXE's data segment (read at run time) ---
    std::string dataString(uint16_t offset) const;
    uint16_t dataWord(uint16_t offset) const;

    Options options_;
    std::string cdRoot_;
    std::vector<uint8_t> data_;
    int driver_ = -1;  // [83D0]
};

}  // namespace edison
