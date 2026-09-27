#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "artech/context.h"
#include "mystery/panels.h"

namespace edison {

// Mystery at the Museums (MALL.EXE). See docs/MYSTERY.md for the map of
// the original; functions here name the one they port (fSS_OOOO).
class Mystery {
public:
    struct Options {
        std::string cdDir;  // the CD's DSK3 folder (MALL.EXE, MYSTERY.D01, MADLIB.DLL)
        bool music = true;  // not -A
        int startLevel = -1;  // for testing: skip setup and play this level
        int startPuzzle = -1; // for testing: play only this puzzle (at difficulty startLevel)
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
    void drawVia3(int x, int y, int w, int h, Draw draw) {
        if (current_ == 1) {
            copyArea(1, 3, x, y, w, h);
            draw(3);
            copyArea(3, 1, x, y, w, h);
        } else {
            draw(current_);
        }
    }

    void drawLogo(int x, int y, uint16_t id);                   // f06_120c (clamped to the screen)
    // f06_19fa at 1:1: each non-transparent pixel's colour shifted by `add`.
    void drawShifted(int x, int y, uint16_t id, int add);
    void fill(int x, int y, int w, int h, uint8_t colour);      // f06_17c8
    void text(int x, int y, const std::string& s, int colour);  // f06_15b8
    void copyArea(int src, int dst, int x, int y, int w, int h) { ctx_.screens.copyArea(src, dst, x, y, w, h); }
    // duplicate_area: a rectangle to another position (possibly another screen).
    void duplicateArea(int src, int dst, int sx, int sy, int w, int h, int dx, int dy);
    int saveArea(int x, int y, int w, int h);                   // f06_1424 (a handle)
    void restoreArea(int handle);                               // f06_2924
    // recolour_area: pixels of colour `from` become `to` (f06_16d8).
    void recolour(int x, int y, int w, int h, uint8_t from, uint8_t to);
    // Speech bubble (f06_2494): corners 210C-210F, tail 2106/2107 on
    // `side` 0-3. Returns a saved-area handle when `save` is set.
    int speechBox(int x, int y, const std::vector<std::string>& lines, int side, bool save);
    void computeUiColours();                                    // f06_01f6
    void sound(uint16_t id);                                    // f06_2da8: \MYSTERY\<name>.wav
    void music(uint16_t id);                                    // f02_0000: MADLIB SENDSND
    void waitCountdown(int tenths);                             // [92B2] = n; wait for 0
    void clearInput();                                          // f06_2ccc
    bool anyInput();                                            // a click or key since the last check

    // --- segment 8: setup (setup.cpp) ---
    void title();                                               // f08_2284
    void setupScreen();                                         // start of f08_232c
    void scene(int part);                                       // f08_06f8
    bool talk(int frames);                                      // g08_0cf8 (true if cut short)
    void nameEntry();                                           // g08_0380
    void loadColourTables();
    void applyColours(int screen, bool toDisplay);              // f09_0b88
    void setColourGroup(int group);                             // f06_1c42
    std::vector<std::string> dataLines(uint16_t table) const;
    int yesNo(int x, int y, int w, int h);                      // f06_2976: 0 yes, 1 no
    bool askChangeLooks();                                      // f08_14d2
    void letsDoIt();                                            // f08_157e
    void customizer();                                          // f08_0f68
    void pickLevel();                                           // f08_1d2a
    void runOff();                                              // f08_1264
    int setup(int mode);                                        // f08_232c

    // --- segments 9-10: the game (game.cpp, floor.cpp) ---
    // f09_1dd8: returns 0 to play another game at the same level, 1 to
    // leave, 2 to pick a new level.
    int play();
    void newBoard();                                            // the new-game part of f09_1dd8
    void show(int screen);                                      // f04_005c: palette and pixels to the display
    void backdrop(uint16_t id);                                 // f04_0000(1) + f32_0d48(id, 2)
    void drawOpaque(int x, int y, uint16_t id);                 // f06_1326 (show_logo, clamped)
    void line(int x0, int y0, int x1, int y1, uint8_t colour);  // f06_1af8
    void clock(bool force);                                     // f09_0ab2
    void clockHand(int cx, int cy, uint16_t hand, int value, uint8_t colour);  // f05_00a0
    void digitalTime(int x, int y, int w, int h, int seconds);  // f06_0fc0
    void number(int x, int y, int w, int h, long value);        // f06_1150
    void drawObjects(int found);                                // f09_08dc
    bool allFound() const;                                      // f09_0a1e
    void mapTalk(int side, int frames);                         // f09_0dc4
    void smittyTalk();                                          // f09_10fe
    void smittySays(int mode, int result);                      // f09_1212
    void entrance(bool first, int found);                       // f09_15f8
    void museumList();                                          // f09_0f8a
    void smittyShow();                                          // f09_002e
    void mapView();                                             // f09_01ee
    bool askQuit();                                             // f09_0592
    void quitPressed();                                         // f09_0680
    // f05_0266; with a text colour it also draws the button ("HELP").
    void helpPanel(int x, int y, int w, int h, int textColour = -1, int fillColour = 0);
    void help(uint16_t text);                                   // f05_0320
    void messageBox(const std::vector<std::string>& lines);    // f06_0f34 / f06_09ee (style 0)
    bool waitOrClick(int tenths);                               // [92B2] = n; wait for 0 or a click
    int floor();                                                // f10_0708
    void floorRegions();                                        // f10_008e
    void floorRooms(bool masks);                                // f10_0328
    void squareName(int square);                                // f10_0000
    void director();                                            // f10_0638
    bool puzzle(int kind, int level);                           // f10_0708's switch over the 16 games
    // f06_1d76: the end of a game. Edison pops up with the verdict; the
    // points go to the score. Returns `won`.
    bool puzzleResult(bool won, int points, int mode, int seconds);
    void intBox(int x, int y, int w, int h, int value);         // f06_1090

    // --- segments 12-14: the picture puzzles (pictures.cpp) ---
    bool arrowPuzzle(int level);                                // g12_1c60
    // g12_1438 / g13_058a: `size` is a row of DS:1F2C or DS:242A (width,
    // height, and the scale, 256 = 1:1).
    void loadPicture(int index, const int size[4]);
    bool slidePuzzle(int level);                                // g14_028a
    bool switchPuzzle(int level);                               // g13_0d44

    // --- the smaller puzzles (small.cpp) ---
    bool circuitAnalyzer(int level);                            // g16_089c
    bool binaryLights(int level);                               // g17_1256
    bool codes(int level);                                      // g20_1474
    bool concentration(int level);                              // g15_1142
    // f19_16a2: as puzzle 4, or (asPuzzle false) the quiz after a won game.
    bool questionPeriod(int level, bool asPuzzle);
    bool dig(int level);                                        // g21_185c
    // The part of the picture puzzles' main loops they share; `help` is
    // the help text. Returns the result from puzzleResult.
    bool pictureLoop(uint16_t help);
    void slideTo(int cell);                                     // g14_0000 (cell = row * 6 + col)
    void addBoardPanel();                                       // g13_0458 + panel DS:27BE
    void drawBoard();                                           // g12_03b2
    void drawSolution();                                        // g12_06b8
    void redrawLine(int row, int col);                          // g12_04b6
    void checkPicture();                                        // g12_024a
    void shove(int arrow);                                      // g12_0846
    void addArrows();                                           // g12_0f8c + panel DS:2394
    void puzzleClock();                                         // g12_13ce
    void countBonus();                                          // g12_12d6
    void pictureGizmo();                                        // g12_00d6
    void pictureShow();                                         // g12_0168
    void addPicturePanels(uint16_t help);                       // DS:223A, 21E6, 220A and help
    void endOfGame();                                           // segments 22-24 (placeholder)

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
    // The player record (DS:B465): name and Edison's colour choices.
    struct Player {
        std::string name;
        uint8_t colours[4] = {0, 0, 0, 0};  // hair, shirt, trousers, shoes (B46F-B472)
        uint8_t level = 0;                  // B46E
    } player_;
    std::array<std::vector<Rgb>, 4> edisonColours_;
    Panels panels_;
    bool highScoresRequested_ = false;
    int nextHandle_ = 1;

    // --- the game in progress ---
    std::vector<uint8_t> sine_;     // segment 51: a quarter sine wave, 2048 words
    std::vector<uint8_t> puzzles_;  // segment 62: each level's puzzles
    struct Square {                 // DS:931C
        uint8_t puzzle = 0xFF, level = 0xFF, object = 0xFF;
        uint8_t state = 0;          // 0 unused, 1 open, 2 solved, 3 failed
    };
    std::array<Square, 29> squares_{};
    struct Object {                 // DS:C500
        uint8_t museum = 0xFF, square = 0xFF, found = 0;
    };
    std::array<Object, 16> objects_{};
    int objectCount_ = 0;           // [B770]
    int timeLeft_ = 0;              // [9314], seconds
    int timeTotal_ = 0;             // [9312]
    int clockShown_ = -1;           // [0EB4]
    uint16_t hourStart_ = 0;        // [0CA9], the hour hand's start angle
    uint16_t hourPeriod_ = 0;       // [0CAD]
    int outcome_ = 0;               // [C12E]: 1 out of time, 2 all found
    long score_ = 0;                // [C12A]
    bool savedGame_ = false;        // [B76B]
    bool lastOne_ = false;          // [B054]: one object left
    bool greeted_ = false;          // [0C9E]
    int square_ = 0;                // [C76C], the square being played
    int bubble_ = 0;                // [81CE]
    bool modal_ = false;            // [C23E]
    int menu_ = 0;                  // [0DC4]: 1 quit, 2 new game
    bool go_ = false;               // [0E74]
    bool wantMap_ = false;          // [0CDC]
    bool wantShow_ = false;         // [0CB8]
    bool helpPressed_ = false;      // [0096]
    int listState_ = 0;             // [0EBE]
    int listTop_ = 0;               // [0EC8]
    bool floorBack_ = false;        // [18C8]
    bool floorDirector_ = false;    // [18EC]
    // DS:B7CE: facts learned in Concentration (per theme), asked about in
    // the Question and Answer Period.
    bool learned_[4][10] = {};

    // --- the picture puzzles ---
    struct Picture {
        int divisions = 2;          // [9456]: the picture is N x N tiles
        int size = 2;               // [B38A]: the board is size x size
        int tileW = 0, tileH = 0;   // [C450], [B80A]
        int left = 0, top = 0;      // [B808], [C3F0]
        int arrowsX = 0, arrowsY = 0, arrowsW = 0, arrowsH = 0;  // DS:2394
        int arrowDX = 0, arrowDY = 0;  // [944C], [944E]
        int board[8][8] = {};       // DS:B3DE; kBlank or tile numbers
        int mode = -1;              // [C650]: 0 whole, 1 arrows, 2 slide
        int index = 0;              // [C5B4]
        bool solved = false;        // [C64C]
        int state = 0;              // [21E4]: 1 given up, 2 solved
        bool gizmo = false;         // [2208]
        int timeLeft = 0;           // [C138]
        int timeLimit = 0;          // [930E]
        int shownTime = -1;         // [23AE]
        bool timerIdle = true;      // [B3BB]
        int moves = 0;              // [C4FE]
        int points = 0;             // [1F2A]
        int used = 0;               // [B794]
        int blank = 0;              // [92BC]: the slide puzzle's blank, row * 6 + col
        int picked = 0;             // [C64A]: the switch puzzle's first tile, row * 8 + col
        int pickedCount = 0;        // [C65A]
    } pic_;
    static constexpr int kBlank = 0x1000;
};

}  // namespace edison
