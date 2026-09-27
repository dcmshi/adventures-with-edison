#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "artech/context.h"

namespace edison {

// Mystery at the Museums (MALL.EXE). See docs/MYSTERY.md for the map of
// the original; functions here name the one they port (fSS_OOOO).
class Mystery {
public:
    struct Options {
        std::string cdDir;  // the CD's DSK3 folder (MALL.EXE, MYSTERY.D01, MADLIB.DLL)
        bool music = true;  // not -A
    };

    explicit Mystery(Platform& platform) : ctx_(platform) {}

    bool load(const Options& options, std::string* error);
    // Plays until the player leaves (the original then runs "edison.exe -O").
    void run();

private:
    // --- segment 6: the game's drawing and UI helpers (ui.cpp) ---
    // Most helpers draw on the current screen; when that's the display (1)
    // they draw into screen 3 and copy the rectangle back to 1.
    void select(int screen) { current_ = screen; }
    int current() const { return current_; }
    template <class Draw>
    void drawVia3(int x, int y, int w, int h, Draw draw);

    void drawLogo(int x, int y, uint16_t id);                   // f06_120c (clamped to the screen)
    void fill(int x, int y, int w, int h, uint8_t colour);      // f06_17c8
    void text(int x, int y, const std::string& s, int colour);  // f06_15b8
    void copyArea(int src, int dst, int x, int y, int w, int h) { ctx_.screens.copyArea(src, dst, x, y, w, h); }
    // duplicate_area: a rectangle to another position (possibly another screen).
    void duplicateArea(int src, int dst, int sx, int sy, int w, int h, int dx, int dy);
    int saveArea(int x, int y, int w, int h);                   // f06_1424 (a handle)
    void restoreArea(int handle);                               // f06_2924
    // Speech bubble (f06_2494): corners 210C-210F, tail 2106/2107 on
    // `side` 0-3. Returns a saved-area handle when `save` is set.
    int speechBox(int x, int y, const std::vector<std::string>& lines, int side, bool save);
    void computeUiColours();                                    // f06_01f6
    void sound(uint16_t id);                                    // f06_2da8: \MYSTERY\<name>.wav
    void music(uint16_t id);                                    // f02_0000: MADLIB SENDSND
    void waitCountdown(int tenths);                             // [92B2] = n; wait for 0

    // --- segment 8: setup (setup.cpp) ---
    void setupScreen();                                         // start of f08_232c
    void scene(int part);                                       // f08_06f8

    // --- data from MALL.EXE's data segment (read at run time) ---
    std::string dataString(uint16_t offset) const;
    uint16_t dataWord(uint16_t offset) const;

    GameContext ctx_;
    Options options_;
    std::string cdRoot_;
    std::vector<uint8_t> data_;
    int current_ = 1;
    const Font* font_ = nullptr;
    // UI colours, nearest palette matches of the 15 colours at DS:00BE
    // (the original's [B3BE + 2k]).
    std::array<uint8_t, 15> ui_{};
    struct SavedArea {
        int x, y, w, h;
        std::vector<uint8_t> pixels;
    };
    std::map<int, SavedArea> saved_;
    int nextHandle_ = 1;
};

}  // namespace edison
