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
    // An object of S<n>.SRF: OBJn x y type a b c d e f (f61_011d).
    struct Object {
        int x = 0, y = 0, type = 0;
        int args[6] = {};
        // A hole's state (f28_00f3): swallowing the ball (+21, counting
        // ticks; +27 done), spitting it out (+23; +29 done; +1F how), and
        // the ball still leaving it (+25).
        int swallow = 0, spit = 0, spitMode = 0;
        bool swallowDone = false, spitDone = false, leaving = false;
        // A target (type 10, f03_002c; c its kind, b its points): hit (+12,
        // counting), scored (+14, counting), its frame (+18) of a sequence
        // (+16: DS offset, 0 its kind's idle frames) of +1A frames; its
        // drawables' creation number (+1E: the animations' phase); live
        // (its +34 / +38 not 0) and shown.
        int kind = 0, points = 0, hit = 0, scored = 0, frame = 0, frames = 0, sequence = 0, id = 0;
        bool live = false, shown = true;
    };
    // The room as its camera (segment 25) and the root of its boxes.
    struct Table {
        std::vector<Object> objects;
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
    int heightUnder(int x, int y) const;            // f27_0903 + f34_07ec
    std::pair<int, int> objectCentre(int x, int y, int z, int w, int d, int h) const;  // f25_0a51
    Rect objectRect(int x, int y, int z, int w, int d, int h) const;               // f27_16ae
    void objectSprite(int cx, int cy, uint16_t id);  // f14_0d69 at 1:1
    void drawObjects();                             // the drawables at rest
    void roomPictures(int room);                    // the room's method 4 (room 1: f41_0126)
    void redrawTable(const Rect& area);             // f27_1e36, the room's method 3
    void enterRoom(int room);                       // f31_0783 for rooms 1-100

    // --- the controls (segment 30; panel.cpp) ---
    // A room's PANEL line (f61_0f76): four (flags, value) pairs; flags 1
    // locks the control, 2 hides it (f30_2081, f30_2097).
    struct PanelState {
        int gravity = -4, friction = 4, power = 5, ballType = 2;
        int gravityFlags = 0, frictionFlags = 0, powerFlags = 0, ballTypeFlags = 0;
        // Locked controls' OUT OF ORDER signs (+2C): 0 gravity, 1 friction,
        // 2 the ball type, 3 power.
        bool sign[4] = {};
    };
    // Edison putting the signs up (segment 30's walking figure, the panel's
    // +132-+15E: f30_0910; its tick f30_0d7c): where (its middle), which
    // way (+146, 17 a step), where to (+148), what for (+15E: 1 going, 2
    // putting a sign up), its frames (+13C, from +13E, +140 of them, a step
    // every +142 ticks) and a mode change pending (+144); the controls to
    // sign (+17C, +198 done, +19A of them; +19C the one now).
    struct Runner {
        int x = 0, y = 347, dx = -17, target = 0, flags = 0;
        int frame = -1, first = 0, count = 0, period = 3;
        bool pending = true;
        int queue[4] = {}, queued = 0, done = 0, current = -1;
    };
    Runner runner_;
    bool panelBusy_ = false;   // [22C8]: the controls don't take the mouse
    int runnerTicks_ = 0;      // [22CA]
    void lockControls();       // f61_0f76's flags: 2 a sign now, 1 Edison puts one up (f30_1569)
    void runnerTick();         // f30_1430's f30_0d7c and f30_148f
    void runnerMode(int mode); // f30_0c6e
    Rect controlArea(int c);
    void signAt(int c);
    bool panelSprite(int x, int y, uint16_t id);    // f14_1179
    void drawKnob(int kind, int value);             // 0 gravity, 1 friction, 2 power
    void valueBox(int x, int y, int w, int h, const std::string& text);  // f30_1af3
    void drawPanel(const Rect* changed = nullptr);  // f30_15f2
    void markPanel(const Rect& r);                  // f29_0313
    void drawColumn(bool right);                    // f30_0599
    int borlandRand();                              // f01_3309
    // --- the ball's physics (physics.cpp) ---
    struct Face {
        const Box* box = nullptr;
        int type = 0;  // 0 none (the stand-in at DS:2950), 1 top, 2-5 sides
        bool operator==(const Face& o) const { return box == o.box && type == o.type; }
    };
    // A kind of ball (f33_0003 at start-up: DS:286C Ice ... DS:292A Magic;
    // the mass from f07_05bf).
    struct BallKind {
        int32_t bounceNum, bounceDen;  // +1, +5
        int fragility;                 // +21: breaks above 100h / this
        int mass;                      // the core's +34 / +38
    };
    struct Ball {
        int cx = 0, cy = 0, cz = 0, r = 10;     // the sphere (the motion part's +2)
        int16_t v[3] = {}, rem[3] = {}, disp[3] = {};  // +62, +68 (50ths), +40
        int16_t kick[3] = {}, push[3] = {};     // +46 (the shot), +4C
        bool onGround = true;                   // +5E
        int lastHit = 0, hitFlag = 0;           // +56, +54
        int state = 0;                          // +7C: 1.. breaking
        int kind = 2;                           // its type record (+52)
        int kickTicks = 0;                      // the motion part's +E
        int32_t rollAcc = 0;                    // +A
        int frame = 0, drawFrame = 0;           // +1A, +1C
        int rollThreshold = 6;                  // +1E: r * r / 16 * 9 / 9
        bool hidden = false;                    // its drawable's +60 (in a hole)
        int startX = 0, startY = 0;             // where the room put it (f27_287d puts it back)
    };
    static constexpr int kTimerRate = 50, kTimerK = 42;  // [27B0] (f32_0777), [27B2]
    static constexpr int kStepNum = 10, kStepDen = 100;  // [1010], [1014] (seg8:3A98)
    static constexpr int kMaxPower = 32760;              // [E02]
    int libCos4096(int angle) const;                // f86_106d
    int32_t libLength(const int16_t v[3]) const;    // f11_0000
    void libUnit(const int16_t v[3], int16_t out[3]) const;  // f84_0000
    int16_t libDot(const int16_t a[3], const int16_t b[3]) const;  // f11_0651
    void libScaleTo(int16_t v[3], int length) const;              // f11_02f7
    void libCross(const int16_t a[3], const int16_t b[3], int16_t out[3]) const;  // f11_043e
    Face faceUnder(int x, int y) const;             // f27_0903
    int faceHeight(const Face& f, int x, int y) const;  // f34_07ec
    void faceNormal(const Face& f, int16_t n[3], int32_t* length, int32_t* cosine) const;  // f34_0a10
    void gravityAlongSlope(const Face& f, int32_t num, int32_t den, int16_t out[3]) const;  // f34_0b8a
    void projectOnFace(const Face& f, const int16_t v[3], int16_t out[3]) const;  // f34_0d40
    void acrossFace(const Face& f, const int16_t v[3], int16_t out[3]) const;     // f34_0e4c
    void ballSetForce(const int16_t f[3]);          // f08_13a2
    void ballMove(const int16_t d[3]);              // f08_0aab
    bool ballDampsOthers() const;                   // f08_1802
    void ballBounce(const int16_t normal[3], bool always);  // f08_1843
    void ballStep();                                // f08_1a42
    void ballTick();                                // f07_077e
    void ballLaunch(int tx, int ty, int tz);        // f07_0ca0
    void crackGlass();                              // f27_0772 (not yet)
    void shoot();                                   // f27_27a3 → f06_09a1
    void applyPanelPhysics();                       // the sliders' values to the room's

    // The library's angles (binary: 10000h a turn) and fixed-point vectors.
    int libAtan2(int adjacent, int opposite) const;                 // f87_0804
    std::pair<int, int> libSinCos(int angle) const;                 // f86_1000: (sin, cos) in 32767ths
    void libNormalize(const int v[3], int out[3]) const;            // f84_0000: in 7FFEhs

    // --- playing a room (the player's methods, f31; the room's, f27) ---
    enum class Control { None, Gravity, Friction, Power, BallType, Shoot };
    struct Column {
        bool ballOut = true;     // +132: its ball is on the table (f30_017c: 1)
        bool pushing = false;    // +134
        int offset = 0;          // +138: the top ball pushed up so far
    };
    struct Mouse {
        int x = 0, y = 0;
        bool click = false;      // the event's +9 ([6EC5]: pressed since the last)
        bool held = false;       // [6EC4]
    };
    void saveTickShot(const std::string& dir);    // (testing: SCI_TICKSHOTS)
    int playRoom(int room);                         // the arcade's loop (f32_0c1d) in a room: the next room
    void arcade(int room);                          // event 9 from room to room (f31_0783)
    // The holes (f28_00f3): the sphere the ball meets, a tick (f28_0671),
    // the room's word on a swallowed ball (+20: f28_156a → the room's
    // method 8, f27_2530 or room 1's f41_02a4), spitting it out (+2C,
    // f28_1445), the ball put back (f27_287d).
    void holeSphere(const Object& o, int out[4]) const;
    void holeTick(Object& o);
    void holeEntered(Object& o);
    void spitBall(Object& o, int mode);
    void ballToStart();
    void shadowTick();                              // f07_15ba
    void targetTick();                              // f06_0aa8
    void growCracks();                              // f27_2434's every 20 ticks
    void ballLost();                                // f31_0504
    void dropBall(bool right);                      // f30_02d8 → f27_293b
    bool noShadow_ = false;      // [14E0]: the ball's shadow not drawn (while one drops)
    int roomTicks_ = 0;          // [FFE]: the room ticks (to 1000)
    int crackStage_[5] = {};     // the room's glass marks: +F65 stages, +F3D rectangles
    Rect crackRect_[5];
    bool roomBusy_ = false;      // the room's +F6F: a hole has the ball
    int exitRoom_ = 0;           // event 9's room, when a hole sends the ball on
    int exitHole_ = -1;          // and the hole (its +3), to spit the ball out of on coming back
    void mouseEvent(const Mouse& m);                // f31_241a (event 7)
    bool sliderClick(Control c, const Mouse& m);    // f30_306a
    bool buttonClick(Control c, const Mouse& m);    // f30_28f1
    void columnClick(bool right, const Mouse& m);   // f30_0496
    void aim(const Mouse& m);                       // f27_2d15
    void setSlider(Control c, int value);           // f30_3339 and the kind's +28
    void setBallType(int type);                     // f30_29cc → f30_2574
    void tickRoom();                                // the player's tick (f31_1c79)
    void flushRoom();                               // the areas marked changed, redrawn
    void markButton(Control c);                     // a button's sprite (and the name box)
    void drawBox(const Box& box);                   // f12_2a09
    void drawStanding(const Box& box);              // f12_38ad: on screen 3, the pits cut out
    void drawPits(const Box& box);                  // f12_39b5: on screen 2
    void faceFill(std::vector<std::pair<int, int>> points, int look, bool texture);  // f12_0ddf
    void tableLine(int x0, int y0, int x1, int y1, uint8_t colour);  // f14_15f1: ends clamped to the view
    void libraryLine(int x0, int y0, int x1, int y1, uint8_t colour);  // f63_1cd5 (f80_0024)
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
    std::vector<uint8_t> sines_;       // segment 86: the sine table
    std::vector<uint8_t> atans_;       // segment 87: the arctangent table
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
    PanelState panel_;
    Ball ball_;
    static constexpr BallKind ballKinds_[6] = {
        {4, 10, 20, 5}, {4, 10, 4, 15}, {9, 10, 0, 5}, {2, 10, 0, 20}, {4, 10, 32, 5}, {15, 10, 0, 5}};
    int gravity_ = -200;                // the room's +EFF / +F03
    int32_t frictionNum_ = 45, frictionDen_ = 255;  // +F07, +F0B
    int power_ = 0;                     // +F0F
    int worldW_ = 809, worldD_ = 789;   // [1018], [101A]
    int currentRoom_ = 0;               // the player's +8E
    int timerTicks_ = 0;                // [12F8:0002]
    int lastBounceTick_ = 0;            // [100C]
    int lastHitSound_ = 0;              // [100A]
    int stepDepth_ = 0;                 // [101E]
    Control captured_ = Control::None;  // [27AC] / [27AE]: who has the mouse
    bool ballTypePressed_ = false, shootPressed_ = false;
    bool shotHeld_ = false;  // (testing: SCI_SHOOT_WHEN)  // the buttons' +3A
    int sliderRepeats_ = 0;             // [2338]
    int panelTicks_ = 0;                // [8E4E]
    Column columns_[2];                 // +AA (left), +AC (right)
    bool hasBall_ = false;
    int targetX_ = 0, targetY_ = 0;     // its target (+F79)
    bool targetMoved_ = false;
    // The target's watch on the ball (f06_0aa8): moving (its +60) while the
    // ball's speed is over 100 and its centre changed since the last tick;
    // the ring is hidden then, and the shadow drawn on the ground below.
    bool ballMoving_ = false;
    // The ball's shadow object (f07_12d6, a 2r+1 square box 2 high; its
    // tick f07_15ba): shown on the ground below while the ball's bottom is
    // 2 or more above it, at (x, y, ground + 1); hidden, the ball draws its
    // own under it (f13_01ce).
    bool shadowShown_ = false;
    int shadowX_ = 0, shadowY_ = 0, shadowZ_ = 0;
    int16_t shadowSeen_[3] = {0, 0, 0};  // the ball's centre it last saw
    int16_t lastCentre_[3] = {0, 0, 0};
    bool viewDirty_ = false, panelDirty_ = false, columnDirty_[2] = {};
    Rect panelDirtyRect_;
    int leftBalls_ = 7, rightBalls_ = 0;  // the player's +BA, +BC
    uint32_t randSeed_ = 1;             // Borland's rand()
    long score_ = 0;                    // the room's +F35
    long totalScore_ = 0;               // [BD8]: the game's (0 for a new game)
    int targetsHit_ = 0, targets_ = 0;  // [308], [30A]: the room's targets
    bool shotBonus_ = false;            // [30C]: the targets' points by the shots (+F8D)
    uint8_t targetBonus_[6] = {};       // +F8D: by the shots (128ths)
    long completionBonus_ = 1500;       // +F7B (and +F87 by the shots: the room's end)
    uint8_t completionShare_[6] = {};
    void pointTick(Object& o);          // f03_0207: a point target's tick
    void pointHit(Object& o);           // f03_0593: hit by the ball
    void addScore(long points);         // f06_0208
    void roomConfig(int room);          // the builder's [30C], +F7B, +F8D, +F87
    int shots_ = 0;                     // +F39
    bool gridFaces_[6] = {};            // [11F0] 1, [11EE] 2, [11EC] 3, [11E8] 4, [11EA] 5
};

}  // namespace edison
