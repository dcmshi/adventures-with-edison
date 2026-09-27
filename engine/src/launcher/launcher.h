#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "artech/anims.h"
#include "artech/scripts.h"
#include "artech/context.h"

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

private:
    void opening();                                        // f05_0034
    Choice menu();                                         // f04_0172
    int pollButton();                                      // f04_0024
    bool runScripts(ScriptEvent& event);
    void fm(uint16_t sound);

    GameContext ctx_;
    Anims anims_;
    Scripts scripts_;
    Options options_;
    std::array<int16_t, 4 * 112> introPath_{};  // DS:0150, {x, y, w, h} until x < 0
    int introEntries_ = 0;

    std::array<int, 5> countdown_{};  // DS:55EE, decremented at 10 Hz
    bool introFrameDue_ = false;
};

}  // namespace edison
