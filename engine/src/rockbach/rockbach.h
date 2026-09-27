#pragma once

#include <array>
#include <initializer_list>
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

    // --- songs in the FM driver (segment 20, music.cpp) ---
    void musicReset();                                          // f20_0000
    void musicStop();                                           // f20_0248
    void musicPlay(int from = 0);                               // f20_0070 / f20_00e8
    void musicPlaySlot(int slot);                               // g20_016c
    void setSong(const int tracks[16][2]);                      // f20_0386: {track, style} per slot
    void setSlot(int track, int slot, int style);               // f20_03fa / f20_0532 / f20_0618

    // --- the intro ---
    void corelPresents();                                       // f25_0016
    void logo();                                                // f05_04d8
    void logoFrame();                                           // f05_03d4 + f05_02c2

    // --- the widgets (segment 34, widgets.cpp) ---
    // A slider's record (flags 08 horizontal, 10 vertical).
    struct Slider {
        int min = 0, value = 0, max = 0;
        int w = 0, h = 0;                     // the knob
        int a = 0, c = 0;                     // the track's offsets (across, along)
        int len = 0;                          // the track's length
        int kx = 0, ky = 0;                   // the knob's position
    };
    struct Widget {
        enum : uint16_t {
            kPressed = 0x2, kNoBevel = 0x4, kSlider = 0x18, kLabel = 0x20, kRadio = 0x40,
            kFace = 0x80, kBitmap = 0x100, kHidden = 0x200, kToggle = 0x400,
        };
        int x0, y0, x1, y1;                   // inclusive
        uint16_t flags;
        std::string label = {};               // kLabel
        Slider* slider = nullptr;             // kSlider
        int group = 0;                        // kRadio
        uint8_t faceDown = 0, faceUp = 0;     // kFace
        uint16_t bitmapDown = 0, bitmapUp = 0;
        int hotkey = 1;                       // (upper case)
        uint16_t overlayUp = 0, overlayDown = 0;
        char mode = 0;                        // 'c' clear bitmaps, 'b' both overlays, else opaque
        bool toggle = false;                  // kToggle
    };
    struct Bevel {
        uint8_t a, b, c, d;                   // left, top, bottom, right when up
    };
    void initWidgets(std::vector<Widget>& list, Bevel bevel);   // f34_0ebc
    void drawWidget(const Widget& w, bool pressed);             // f34_07b8
    // f34_0fb6: the index pressed, or -1 (a click on no widget is left in
    // lastClick_, a key in lastKey_).
    int pollWidgets();
    void placeSlider(Widget& w, bool onDisplay);                // f34_0d6c / f34_0e14: the knob from the value
    void drawSlider(const Widget& w, bool onDisplay);           // f34_0372 / f34_0168
    void dragSlider(Widget& w);                                 // f34_04dc
    void showWidgets(std::initializer_list<int> which);         // 200 off, drawn
    void hideWidgets();                                         // 200 on, all

    // --- the hallway (segment 24, hallway.cpp) ---
    // Returns the hot spot clicked (the colour of mask 1006 under the
    // mouse): 1 leave, 2-4 and 6-9 an activity.
    int hallway(bool again);                                    // f24_1d4a
    void hallwayWidgets();                                      // f24_0000
    void edison(int from, int to);                              // f24_0666: Edison's frames
    void bubble(int x, int y, int w, int h, int tail);          // f24_0448
    int say(int a, int b, int c, int d, bool withName);         // f24_1452: a handle
    void enterName();                                           // f24_058e
    bool editLine(std::string& s, int max, int x, int y, int w, uint8_t colour);  // f04_0d52
    void findPlayer(bool* found);                               // f24_09c0
    void savePlayer(bool keep);                                 // f24_088a
    void loadLook();                                            // f24_0e94: ed.yyy
    void saveLook();                                            // f24_0f10
    void lookColours(int which);                                // f24_0cd8
    void lookEditor();                                          // f24_1bba (+ f24_1b00)
    int askChangeLook();                                        // f24_191e: 0 yes
    int askQuit();                                              // f24_0f8c: 1 quit
    void credits();                                             // f24_12e6
    void activity(int which);                                   // f33_0422's switch

    // --- the jukebox (segment 3, and its music, segment 9: jukebox.cpp) ---
    int jukebox();                                              // f03_22e8
    void jukeboxWidgets();                                      // f03_006c
    void bandCommand(const std::vector<uint8_t>& bytes);        // f09_036e: over sound 1, started
    void bandReset();                                           // f09_0000
    void bandStop();                                            // f09_0220
    void bandStart();                                           // f09_00fc
    void bandVolume(int role, int volume);                      // f09_0262
    void bandTempo(int tempo);                                  // f09_0320
    void bandPart(int role, int choice);                        // f09_00bc

    // --- the Drum Clinic (segment 6, and its drum machine, segment 10: drums.cpp) ---
    int drumClinic();                                           // f06_1e9e
    void drumClinicWidgets();                                   // f06_0046
    void drumsReset();                                          // f10_0000
    void drumsDone();                                           // f10_00e6
    void drumTick();                                            // g10_0122
    void drumStart();                                           // f10_01e6
    void drumStop();                                            // f10_0212
    void drumPlay(int drum);                                    // f10_0242
    void drumVariant(int drum, int v);                          // f10_02ae
    void drumToggle(int drum, int step);                        // f10_02fa
    void drumPattern(int p);                                    // f10_03c2
    void drumKit(int kit);                                      // f10_0410
    void drumTempo(int t);                                      // f10_04b6

    // --- the Music Library (segments 30, 21 and 12: library.cpp) ---
    struct TextBox {
        int block;                            // which '!' block of the file
        bool paging;                          // wait for a key or click when full
        uint16_t bitmap;                      // the background, or 0: a fill
        uint8_t colour;                       // the fill
        int x, y, w, h;                       // the box
        int tx, ty;                           // the text (from tx + 2)
        uint8_t ink;
    };
    int library();                                              // f30_1780
    void libraryWidgets();                                      // f30_0000
    void showComposer(int c, bool viaScreen2);                  // f30_0f24
    void nextInfoPage(int c);                                   // f30_0b98
    void pieceList(int c, bool viaScreen2);                     // f30_0d46
    void libraryStopped();                                      // f30_0c7c
    void choosePiece(int widget, int c, bool viaScreen2);       // f30_169e
    bool textBox(const std::vector<std::string>& lines, const TextBox& box, bool viaScreen2);  // f21_0000
    void pieceReset();                                          // f12_0000
    void pieceChoose(int composer, int n);                      // f12_007a
    void pieceInstrument(int i);                                // f12_00b6
    void pieceStart();                                          // f12_0118
    void pieceStop();                                           // f12_018c
    void pieceTempo(int t);                                     // f12_0270

    // --- Harmony Hall (segments 7 and 11: harmony.cpp) ---
    int harmonyHall();                                          // f07_1908
    void harmonyWidgets();                                      // f07_0000
    void showChord(int key, int chord);                         // f07_1278 (+ f07_1048, f07_112a)
    void harmonyCommand(const std::vector<uint8_t>& bytes);     // f11_04ee: over sound B, started
    void harmonyReset();                                        // f11_0000
    void harmonyStop();                                         // f11_03a2
    void harmonyStyle(int s);                                   // f11_00b2
    void harmonyKey(int key);                                   // f11_00e4
    void harmonyChord(int chord);                               // f11_0100
    void harmonyPart(int part, int choice);                     // f11_011c
    void harmonyStart();                                        // f11_0288
    void harmonyVolume(int part, int volume);                   // f11_03e0
    void harmonyTempo(int t);                                   // f11_04a2
    void harmonyRiffs();                                        // f11_05e2 (+ f11_0790)
    // f27_01c4; also forgets a click or key f24_0666 has seen.
    void clearInput() {
        ArtechGame::clearInput();
        inputSeen_ = false;
    }

    // --- data from WINMAIN.EXE's data segment (read at run time) ---
    std::string dataString(uint16_t offset) const;
    uint16_t dataWord(uint16_t offset) const;

    Options options_;
    std::string cdRoot_;
    std::vector<uint8_t> data_;
    int driver_ = -1;  // [83D0]
    int keyShift_ = 0, modeShift_ = 0;  // [2164], [2166]
    uint8_t tempo_ = 0xC0;              // [20B6]

    // The band members on the stage (the logo, the jukebox): 36 of them.
    int member_ = 0;                                  // [69C4]
    std::array<bool, 36> memberPlaying_{};            // DS:0074 + 8m
    std::array<int, 36> memberFrame_{};               // DS:0076 + 8m
    struct Spot {
        int left = 0xC8;                              // [4E7A]: the spot's left end (the right is 100 on)
        int speed = 2;                                // [0780]
    } spot_;

    // The jukebox.
    std::vector<Widget> jukeboxWidgets_;
    std::array<Slider, 9> jukeboxSliders_{};           // DS:8BD8 (8 x 0x14), 8C78: tempo
    struct {
        std::array<int, 4> members{};                  // DS:6736: drums, chords, bass, solo
        std::array<uint16_t, 4> part{};                // [14B4]: each role's part record
        std::array<uint8_t, 4> volume{};               // [126E]
        uint8_t tempo = 0;                             // [1272]
        int song = 0;                                  // [8710]
    } band_;
    // Harmony Hall.
    std::vector<Widget> harmonyWidgets_;
    std::array<Slider, 5> harmonySliders_{};           // DS:8BD8 (4 parts), 8C28 the tempo
    struct HarmonyPlayer {
        std::array<uint16_t, 4> parts{0x16D0, 0x16A0, 0x1640, 0x1670};  // [172C]: drums, rhythm, lead, bass
        std::array<uint8_t, 4> volume{};               // DS:1608
        int key = 0, chord = 0, style = 0;             // [17BC], [17BE], [66C2]
    } harmony_;
    // The Music Library.
    std::vector<Widget> libraryWidgets_;
    Slider librarySlider_{};                           // DS:8BD8: the tempo
    struct LibraryState {
        std::vector<std::string> info, words, list;    // LIBINFO.HI, SONGINFO.HI, SONG.HI
        std::array<int, 8> page{};                     // DS:8CEC: each composer's INFO page
        int infoPage = 0;                              // [8CEA]
        int piece = -1;                                // [bp-C]
        bool playing = false;                          // [673E]
        int marker = 0;                                // the timeline's saved area
    } library_;
    struct Piece {
        uint16_t record = 0x17E0;                      // [18A8]
        int instrument = 3;                            // [17D5]
        int tempo = 0;                                 // [17D6]
    } piece_;
    // The Drum Clinic.
    std::vector<Widget> clinicWidgets_;
    std::array<Slider, 2> clinicSliders_{};            // DS:8BD8 tempo, 8BEC the step
    struct DrumMachine {
        std::array<uint8_t, 0x1000> patterns{};        // DS:73CA: kit x pattern x 64 steps, a bit a drum
        int kit = 0, pattern = 0;                      // [8E10], [8762]
        int period = 0xA;                              // [1506]
        bool loop = false, running = false;            // [1508], [1509]
        int step = 0, wait = 0;                        // [52A0], [83D6]
        std::array<uint8_t, 5> variant{}, sound{};     // DS:150A, 1500
        bool onOwn = false;                            // [150F]: pattern 7, the player's
    } drums_;
    // Widgets.
    std::vector<Widget>* widgets_ = nullptr;           // [8D30]
    Bevel bevel_{};                                    // [655A..6560]
    int lastKey_ = 0;                                  // a key no widget took
    struct {
        bool on = false;
        int x = 0, y = 0;
    } lastClick_;                                      // [3BA8], [3BAA]: a click no widget took
    // The hallway.
    std::vector<Widget> hallwayWidgets_;               // DS:69F4
    std::string name_;                                 // DS:21A0
    int slot_ = 0;                                     // [9244]: the player in user.yyy
    int playerFlag_ = 0;                               // the record's +14 byte
    std::array<uint8_t, 4> look_{};                    // [8D44]: Edison's look, 0-7 a part
    std::array<std::vector<Rgb>, 4> lookTables_;       // DS:223E, 2286, 22CE, 232E (f24_0c0c)
    std::array<Rgb, 13> savedLook_{};                  // DS:8CC0: colours E1-ED
    int edisonArea_ = 0;                               // Edison's saved area on screen 2
    bool edisonTick_ = false;                          // [8CA0]
    bool inputSeen_ = false;                           // [83D8] / [3BA7] still set
};

}  // namespace edison
