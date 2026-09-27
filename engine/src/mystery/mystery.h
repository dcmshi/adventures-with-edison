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
        std::string saveDir = "save";  // MYSTERY.HS and the players' files
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
    // A filled polygon on the current screen (f32_2e46's fill); `edges`
    // also draws its outline in the same colour (the vertices included).
    void fillPolygon(const std::vector<std::pair<int, int>>& points, uint8_t colour, bool edges = false);
    void drawCentred(int x, int y, uint16_t id);                // f06_189a at 1:1
    void drawScaledCentred(int x, int y, int scaleX, int scaleY, uint16_t id);  // f06_189a
    void frame(int x, int y, int w, int h, uint8_t colour);     // f06_08c0: a rectangle's outline
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
    std::string playerPath() const;                             // g08_0154: <name>.INF
    bool loadPlayer();                                          // g08_01ee (false: a new player)
    void savePlayer() const;                                    // f08_0000
    void loadLook();                                            // f08_11ea: MEDISON.COL
    void saveLook() const;                                      // f08_1102
    void saveGame();                                            // f08_1f6e
    void loadSavedGame();                                       // f08_2092
    void storeCustomLevel();                                    // f08_1e2a
    void useCustomLevel();                                      // f08_1eca
    bool playAgain();                                           // g08_21b6
    bool askSavedGame();                                        // g08_1600
    // g08_16c2 + g08_18be: the level panel (DS:0A24). Mode 0 is the custom
    // level's three choices, 1 the last level's two, 2 the eight levels and
    // "make a custom level". The choice ([09D2]) is 1-3 (modes 0-1), or 1 a
    // level and 2 a custom level (mode 2).
    void levelPanel(int mode, int x, int y);
    int waitChoice();                                           // until [09D2] is set
    void waitSpeech();                                          // until [73B6] (the WAV) is clear
    bool askCustomLevel();                                      // g08_1a4e
    bool askLastLevel();                                        // g08_1b6e
    void customLevelEditor(bool edit);                          // f11_19b4

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
    void redrawMap();                                           // g09_1c92
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
    bool whatComesNext(int level);                              // g26_19d8
    bool colourTransformation(int level);                       // g27_1032
    bool stackup(int level);                                    // g25_1f6a
    bool droppingSquares(int level);                            // g18_26da
    bool planetarium(int level);                                // g28_178e
    bool foldedCube(int level);                                 // g29_10c0
    bool ballSculpture(int level);                              // g30_136c
    void cubeGadget();                                          // g30_0ad2 (shared by 29 and 30)

    // --- segments 47-51: the 3D helpers the last three puzzles share (three.cpp) ---
    struct Point3 {
        int16_t x, y, z;  // y is the depth
    };
    void sinCos(uint16_t angle, int* sine, int* cosine) const;  // g51_1000 (Q15, a turn is 0x10000)
    // g50_0000: a rotation from three angles, nine Q15 words.
    void rotation(uint16_t a, uint16_t b, uint16_t c, int16_t m[9]) const;
    static void transform(std::vector<Point3>& pts, const int16_t m[9]);  // f48_0000
    static void translate(std::vector<Point3>& pts, int dx, int dy, int dz);  // g49_0000
    void setView(int x0, int y0, int x1, int y1);               // f47_0000
    // f47_04de: a polygon seen in perspective, cut at the eye's plane and
    // then at the view's rectangle.
    std::vector<std::pair<int, int>> project(const std::vector<Point3>& pts) const;
    void monitorGadget();                                       // g27_0f16 (shared by 25-27)
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

    // --- segments 22-24: the end of a game (end.cpp) ---
    void endOfGame();                                           // f09_1dd8 after the map loop
    void allFoundDance();                                       // f23_10aa
    void endScreen(bool happy, bool redraw);                    // f23_140a
    void happyEnding();                                         // f23_0000
    void sadEnding();                                           // f23_0a6a
    std::string highScorePath() const;
    void loadHighScores();                                      // f24_005a
    void saveHighScores() const;                                // f24_0000
    bool addHighScore();                                        // f24_03f6
    void showHighScores();                                      // f24_0112
    void bonusMaze(int level);                                  // f22_0ec8
    void mazeLook(int dx, int dy);                              // f22_0172
    void mazeReveal();                                          // f22_04b0
    void mazePace();
    void mazeWander();                                          // f22_0518
    void mazeStep(int dx, int dy);                              // f22_0a62

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
    struct Square {                 // DS:931C
        uint8_t puzzle = 0xFF, level = 0xFF, object = 0xFF;
        uint8_t state = 0;          // 0 unused, 1 open, 2 solved, 3 failed
    };
    struct Object {                 // DS:C500
        uint8_t museum = 0xFF, square = 0xFF, found = 0;
    };
    // The player record (DS:B465-B595, saved as <name>.INF): name,
    // Edison's colours, the last level, a custom level and a saved game.
    struct Player {
        std::string name;                   // B465, up to 8 letters
        uint8_t colours[4] = {0, 0, 0, 0};  // hair, shirt, trousers, shoes (B46F-B472)
        uint8_t level = 0;                  // B46E
        std::array<Square, 29> customSquares{};  // B473
        uint8_t customLevel = 0xFF;              // B4E7: the custom level's level, FF none
        std::array<Square, 29> savedSquares{};   // B4E8
        std::array<Object, 16> savedObjects{};   // B55C
        uint8_t savedLevel = 0xFF;               // B58C: FF no saved game
        int32_t savedScore = 0;                  // B58D
        int16_t savedTimeLeft = 0, savedTimeTotal = 0;  // B591, B593
        uint8_t savedCustom = 0;                 // B595
    } player_;
    std::array<std::vector<Rgb>, 4> edisonColours_;
    Panels panels_;
    int choice_ = 0;            // [09D2]
    int menuEvent_ = 0;         // [91A4]: 1 make a custom level, 2 play it, 3 the saved game, 4 edit it
    bool customLevel_ = false;  // [C654]: playing a custom level
    struct HighScore {
        std::string name;  // up to 8 characters
        int32_t score = 0;
    };
    std::array<std::array<HighScore, 10>, 9> highScores_{};  // DS:BC24: levels 0-7, then custom
    int nextHandle_ = 1;

    // --- the game in progress ---
    std::vector<uint8_t> sine_;     // segment 51: a quarter sine wave, 2048 words
    std::vector<uint8_t> puzzles_;  // segment 62: each level's puzzles
    std::array<Square, 29> squares_{};
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
    struct {
        int left = 0, top = 0, right = 0, bottom = 0, cx = 0, cy = 0, d = 1;  // DS:764C-765C
    } view3_;
    int ballLevel_ = 0;  // [9282]: the Ball Sculpture's last level
    std::vector<std::vector<Point3>> stars_;  // the Planetarium's, set up once (g28_1218)
    // DS:B7CE: facts learned in Concentration (per theme), asked about in
    // the Question and Answer Period.
    bool learned_[5][10] = {};  // rows: B38C (1-3 Concentration, 4 the Planetarium)

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

    // --- the bonus maze (DS:C284, 8A00, 89DE, 8AA0-8ABA) ---
    struct Maze {
        std::array<std::array<uint8_t, 24>, 15> cells{};
        std::array<std::array<bool, 24>, 15> visible{}, shown{};  // [8A00], [8A50]
        struct Wanderer {
            int x = 0, y = 0, px = 0, py = 0;
            int dir = -1;          // 0 up, 1 left, 2 down, 3 right
            bool alarmed = false;  // saw Edison: turns back
            int step = 0;          // quarters of a cell
        };
        std::array<Wanderer, 3> wanderers{};
        int x = 1, y = 12;         // [8AAC], [8AAE]
        int level = 0;             // [8AB6]
        int timeLeft = 0;
        int exitX = 0, exitY = 0;  // [B796], [B798]
        uint16_t sprite = 0x2144;  // [8AA6]: Edison's last frame, at [8AA8], [8AAA]
        int spriteX = 0, spriteY = 0;
        bool done = false;         // [8AA4]
        bool moved = false;        // [8AA5]
        uint64_t lastStep = 0;
        bool seen(int x, int y) const;
        void see(int x, int y);
    } maze_;
};

}  // namespace edison
