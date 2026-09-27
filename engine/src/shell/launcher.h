#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "shell/anims.h"
#include "shell/scripts.h"
#include "shell/shell_context.h"

namespace edison {

// The Adventures with Edison launcher (EDISON.EXE): the opening and the
// menu that picks one of the three games. See docs/GAME.md.
class Launcher {
public:
    enum Choice { kQuit = 0, kRockAndBach = 1, kWildScience = 2, kMystery = 3 };

    struct Options {
        std::string cdDir;         // the CD's DSK3 folder (EDISON.EXE, SHELL.D01)
        bool skipOpening = false;  // -O
        bool music = true;         // not -A
    };

    explicit Launcher(Platform& platform) : ctx_(platform), anims_(ctx_), scripts_(ctx_, anims_) {}

    bool load(const Options& options, std::string* error);
    // Runs the opening (unless skipped) and the menu; returns the choice.
    Choice run();

    // Thrown out of pump() when the window is closed.
    struct Closed {};

private:
    // The library timer (CARTDLL): a 13 ms tick drives callbacks by rate.
    struct Periodic {
        uint32_t rate = 0;  // Hz; 0 = unregistered
        uint32_t acc = 0;
        std::function<void()> fn;
    };

    void pump();
    void advanceTimer();
    void setPeriodic(int slot, uint32_t rate, std::function<void()> fn);

    void opening();                                        // f05_0034
    Choice menu();                                         // f04_0172
    int pollButton();                                      // f04_0024
    bool runScripts(ScriptEvent& event);
    void blackout();                                       // f11_0000
    void showFullScreen(uint16_t bitmap, int screen);      // show_fscreen
    void setDisplayPalette(int screen);                    // f11_00be
    void drawLogo(int screen, int x, int y, uint16_t id);  // show_Clogo
    void fm(uint16_t sound);

    ShellContext ctx_;
    Anims anims_;
    Scripts scripts_;
    Options options_;
    Palette displayPalette_{};
    std::array<int16_t, 4 * 112> introPath_{};  // DS:0150, {x, y, w, h} until x < 0
    int introEntries_ = 0;

    uint64_t timerTicks_ = 0;
    std::array<Periodic, 4> periodic_{};
    std::array<int, 5> countdown_{};  // DS:55EE, decremented at 10 Hz
    bool introFrameDue_ = false;
};

}  // namespace edison
