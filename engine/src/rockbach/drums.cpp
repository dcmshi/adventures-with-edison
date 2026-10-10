// WINMAIN's segment 6 (the Drum Clinic: eight kits, eight patterns of 64
// steps for five drums, a grid to edit them, a chain of up to 12 kit and
// pattern pairs, playing the drums on the keyboard, and some palette tricks)
// and segment 10 (its drum machine, in ADLIB1.DLL).

#include <algorithm>
#include "formats/paths.h"
#include <array>
#include <cctype>
#include <fstream>

#include "rockbach/rockbach.h"

#include "audio/artech_fm_driver.h"

namespace edison {
namespace {

constexpr int kDrumSlot = 6;    // the drum machine's timer slot (g10_0122, 72 a second)
constexpr int kClinicSlot = 7;  // the Drum Clinic's (g06_1b82, 15 a second)
constexpr int kGridX = 0xC0, kGridY = 0x40, kRow = 0x24, kColumns = 64;
constexpr int kStageX = 0xBA, kStageY = 0x2A, kStageW = 0x10E, kStageH = 0xB2;

// The keys that play each drum (DS:07AC gives scan codes; these are the
// characters they type on a US keyboard).
int drumForKey(int key) {
    static const char* const kKeys[5] = {"`12\tqwaz", "345ertsdfxcv", "678yuighjbnm", "90-op[kl;,.", "=]\\'/ "};
    if (key == Platform::kBackspace || key == Platform::kEnter) return 4;
    if (key <= 0 || key >= 0x80) return -1;
    const char c = static_cast<char>(std::tolower(key));
    for (int d = 0; d < 5; ++d)
        for (const char* k = kKeys[d]; *k; ++k)
            if (*k == c) return d;
    return -1;
}

}  // namespace

// --- the drum machine (segment 10) ----------------------------------------------

void RockBach::drumsReset() {
    // f10_0000: the patterns (kit 0-7 x pattern 0-7 x 64 steps: a bit a
    // drum) from STANDARD.PAT (patterns 0-6) and NONAME.PAT (7, the
    // player's); kit 0, pattern 0, period A, no loop; every drum's first
    // sound; the step timer.
    drums_ = DrumMachine{};
    for (int d = 0; d < 5; ++d) drums_.sound[d] = data_[0x14D8 + d];
    drums_.variant.fill(2);
    if (!options_.music) return;
    auto load = [](const std::string& path, uint8_t* to, size_t count) {
        std::ifstream in(findPath(path), std::ios::binary);
        if (in) in.read(reinterpret_cast<char*>(to), static_cast<std::streamsize>(count));
        return static_cast<bool>(in);
    };
    // f10_0612: STANDARD.PAT, then the player's file (noname.pat when
    // no name is given).
    std::array<uint8_t, 8 * 7 * 64> standard{};
    if (!load(options_.cdDir + "/STANDARD.PAT", standard.data(), standard.size()))
        logLine("Rock and Bach: can't read STANDARD.PAT");
    for (int k = 0; k < 8; ++k)
        std::copy_n(standard.begin() + k * 7 * 64, 7 * 64, drums_.patterns.begin() + k * 8 * 64);
    std::array<uint8_t, 8 * 64> own{};
    if (!load(options_.saveDir + "/noname.pat", own.data(), own.size()) &&
        !load(options_.cdDir + "/NONAME.PAT", own.data(), own.size()))
        logLine("Rock and Bach: can't read NONAME.PAT");
    for (int k = 0; k < 8; ++k) std::copy_n(own.begin() + k * 64, 64, drums_.patterns.begin() + (k * 8 + 7) * 64);
    for (int d = 0; d < 5; ++d) drumVariant(d, 2);
    drumKit(0);
    drumPattern(0);
    drumTempo(0x55);
    ctx_.timer.setPeriodic(kDrumSlot, 0x48, [this] { drumTick(); });
}

void RockBach::drumsDone() {
    // f10_00e6: the timer off, the drums stopped, the player's patterns
    // saved.
    if (!options_.music) return;
    ctx_.timer.setPeriodic(kDrumSlot, 0, nullptr);
    drumStop();
    std::array<uint8_t, 8 * 64> own{};
    for (int k = 0; k < 8; ++k) std::copy_n(drums_.patterns.begin() + (k * 8 + 7) * 64, 64, own.begin() + k * 64);
    std::ofstream out(options_.saveDir + "/noname.pat", std::ios::binary);  // f10_07d2
    if (out) out.write(reinterpret_cast<const char*>(own.data()), static_cast<std::streamsize>(own.size()));
    else logLine("Rock and Bach: error opening noname.pat");
}

void RockBach::drumTick() {
    // g10_0122: every period / 8 ticks the next step's drums; after step 63
    // it stops unless it loops.
    if (drums_.running && drums_.wait == 0) {
        drums_.wait = drums_.period / 8;
        if (drums_.step == kColumns) {
            drums_.step = 0;
            if (!drums_.loop) drums_.running = false;
        }
        const uint8_t bits = drums_.patterns[(drums_.kit * 8 + drums_.pattern) * 64 + drums_.step++];
        for (int d = 0; d < 5; ++d)
            if (bits & (1 << d)) drumPlay(d);
    }
    --drums_.wait;
}

void RockBach::drumStart() {
    // f10_01e6.
    if (!options_.music) return;
    drums_.wait = 0;
    drums_.running = true;
}

void RockBach::drumStop() {
    // f10_0212: sound E silences.
    if (!options_.music) return;
    ctx_.platform.sendFm(0xE);
    drums_.running = false;
}

void RockBach::drumPlay(int drum) {
    // f10_0242.
    if (options_.music) ctx_.platform.sendFm(drums_.sound[drum]);
}

void RockBach::drumVariant(int drum, int v) {
    // f10_02ae: each drum has three sounds (0x14 apart); v 2 is the first.
    drums_.variant[drum] = static_cast<uint8_t>(2 - v);
    drums_.sound[drum] = static_cast<uint8_t>(data_[0x14D8 + drums_.kit * 5 + drum] + drums_.variant[drum] * 0x14);
}

void RockBach::drumToggle(int drum, int step) {
    // f10_02fa: a step's drum on or off; a standard pattern is first
    // copied to the player's (7).
    if (!drums_.onOwn) {
        const auto from = drums_.patterns.begin() + (drums_.kit * 8 + drums_.pattern) * 64;
        std::copy_n(from, 64, drums_.patterns.begin() + (drums_.kit * 8 + 7) * 64);
        drumPattern(7);
    }
    drums_.patterns[(drums_.kit * 8 + drums_.pattern) * 64 + step] ^= static_cast<uint8_t>(1 << drum);
}

void RockBach::drumPattern(int p) {
    // f10_03c2.
    if (!options_.music || p >= 8) return;
    drums_.onOwn = p == 7;
    drums_.pattern = p;
}

void RockBach::drumKit(int kit) {
    // f10_0410: the kit's sounds; each of a drum's three sounds is told
    // which drum it is (its first byte).
    if (!options_.music || kit >= 8) return;
    drums_.kit = kit;
    std::array<uint16_t, 5> base{};
    for (int d = 0; d < 5; ++d) {
        base[d] = data_[0x14D8 + kit * 5 + d];
        drums_.sound[d] = static_cast<uint8_t>(base[d] + drums_.variant[d] * 0x14);
    }
    // (f10_0558 / f10_05b4: a sound's address, GETADDR.)
    ctx_.platform.withFm([&base](ArtechFmDriver& d) {
        for (int drum = 0; drum < 5; ++drum)
            for (int j = 0; j < 3; ++j) d.poke(d.soundAddress(static_cast<uint16_t>(j * 0x14 + base[drum])), static_cast<uint8_t>(drum));
    });
}

void RockBach::drumTempo(int t) {
    // f10_04b6.
    drums_.period = 0x8C - t;
}

// --- the Drum Clinic (segment 6) ----------------------------------------------

void RockBach::drumClinicWidgets() {
    // f06_0046: 61 widgets.
    auto w = [](int x0, int y0, int x1, int y1, uint16_t flags, uint16_t down, uint16_t up, uint16_t ov,
                int group = 0, char mode = 0) {
        Widget r{x0, y0, x1, y1, flags};
        r.group = group;
        r.bitmapDown = down;
        r.bitmapUp = up;
        r.overlayUp = r.overlayDown = ov;
        r.mode = mode;
        return r;
    };
    std::vector<Widget>& l = clinicWidgets_;
    l.clear();
    for (int i = 0; i < 8; ++i)  // 0-7 the kits
        l.push_back(w(i % 2 ? 92 : 20, 34 + 54 * (i / 2), (i % 2 ? 92 : 20) + 69, 85 + 54 * (i / 2), i ? 0x140 : 0x142,
                      0x231A, 0x231B, static_cast<uint16_t>(0x231C + i), 1));
    l.push_back(w(18, 6, 159, 29, 0x124, 0x233F, 0x233F, 0x2000));  // 8 the kit's name
    for (int i = 0; i < 4; ++i)                                    // 9-12 the modes
        l.push_back(w(i % 2 ? 110 : 10, 270 + 64 * (i / 2), (i % 2 ? 110 : 10) + 95, 329 + 64 * (i / 2),
                      i == 1 ? 0x142 : 0x140, 0x2324, 0x2325, static_cast<uint16_t>(0x2326 + i), 3));
    for (int i = 0; i < 5; ++i)  // 13-17 the mouths (each drum's sound)
        l.push_back(w(224 + 80 * i, 270, 295 + 80 * i, 313, 0x104, 0x232C, 0x232C, 0x2000));
    static const uint8_t kFace[5] = {0xD0, 0xE0, 0xDD, 0xEC, 0xA0};
    static const uint16_t kDrum[5] = {0x232F, 0x2330, 0x2331, 0x2333, 0x2334};
    for (int i = 0; i < 5; ++i) {  // 18-22 the drums
        Widget d = w(224 + 80 * i, 316, 295 + 80 * i, 341, 0x184, 0x2000, 0x2000, kDrum[i], 0, 'c');
        d.faceDown = d.faceUp = kFace[i];
        l.push_back(d);
    }
    l.push_back(w(570, 348, 627, 393, 0x100, 0x232D, 0x232E, 0x2000));  // 23 exit
    l.push_back(w(218, 344, 249, 369, 0x100, 0x235A, 0x235D, 0x2000));  // 24-27 the palette tricks
    l.push_back(w(252, 344, 283, 369, 0x500, 0x235C, 0x235F, 0x2000));
    l.push_back(w(218, 372, 249, 397, 0x500, 0x2360, 0x2361, 0x2000));
    l.push_back(w(252, 372, 283, 397, 0x500, 0x2360, 0x2361, 0x2000));
    for (int i = 0; i < 8; ++i)  // 28-35 the patterns
        l.push_back(w(i % 2 ? 552 : 480, 34 + 54 * (i / 2), (i % 2 ? 552 : 480) + 69, 85 + 54 * (i / 2),
                      i ? 0x140 : 0x142, 0x2367, 0x2368, static_cast<uint16_t>(0x234D + i), 2));
    l.push_back(w(478, 6, 619, 29, 0x124, 0x2340, 0x2340, 0x2000));             // 36 the pattern's name
    l.push_back(w(180, 226, 239, 255, 0x140, 0x2009, 0x200A, 0x2000, 23));      // 37 go
    l.push_back(w(334, 226, 413, 255, 0x142, 0x200B, 0x200C, 0x2000, 23));      // 38 stop
    Widget tempo = w(260, 13, 375, 30, 0x108, 0x2356, 0x2356, 0x2355);          // 39 tempo
    tempo.slider = &clinicSliders_[0];
    l.push_back(tempo);
    l.push_back(w(416, 226, 457, 255, 0x100, 0x2358, 0x2359, 0x2000));         // 40 the bomb
    Widget where = w(255, 232, 318, 249, 0x108, 0x2357, 0x2357, 0x2355);        // 41 the step
    where.slider = &clinicSliders_[1];
    l.push_back(where);
    for (int i = 0; i < 12; ++i)  // 42-53 the chain
        l.push_back(w(184 + 68 * (i % 4), 40 + 59 * (i / 4), 249 + 68 * (i % 4), 87 + 59 * (i / 4), 0x304, 0x2000,
                      0x2000, 0x2000, 0, 'c'));
    for (int i = 0; i < 5; ++i) {  // 54-58 the drums beside the grid
        Widget d = w(190, 40 + 36 * i, 261, 65 + 36 * i, 0x384, 0x2000, 0x2000, kDrum[i], 0, 'c');
        d.faceDown = d.faceUp = 0xB;
        l.push_back(d);
    }
    l.push_back(w(188, 8, 245, 37, 0x104, 0x2366, 0x2366, 0x214A, 0, 'c'));   // 59 slower
    l.back().overlayDown = 0x214B;
    l.push_back(w(390, 6, 455, 39, 0x104, 0x214E, 0x214E, 0x214C, 0, 'c'));   // 60 faster
    l.back().overlayDown = 0x214D;
    for (Widget& x : l) x.hotkey = 1;
}

int RockBach::drumClinic() {
    // f06_1e9e.
    static const int kChainX[4] = {0xD4, 0x118, 0x15C, 0x1A0}, kChainY[3] = {0x5C, 0x97, 0xD2};
    auto chainX = [](int i) { return kChainX[i % 4]; };
    auto chainY = [](int i) { return kChainY[i / 4]; };
    auto rowY = [](int d) { return kGridY + kRow * d; };
    std::vector<Widget>& w = clinicWidgets_;
    auto kitName = [this](int k) { return dataString(dataWord(static_cast<uint16_t>(0x81E + 2 * k))); };
    auto patternName = [this](int p) { return dataString(dataWord(static_cast<uint16_t>(0x87C + 2 * p))); };
    auto drumIcon = [this](int kit, int d) { return static_cast<uint16_t>(0x232E + data_[0x784 + kit * 5 + d]); };

    drumClinicWidgets();
    w[8].label = kitName(0);
    w[36].label = patternName(0);
    int kit = 0, pattern = 0;                  // [bp-52], [bp-8A]
    int tempo = 0x3C;                          // [bp-1CE]
    int chainSlot = 0, chainAt = 0;            // [bp-24], [bp-46]
    bool chainKitSet = false, chainPatSet = false;  // [bp-A], [bp-8]
    std::array<int, 12> chainKit, chainPattern;     // [bp-44], [bp-22]
    chainKit.fill(-1);
    chainPattern.fill(-1);
    bool playing = false, edited = false, done = false;  // [bp-16], [bp-36], [bp-54]
    int playhead = -1, lastStep = -1;          // [bp-2C], [bp-86]
    std::array<int, 5> sound{}, step{};        // [bp-50], [bp-34]: the mouths
    sound.fill(3);
    step.fill(-1);
    std::array<std::array<uint8_t, 64>, 5> cells{};  // [bp-1CC]: the grid as drawn
    // The modes (widgets 9-12): the kit, the grid, the chain, the keyboard.
    bool kitMode = false, gridMode = true, chainMode = false, keysMode = false;
    clinicSliders_[1] = Slider{0, 0, 0x52, 0xF, 0x12, 8, 0xD, 0x40};
    clinicSliders_[0] = Slider{0, tempo - 13, 0x64, 0xF, 0x12, 7, 0xC, 0x74};

    select(2);
    blackout();
    backdrop(0x100B);
    initWidgets(w, {0xFC, 0xFE, 0xF7, 0xF5});
    show(2);
    select(1);
    drumsReset();
    drumStop();
    drumTempo(tempo);
    drums_.loop = !drums_.loop;  // f10_0276
    drumStop();
    bool scroll = false, chase = false, swap = false, spin = false;  // [870E], [8CA2], [8B4A], [8E14]
    int half = 0;                                                      // [782]
    ctx_.timer.setPeriodic(kClinicSlot, 15, [&] {  // g06_1b82
        scroll = chase = true;
        if (++half >= 2) swap = spin = true, half = 0;
    });
    const std::vector<Rgb> lights(ctx_.displayPalette.begin() + 0x60, ctx_.displayPalette.begin() + 0x66);  // DS:9246
    const Palette saved = ctx_.displayPalette;                                                              // DS:83DE
    int lightStep = 0;                                                                                       // [8CEB]
    select(2);
    ctx_.screens.drawSprite(2, ctx_.bitmap(0x2362), 0, 0, true);  // the playhead's bar, kept at (0, 0)
    select(1);
    clearInput();
    drumKit(kit);
    drumPattern(pattern);

    auto colours = [this](int first, int count) {
        return std::vector<Rgb>(ctx_.displayPalette.begin() + first, ctx_.displayPalette.begin() + first + count);
    };
    auto drawGrid = [&] {  // f06_1ab4
        for (int s = 0; s < kColumns; ++s) {
            const uint8_t bits = drums_.patterns[(drums_.kit * 8 + drums_.pattern) * 64 + s] & 0x1F;  // f10_04d0
            for (int d = 0; d < 5; ++d) {
                cells[d][s] = bits >> d & 1;
                drawOpaque(kGridX + 4 * s, rowY(d), cells[d][s] ? 0x2341 : 0x2342);
            }
        }
    };
    auto hidePlayhead = [&] {
        if (gridMode && playhead != -1) duplicateArea(2, 1, 0, 0xC8, 4, 0x9A, playhead, kGridY);
    };
    auto stopped = [&] {  // f06_1c00
        w[37].flags &= ~Widget::kPressed;
        w[38].flags |= Widget::kPressed;
        drawWidget(w[37], false);
        drawWidget(w[38], true);
        playing = false;
    };
    auto releaseChoices = [&] {  // f06_18c4
        for (int i = 0; i < 8; ++i) {
            if (w[i].flags & Widget::kPressed) {
                w[i].flags &= ~Widget::kPressed;
                drawWidget(w[i], false);
            }
            if (w[28 + i].flags & Widget::kPressed) {
                w[28 + i].flags &= ~Widget::kPressed;
                drawWidget(w[28 + i], false);
            }
        }
    };
    auto setHidden = [&](int from, int to, bool hidden) {  // f06_0016 / f06_1624 / f06_1a84
        for (int i = from; i <= to; ++i) {
            if (hidden) w[i].flags |= Widget::kHidden;
            else w[i].flags &= ~Widget::kHidden;
        }
    };
    auto press = [&](int i) {
        w[i].flags |= Widget::kPressed;
        drawWidget(w[i], true);
    };
    auto choosePattern = [&](int p) {  // pattern p's button down, the others up, its name
        press(28 + p);
        for (int i = 28; i <= 35; ++i)
            if (i != 28 + p) {
                w[i].flags &= ~Widget::kPressed;
                drawWidget(w[i], false);
            }
        w[36].label = p == 7 ? dataString(dataWord(0x88A)) : patternName(p);
        drawWidget(w[36], false);
    };
    auto drumsBesideGrid = [&] {  // 54-58, drawn but not live
        for (int d = 0; d < 5; ++d) {
            Widget& x = w[54 + d];
            x.flags &= ~Widget::kHidden;
            x.overlayUp = x.overlayDown = drumIcon(drums_.kit, d);
            drawWidget(x, x.flags & Widget::kPressed);
            x.flags |= Widget::kHidden;
        }
    };
    auto resetTempo = [&] {
        drumTempo(tempo);
        clinicSliders_[0].value = tempo - 13;
        placeSlider(w[39], true);
    };
    drawGrid();

    while (!done) {
        ctx_.pump();
        if ((w[37].flags & Widget::kPressed) && gridMode) {
            // f06_16a0: the playhead (the bar from screen 2) over the step
            // playing; what was under it is kept at (0, C8) on screen 2.
            if (drums_.step >= 0 && drums_.step <= kColumns) {
                const int x = drums_.step == 0 ? 0x1BC : kGridX + 4 * (drums_.step - 1);
                if (x != playhead) {
                    if (playhead != -1) duplicateArea(2, 1, 0, 0xC8, 4, 0x9A, playhead, kGridY);
                    duplicateArea(1, 2, x, kGridY, 4, 0x9A, 0, 0xC8);
                    playhead = x;
                    duplicateArea(2, 1, 0, 0, 4, 0x9A, x, kGridY);
                }
            }
        }
        if ((w[25].flags & Widget::kPressed) && spin) {
            // f06_1c46: colours 2-FD turn one step.
            const std::vector<Rgb> most = colours(2, 0xFC), last = colours(0xFC, 1);
            setColours(most, 3);
            setColours(last, 2);
            spin = false;
        }
        if ((w[26].flags & Widget::kPressed) && chase) {
            // f06_1cc4: each group of five at 10, 15, 20, 25, 30, 35 turns.
            for (int first : {0x10, 0x15, 0x20, 0x25, 0x30, 0x35}) {
                const std::vector<Rgb> four = colours(first, 4), last = colours(first + 4, 1);
                setColours(four, first + 1);
                setColours(last, first);
            }
            chase = false;
        }
        auto swapGroups = [&] {  // f06_17ce: the groups at 10, 20, 30 (and 15, 25, 35) change places
            for (int first : {0x10, 0x15}) {
                const std::vector<Rgb> a = colours(first, 5), b = colours(first + 0x10, 5), c = colours(first + 0x20, 5);
                setColours(b, first);
                setColours(c, first + 0x10);
                setColours(a, first + 0x20);
            }
        };
        auto nextLights = [&] {  // f06_175c: colours 60-65 from the next of four sets
            if (++lightStep == 4) {
                lightStep = 0;
                setColours(lights, 0x60);
            } else {
                setColours(colours((lightStep + 0xB) * 16, 6), 0x60);
            }
        };
        if ((w[27].flags & Widget::kPressed) && swap) {  // f06_1ca8
            nextLights();
            swapGroups();
            swap = false;
        }
        if (ctx_.countdown[2] == 0 && !keysMode) {
            // f06_1654: the step slider follows the playing.
            if (drums_.step >= 1 && drums_.step <= kColumns && drums_.step != lastStep) {
                clinicSliders_[1].value = drums_.step;
                placeSlider(w[41], true);
                lastStep = drums_.step;
            }
            ctx_.countdown[2] = 1;
        }
        if (keysMode && scroll) {
            // f06_19c4: the rows move 4 pixels to the left.
            for (int d = 0; d < 5; ++d) {
                duplicateArea(1, 2, 0xC4, rowY(d), 0xFC, 10, kGridX, rowY(d));
                duplicateArea(1, 2, 0x1C0, rowY(d), 4, 10, 0x1BC, rowY(d));
            }
            for (int d = 0; d < 5; ++d) copyArea(2, 1, kGridX, rowY(d), 0x100, 10);
            scroll = false;
        }
        // The chain: at the end of a pattern, the next pair (or the end).
        if (playing && chainMode && drums_.step == kColumns) {
            drums_.step = 0;
            const int was = chainAt++;
            for (int tries = 0; chainAt <= 11 && (chainKit[chainAt] == -1 || chainPattern[chainAt] == -1); ++tries) {
                ++chainAt;
                if (tries == 12) break;
            }
            if (chainAt <= 11) {
                drumKit(chainKit[chainAt]);
                drumPattern(chainPattern[chainAt]);
                drawLogo(chainX(was), chainY(was), 0x2365);
                drawLogo(chainX(chainAt), chainY(chainAt), 0x2364);
            } else {
                drumStop();
                stopped();
                drawLogo(chainX(was), chainY(was), 0x2365);
            }
        }

        const int r = pollWidgets();
        // A key plays its drum (and marks it on the keyboard view).
        if (const int d = drumForKey(lastKey_); d >= 0) {
            drumPlay(d);
            if (keysMode) {
                const Bitmap& mark = ctx_.bitmap(0x2341);
                ctx_.screens.drawSprite(3, mark, 0x1BC, rowY(d), true);
                copyArea(3, 1, 0x1BC, rowY(d), mark.width, mark.height);
                ctx_.screens.drawSprite(2, mark, 0x1BC, rowY(d), true);
            }
        }
        // A click on the grid turns a step's drum on or off (in the
        // player's pattern).
        if (r < 0 && lastClick_.on && gridMode && lastClick_.x >= kGridX && lastClick_.x <= 0x1BF) {
            int d = -1;
            for (int k = 0; k < 5; ++k)
                if (lastClick_.y >= rowY(k) && lastClick_.y <= rowY(k) + 9) d = k;
            if (d >= 0) {
                const int s = (lastClick_.x - kGridX) / 4;
                drumToggle(d, s);
                if (++cells[d][s] >= 2) cells[d][s] = 0;
                drawOpaque(kGridX + 4 * s, rowY(d), cells[d][s] ? 0x2341 : 0x2342);
                if (!edited) {
                    pattern = 7;
                    choosePattern(7);
                    edited = true;
                }
            }
        }
        if (r < 0) continue;

        if (r <= 7) {
            // A kit.
            hidePlayhead();
            kit = r;
            drumKit(r);
            if (chainMode) {
                Widget& c = w[42 + chainSlot];
                const uint16_t ovUp = c.overlayUp, ovDown = c.overlayDown;
                c.bitmapUp = c.bitmapDown = 0x2363;
                c.overlayUp = c.overlayDown = 0x2000;
                c.mode = 0;
                drawWidget(c, false);
                c.mode = 'c';
                if (chainPatSet) c.overlayUp = ovUp;
                c.overlayDown = ovDown;
                c.bitmapUp = c.bitmapDown = static_cast<uint16_t>(0x231C + r);
                drawWidget(c, false);
                chainKitSet = true;
                chainKit[chainSlot] = r;
                if (chainPatSet) {
                    releaseChoices();
                    chainKitSet = chainPatSet = false;
                    chainSlot = std::min(chainSlot + 1, 11);
                }
            } else {
                edited = false;
                w[8].label = kitName(r);
                drawWidget(w[8], false);
                for (int d = 0; d < 5; ++d) {
                    Widget& x = w[18 + d];
                    x.overlayUp = x.overlayDown = drumIcon(r, d);
                    drawWidget(x, false);
                    if (gridMode || keysMode) {
                        Widget& b = w[54 + d];
                        b.overlayUp = b.overlayDown = drumIcon(r, d);
                        b.flags &= ~Widget::kHidden;
                        drawWidget(b, false);
                        b.flags |= Widget::kHidden;
                    }
                }
                choosePattern(0);
                if (!(w[12].flags & Widget::kPressed)) {
                    drumStop();
                    stopped();
                    drumPattern(0);
                    pattern = 0;
                    if (gridMode) drawGrid();
                    resetTempo();
                }
                if (w[9].flags & Widget::kPressed) drawOpaque(kStageX, kStageY, static_cast<uint16_t>(0x2343 + r));
            }
        }
        auto showChoices = [&] {
            setHidden(28, 35, false);
            press(kit);
            press(28 + pattern);
            drumKit(kit);
        };
        if (r == 9 && !kitMode) {
            showChoices();
            hidePlayhead();
            drumStop();
            stopped();
            setHidden(42, 53, true);
            drawOpaque(kStageX, kStageY, static_cast<uint16_t>(0x2343 + kit));
        }
        if (r == 10 && !gridMode) {
            showChoices();
            drumStop();
            stopped();
            setHidden(42, 53, true);
            drumPattern(pattern);
            fill(kStageX, kStageY, kStageW, kStageH, 0xB);
            drawGrid();
            drumsBesideGrid();
        }
        if (r == 11 && !chainMode) {
            setHidden(28, 35, false);
            hidePlayhead();
            drumStop();
            fill(kStageX, kStageY, kStageW, kStageH, 0xB);
            stopped();
            for (int i = 42; i <= 53; ++i) {  // f06_1974
                w[i].flags &= ~Widget::kHidden;
                drawWidget(w[i], w[i].flags & Widget::kPressed);
            }
            releaseChoices();
            chainSlot = 0;
        }
        if (r == 12 && !keysMode) {
            setHidden(28, 35, true);
            press(kit);
            w[28 + pattern].flags |= Widget::kPressed;
            drumKit(kit);
            hidePlayhead();
            drumStop();
            setHidden(42, 53, true);
            w[38].flags &= ~Widget::kPressed;
            w[37].flags |= Widget::kPressed;
            drawWidget(w[37], true);
            drawWidget(w[38], false);
            fill(kStageX, kStageY, kStageW, kStageH, 0xB);
            for (int s = 0; s <= kColumns; ++s)
                for (int d = 0; d < 5; ++d) drawOpaque(kGridX + 4 * s, rowY(d), 0x2342);
            drumsBesideGrid();
            playing = true;
        }
        if (r >= 9 && r <= 12) {
            kitMode = r == 9;
            gridMode = r == 10;
            chainMode = r == 11;
            keysMode = r == 12;
            resetTempo();
        }
        if (r >= 13 && r <= 17) {
            // A mouth: the drum's next sound (1, 2, 3, 2, 1, ...).
            const int d = r - 13;
            sound[d] += step[d];
            if (sound[d] < 2) sound[d] = 1, step[d] = -step[d];
            if (sound[d] > 2) sound[d] = 3, step[d] = -step[d];
            w[r].bitmapUp = w[r].bitmapDown = static_cast<uint16_t>(0x2329 + sound[d]);
            drawWidget(w[r], false);
            drumVariant(d, sound[d] - 1);
        }
        if (r >= 18 && r <= 22) drumPlay(r - 18);
        if (r == 23) {
            drawWidget(w[23], true);
            done = true;
        }
        if (r == 24) {
            swapGroups();
            nextLights();
        }
        // (The original puts the saved colours back one entry up.)
        if (r == 25) setColours(std::vector<Rgb>(saved.begin(), saved.begin() + 0xFE), 1);
        if (r >= 28 && r <= 35) {
            if (chainMode) {
                Widget& c = w[42 + chainSlot];
                const uint16_t up = c.bitmapUp, down = c.bitmapDown;
                c.bitmapUp = c.bitmapDown = 0x2363;
                drawWidget(c, false);
                c.mode = 'c';
                if (chainKitSet) c.bitmapUp = up, c.bitmapDown = down;
                c.overlayUp = c.overlayDown = static_cast<uint16_t>(0x2331 + r);
                drawWidget(c, false);
                chainPatSet = true;
                chainPattern[chainSlot] = r - 28;
                if (chainKitSet) {
                    releaseChoices();
                    chainKitSet = chainPatSet = false;
                    chainSlot = std::min(chainSlot + 1, 11);
                }
            } else {
                hidePlayhead();
                edited = false;
                w[36].label = patternName(r - 28);
                drawWidget(w[36], false);
                drumStop();
                stopped();
                pattern = r - 28;
                drumPattern(pattern);
                if (gridMode) drawGrid();
            }
        }
        if (r == 37 && !(w[12].flags & Widget::kPressed) && !playing) {
            drums_.step = 0;
            clinicSliders_[1].value = 0;
            placeSlider(w[41], true);
            lastStep = -1;
            if (gridMode) playhead = -1;
            playing = true;
            chainAt = -1;
            if (chainMode) {
                for (int i = 11; i >= 0; --i)
                    if (chainKit[i] != -1 && chainPattern[i] != -1) chainAt = i;
                if (chainAt >= 0) {
                    drumKit(chainKit[chainAt]);
                    drumPattern(chainPattern[chainAt]);
                    drawLogo(chainX(chainAt), chainY(chainAt), 0x2364);
                }
            }
            if (!chainMode || chainAt >= 0) {
                drumStart();
                drumTempo(clinicSliders_[0].value + 13);
            }
        }
        if (r == 38) {
            hidePlayhead();
            if (chainMode && chainAt >= 0) drawLogo(chainX(chainAt), chainY(chainAt), 0x2365);
            drumStop();
            playing = false;
        }
        if (r == 39) drumTempo(clinicSliders_[0].value + 13);
        if (r == 40) {
            // The bomb: a bang, and the player's pattern cleared.
            drumStop();
            if (options_.music) ctx_.platform.sendFm(0x1B);
            stopped();
            if (gridMode) {
                if (playhead != -1) {
                    duplicateArea(2, 1, 0, 0xC8, 4, 0x9A, playhead, kGridY);
                    playhead = -1;
                }
                pattern = 7;
                choosePattern(7);
                for (int d = 0; d < 5; ++d)
                    for (int s = 0; s < kColumns; ++s)
                        if (cells[d][s]) {
                            drumToggle(d, s);
                            drawOpaque(kGridX + 4 * s, rowY(d), 0x2342);
                            cells[d][s] = 0;
                        }
            }
            if (chainMode && chainAt >= 0) drawLogo(chainX(chainAt), chainY(chainAt), 0x2365);
        }
        if (r == 41) drums_.step = clinicSliders_[1].value + 1 == kColumns ? 0 : clinicSliders_[1].value + 1;
        if (r >= 42 && r <= 53) {
            // A pair of the chain taken out; the next choices go there.
            Widget& c = w[r];
            c.bitmapUp = c.bitmapDown = 0x2363;
            c.overlayUp = c.overlayDown = 0x2000;
            c.mode = 0;
            drawWidget(c, false);
            c.mode = 'c';
            chainSlot = r - 42;
            chainKit[chainSlot] = chainPattern[chainSlot] = -1;
            releaseChoices();
        }
        if (r == 59 || r == 60) {
            Slider& t = clinicSliders_[0];
            t.value = std::clamp(t.value + (r == 59 ? -8 : 8), 0, 0x57);
            placeSlider(w[39], true);
            drumTempo(t.value + 13);
        }
        clearInput();
    }
    ctx_.timer.setPeriodic(kClinicSlot, 0, nullptr);
    return 0;
}

}  // namespace edison
