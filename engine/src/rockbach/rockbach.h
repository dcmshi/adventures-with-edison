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
    std::vector<uint8_t> soundData(uint16_t id);                // f27_039a: the WAV file
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

    // --- the file dialogs (segments 22 and 23: dialogs.cpp) ---
    void dialogColours();                                       // f27_0054
    static Widget dialogButton(int x, int y, uint16_t pressed, uint16_t up, int w, int h);
    void bevelBox(int x, int y, int w, int h, bool pressed);    // f23_0ab8
    std::vector<std::string> listFiles(const std::string& dir, const std::string& ext);  // f22_0000
    bool fileList(int x, int y, const std::string& dir, bool cd, const std::string& ext, std::string* out, int kind,
                  const std::string& title, bool saveBackground);  // f22_1864
    bool loadDialog(int x, int y, const std::string& ext, std::string* out, int kind, const std::string& title);  // f22_1454
    bool nameDialog(int x, int y, std::string* name, const std::string& ext, std::string* out);  // f22_099e
    int messageBox(int x, int y, const std::vector<std::string>& lines, const std::vector<int>& buttons);  // f22_0dd2

    // --- Sound FX (segments 29 and 13: soundfx.cpp) ---
    int soundFx();                                              // f29_17e2
    void soundFxWidgets();                                      // f29_0000
    void sfxTip(int n);                                         // f29_16f4
    void sfxSliders(int which, bool apply);                     // f29_1322
    void sfxToggle(int k, std::array<bool, 5>& on);             // f29_152e
    void sfxWave(int left, int right);                          // f29_0ffe
    void sfxProcess();                                          // f13_0a90
    void sfxSet(int type, int value);                           // f13_0964
    bool sfxLoad(const std::string& path);                      // f13_0eb6
    std::vector<uint8_t> sfxWav(long from, long to) const;
    void sfxPlay();                                             // f13_117e

    // --- the Instrument Room (segment 8: instruments.cpp) ---
    int instrumentRoom();                                       // f08_0f2e
    void instrumentWidgets();                                   // f08_002c
    void showInstrument(int i, const std::vector<std::string>& text, int page, bool viaScreen2);  // f08_0e0e
    void instrumentText(const std::vector<std::string>& text, int i, int page, bool viaScreen2);  // f08_0802

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
    // --- the Studio (segments 35, 4, 14 and 15: studio.cpp) ---
    int studio();                                               // f35_018a
    int studioRoom(bool edit);                                  // f04_112e: 0 leave, 2 the song, 5 the video, 6 play
    void studioWidgets();                                       // f04_0000
    void studioSign(bool onDisplay);                            // f04_0824
    int studioWhatNext();                                       // f04_0580: 7 song, 8 video, 9 done
    bool videoLoad();                                           // f04_0b46
    void videoDelete();                                         // f04_0bf2
    bool videoSave();                                           // f04_0cd2
    void videoBlank(bool keepProducer);                         // f33_0144
    void videoNew();                                            // f04_107c
    void studioBackdrop(uint16_t id);                           // a backdrop on screen 2 in Edison's colours
    int studioSong();                                           // f35_0000
    int studioVideo();                                          // f35_005a
    int bandMaker();                                            // f14_10f4
    void bandMakerWidgets();                                    // f14_0000
    int songMaker();                                            // f15_1f84: -99 back to the band
    void songMakerWidgets();                                    // f15_0000
    // f15_1d7e: Edison walks in with `text`; then either waits (10 s, a
    // click or a key) or asks for a name (0 the song, 1 the band, 2 the
    // video). `area` is the saved area under him (on screen 2).
    void edisonSays(int* area, const std::string& text, bool ask, int which);
    void askName(uint16_t field);                               // f15_1cf2

    // The video makers (segments 16, 17, 26): kind 1 the backgrounds, 2
    // the special effects, 3 the camera views. They return the next maker
    // (1-3), a preview (11-13: kind + 10) or -66 (leave).
    int videoMaker(int kind);                                   // f16_217c / f17_20f2 / f26_1edc
    void backgroundMakerWidgets();                              // f16_0000
    void effectMakerWidgets();                                  // f17_0000
    void cameraMakerWidgets();                                  // f26_0000

    // --- the Studio's video player (segment 18: video.cpp) ---
    // Modes: 0 the whole video (in the front room), 1-3 the previews of
    // the background, special effect and camera makers.
    void playVideo(int mode);                                   // f18_22a4
    void videoCredits();                                        // f04_08cc
    void videoScene(int scene, int mode);                       // f18_219a
    void videoBackground(int scene, int mode);                  // f18_1f78
    void videoRamps(int scene, uint16_t colours, int first);    // f18_0520 / f18_0778
    void videoEffectStart(int scene);                           // f18_0252
    void videoCamera(int scene, int mode);                      // f18_1c00
    void videoFrame(int mode, bool onDisplay);                  // f18_0bb0
    void videoColours(const std::vector<Rgb>& colours, int first);  // f18_0000
    void videoTurn(int first, int count);                       // f18_09c0 / f18_0ab8
    void videoMeter();                                          // f18_00d0
    void videoSign();                                           // f18_03f8
    int sine(int a) const;                                      // f53_103b: a turn is 0x10000, 1 is 0x1000
    int cosine(int a) const { return sine(a + 0x4000); }        // f53_106d

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
    // The dialogs.
    std::array<uint8_t, 16> dialogColour_{};           // [69CE]
    std::array<int, 2> loadSource_{-1, -1};            // [219E] videos, [219C] sounds: 1 CD, 2 hard drive
    // Sound FX.
    std::vector<Widget> soundFxWidgets_;
    std::array<Slider, 6> sfxSliders_{};               // DS:8BD8
    struct SoundFxState {
        std::vector<uint8_t> header;                   // DS:8722
        int hdr = 0;                                   // [8DCE]
        std::vector<int8_t> input;                     // [6742]: signed
        std::vector<uint8_t> a;                        // buffer A: the result, unsigned
        long len = 0, start = 0, end = 0;              // [8714], [8CBC], [8B76]
        int fileRate = 0, rate = 0;                    // [69F0], [83CA]
        bool reverse = false, echo = false, reverb = false, filter = false, loop = false;
        bool upToDate = false, processing = false;     // 18C6, 18CD
        int filterFreq = 1000, reverbGain = 50, echoDelay = 300, echoGain = 50;
        long reverbDelay = 30;
    } sfx_;
    std::array<int, 2> sfxHandle_{0, 0x266};           // DS:285A, 2862: the handles' x
    std::array<int, 2> sfxArea_{};
    int sfxWaveArea_ = 0, sfxScale_ = 1;
    // The Instrument Room.
    std::vector<Widget> instrumentWidgets_;
    std::vector<uint8_t> sample_;                      // [3BB4]: the instrument's WAV
    int markerStep_ = 0;                               // [6752]
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
    // The Studio. The video being made is the record at DS:6754 (0x26A
    // bytes, which is also the .vid file), read and written by its DS
    // addresses: see docs/ROCKBACH.md.
    std::array<uint8_t, 0x26A> video_{};
    // Its fields, by DS address (words unless said; x16 is one per scene).
    enum VideoField : uint16_t {
        kVersion = 0x6754,      // byte: 1
        kMembers = 0x6755,      // 4 bytes: the player in each role, -1 none
        kSong = 0x6759,         // x16 {track, style}
        kTempo = 0x6799,
        kBgTurns = 0x679B,      // x16 bytes: the background's colours turn
        kBgColours = 0x67AB,    // x16 x4
        kBackground = 0x682B,   // x16
        kFxTurns = 0x684B,      // x16 bytes
        kFxColours = 0x685B,    // x16 x4
        kEffect = 0x68DB,       // x16
        kCamPlays = 0x68FB,     // x16 bytes: a member starts playing
        kCamColours = 0x690B,   // x16 bytes: the members' colours put back
        kCamMotion = 0x691B,    // x16: 1 bouncing, 2 spinning
        kCamPlaces = 0x693B,    // x4 {x, y}: the player's own
        kCamera = 0x694B,       // x16
        kVideoName = 0x696B, kSongName = 0x697F, kBandName = 0x6993, kProducer = 0x69A7,  // 20 bytes each
        kVideoNamed = 0x69BB, kSongNamed = 0x69BC, kBandNamed = 0x69BD,                    // bytes
    };
    uint8_t& vbyte(uint16_t at) { return video_[at - 0x6754u]; }
    int vword(uint16_t at) const { return static_cast<int16_t>(video_[at - 0x6754u] | video_[at - 0x6753u] << 8); }
    void setVword(uint16_t at, int v) {
        video_[at - 0x6754u] = static_cast<uint8_t>(v);
        video_[at - 0x6753u] = static_cast<uint8_t>(v >> 8);
    }
    std::string vstring(uint16_t at) const;
    void setVstring(uint16_t at, const std::string& s);          // 20 bytes
    std::vector<Widget> studioWidgets_;                // DS:69F4 (the hallway's list, reused)
    std::array<Slider, 1> studioSliders_{};            // DS:8BD8: the song maker's tempo
    bool videoOpen_ = false;                           // [8D1C]: a video is loaded or made
    bool videoChanged_ = false;                        // [673A]
    bool bandMade_ = false;                            // [2EFE]
    std::string videoName_;                            // DS:4A4C: its file name
    int signArea_ = 0;                                 // [8D0C]
    // The player (segment 18).
    int videoX_ = 0x7E, videoY_ = 0x1C;                // [2E92], [2E94]: the picture's corner (352 x 248)
    struct VideoState {
        int scene = 0;                                 // [86F6]
        int clean = 0;                                 // [9270]: the picture without the band
        int flashArea = 0, walkerArea = 0;             // [83DC], [69F2]
        bool bgTurns = false, fxTurns = false;         // [8D32], [6740]
        bool bgDue = false, fxDue = false;             // [8D1A], [674C]
        int32_t angle = 0;                             // [86E4]
        int colour = 0x40;                             // [50B6]
        std::array<int, 8> sparkX{}, sparkY{};         // DS:5088, 5098
        int sparkTick = 0, sparkEvery = 3;             // [52A2], [8D08]
        int flash = -1, flashX = 0, flashY = 0;        // [1FCC], [64B0], [52AE]
        int boxW = 0, boxH = 0;                        // [8DD4], [8DD2]
        int walker = 0, walkerX = 0, walkerY = 0;      // [8D12], [69C8], [69EE]
        std::array<int, 4> vx{}, vy{};                 // DS:8D1E
        std::array<bool, 4> shown{};                   // DS:871A
        std::array<int, 4> rolePlaying{};              // DS:1FCE
        int scale = 0x100;                             // [876E]
        int meter = 8;                                 // [8D48]
        int tape = 0;                                  // [4F76]
    } vid_;
    std::array<Rgb, 64> cameraColours_{};              // [bp-CE]: colours 80-BF as the video started
    std::vector<uint8_t> sine_;                        // WINMAIN's segment 53: a quarter of a sine wave
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
