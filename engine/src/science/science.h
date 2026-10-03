#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "artech/game.h"

namespace edison {

// The Wild Science Arcade (WMAIN.EXE): see docs/SCIENCE.md. So far the
// title and the story; the lab and the arcade's rooms come next.
class Science : public ArtechGame {
public:
    struct Options {
        std::string cdDir;              // the CD's DSK3 folder (WMAIN.EXE, GRAFX.DAT, S*.SRF)
        bool music = true;              // not -A
        std::string saveDir = "save";   // wscience.edi, wscience.hs
        int startRoom = -1;             // for testing: 501 the lab, 505-510 the lessons
    };

    explicit Science(Platform& platform) : ArtechGame(platform) {}

    bool load(const Options& options, std::string* error);
    // Plays until the player leaves (the original then runs "edison.exe -O").
    void run();

private:
    // --- the framework's helpers ---
    void showScreen(uint16_t picture, int screen);  // f63_09d7: a full screen and its palette
    // f14_092c: the same with Edison's look in its colours E1-ED (the
    // tables at seg95:004F, 0097, 00DF, 013F; the look at DS:1C56).
    void showScreenWithLook(uint16_t picture, int screen);
    void toDisplay(int screen);                     // f20_00f3: the screen and its palette shown
    void clearDisplay();                            // f20_0094: screen 3 in colour 2, shown
    void fmSound(uint16_t sound);                   // f32_135d: SADLIB's SENDSND
    // f36_00ad: <CD>\science\<name>.wav, else data\<name>.wav; `story`
    // picks the names at seg97:0519 (else seg97:0000).
    void narration(int n, bool story);
    void waitNarration();                           // while [92BC]
    // A wait of `ticks` 50ths of a second (f32_07aa, the 50 Hz counter
    // [12F8:0002] that f32_0777(50) starts); a click or key ends it.
    bool waitTicks(int ticks, bool interruptible = true);
    bool escapePressed();                           // bit 1 of the keys held (DS:9560)

    // --- the opening (segments 32 and 38) ---
    void title();                                   // f32_0319 with [26CE] set
    void story();                                   // f38_0718

    // --- the lab, room 501 (segment 19, lab.cpp) ---
    void lab();                                     // f19_0a59
    void waitCountdown(int ticks);                  // [95F2], 10 a second
    void textAt(int x, int y, const std::string& s, int colour);  // f76_0021
    void sound(uint16_t id);                        // f32_13f2: a WAV in GRAFX.DAT
    Rgb lookColour(int part, int choice, int k) const;
    void applyLook();                               // f19_06bc
    void mouth(int talks);                          // f19_0541
    void labBackground();                           // f19_0f6f
    void walk(int mode);                            // f19_0335
    std::string enterName(int x, int y, int maxLength, int width, int colour);  // f19_0003
    void askName();                                 // f19_0249
    void characterEnhancer();                       // f19_0976
    void loadPlayers();                             // f19_115a: wscience.hs
    void savePlayers() const;                       // f21_0447
    void loadLook();                                // wscience.edi
    void saveLook() const;

    // --- the lessons, rooms 505-510 (segment 15, lesson.cpp) ---
    struct Bubble {
        int x = 0, y = 0, w = 0, h = 0;  // what it covers
    };
    std::vector<std::string> wrapText(const std::string& text, int width) const;  // f23_02a4
    void drawStretched(int x, int y, int w, int h, uint16_t id);                  // f14_148a
    Bubble bubble(int ax, int ay, int width, int tail, const std::string& text);  // g15_0467
    std::string textResource(uint16_t id);
    bool waitMore();
    void lessonStart(uint16_t picture);             // f15_076a
    int lesson(int n);                              // f38_0fb5 (5-10): the next room

    // --- the arcade's table (segments 12, 25, 27, 34; table.cpp) ---
    struct Rect {
        int x = 0, y = 0, w = 0, h = 0;
    };
    // A box (f12_0312 / f12_04a1, class 116E): a node of the room's shape.
    struct Box {
        Box* parent = nullptr;          // +2 (the root's: none, height 0)
        Rect bottom, top;               // +4 at the parent's height, +C at `height`
        int height = 0;                 // +14
        int stepX = 30, stepY = 30;     // +2E, +30: the grid
        // +3E-+44 (f12_093b): sides the camera can't see. Standing up: the
        // left and the back; a pit: the right and the front.
        bool hideLeft = false, hideBack = false, hideRight = false, hideFront = false;
        std::vector<std::unique_ptr<Box>> children;  // +16
        int parentHeight() const { return parent ? parent->height : 0; }
    };
    // The room as its camera (segment 25) and the root of its boxes.
    struct Table {
        Box root;
        Rect view;                      // +50: the play area
        int scrollX = 0, scrollY = 0;   // +58, +5A
        int bottom = 0;                 // +5C: the view's bottom row
        int sin = 0, cos = 0;           // +4C, +4E: of the angle (+46), in 32767ths
    };
    static Rect intersect(const Rect& a, const Rect& b);  // f11_0c12
    static bool inside(const Rect& r, int x, int y);
    bool loadTable(int room);                       // f27_0ad8: S<n>.SRF's shape
    std::pair<int, int> project(int x, int y, int h) const;  // f25_0813
    void hiddenSides(Box& box) const;               // f12_093b
    int faceAt(const Box& box, int x, int y) const;  // f34_02fa: 0 none, 1 top, 2-5 sides
    int heightAt(const Box& box, int x, int y) const;  // f12_44f9 (f34_07ec)
    void drawTable();                               // f27_0e5b (the room's method 0, f27_0ec5)
    void roomPictures(int room);                    // the room's method 4 (room 1: f41_0126)
    void redrawTable(const Rect& area);             // f27_1e36, the room's method 3
    void enterRoom(int room);                       // f31_0783 for rooms 1-100
    void drawBox(const Box& box);                   // f12_2a09
    void drawStanding(const Box& box);              // f12_38ad: on screen 3, the pits cut out
    void drawPits(const Box& box);                  // f12_39b5: on screen 2
    void faceFill(std::vector<std::pair<int, int>> points, int look, bool texture);  // f12_0ddf
    void tableLine(int x0, int y0, int x1, int y1, uint8_t colour);  // f14_15f1: ends clamped to the view
    void gridX(const Box& box, int face, int x, int y, int end);  // f12_335e
    void gridY(const Box& box, int face, int x, int y, int end);  // f12_35c5
    bool gridOn(int face) const;
    void showTable(int room);                       // for testing: the room still, till a click

    std::string dataString(uint16_t offset) const;  // DGROUP (segment 103)

    Options options_;
    std::string cdRoot_;               // the CD's root (its SCIENCE folder of sounds)
    std::vector<uint8_t> data_;        // DGROUP
    std::vector<uint8_t> strings_;     // segment 97: the sounds' names
    std::vector<uint8_t> looks_;       // segment 95: the look's colour tables
    uint8_t look_[4] = {};             // DS:1C56: hair, face, shirt, trousers
    struct PlayerEntry {
        std::string name;
        long score = 0;
        int a = 0, b = 0;
        int look[4] = {};
    };
    std::vector<PlayerEntry> players_;  // the high scores (and looks), wscience.hs
    std::string playerName_;            // DS:8D22
    unsigned labFrame_ = 0;             // [1D40]
    bool looksConverted_ = false;       // [1D3E]: f19_0614 has run
    Table table_;
    long score_ = 0;                    // the room's +F35
    int shots_ = 0;                     // +F39
    bool gridFaces_[6] = {};            // [11F0] 1, [11EE] 2, [11EC] 3, [11E8] 4, [11EA] 5
};

}  // namespace edison
