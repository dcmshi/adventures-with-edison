// WINMAIN's segment 7 (Harmony Hall: a key and a kind of chord, shown on a
// staff, a guitar's neck and a piano, played by a band in one of eight
// styles) and segment 11 (its player, in ADLIB3.DLL).

#include <algorithm>
#include <array>

#include "rockbach/rockbach.h"

#include "audio/artech_fm_driver.h"

namespace edison {
namespace {

// Timer slots (9 is the countdowns'): g07_183a (10/s), g07_1868 (15/s), g07_1824.
constexpr int kSlowSlot = 7, kFastSlot = 8, kRiffSlot = 5;
constexpr uint16_t kNoPart = 0x1610;

}  // namespace

// --- the player (segment 11) ----------------------------------------------------

void RockBach::harmonyCommand(const std::vector<uint8_t>& bytes) {
    // f11_04ee: the bytes over sound B, which is then started.
    if (!options_.music) return;
    ctx_.platform.withFm([&bytes](ArtechFmDriver& d) {
        const uint16_t to = d.soundAddress(0xB);
        for (size_t k = 0; k < bytes.size(); ++k) d.poke(static_cast<uint16_t>(to + k), bytes[k]);
        d.sendSound(0xB);
    });
}

void RockBach::harmonyReset() {
    // f11_0000: the default parts, silent volumes, C major, the first
    // style, tempo C0.
    if (!options_.music) return;
    harmony_ = HarmonyPlayer{};
    harmonyTempo(0xC0);
}

void RockBach::harmonyStop() {
    // f11_03a2: sound 0; then 0.1 s.
    if (!options_.music) return;
    ctx_.platform.sendFm(0);
    ctx_.countdown[3] = 1;
    while (ctx_.countdown[3] != 0) ctx_.pump();
}

void RockBach::harmonyStyle(int s) {
    // f11_00b2: styles 1-12 are 0-11 (anything else 0).
    harmony_.style = s >= 1 && s <= 12 ? s - 1 : 0;
}

void RockBach::harmonyKey(int key) {
    // f11_00e4.
    harmony_.key = key;
    harmonyRiffs();
}

void RockBach::harmonyChord(int chord) {
    // f11_0100.
    harmony_.chord = chord;
    harmonyRiffs();
}

void RockBach::harmonyPart(int part, int choice) {
    // f11_011c: a part's record from DS:1700 (4 parts x 4 choices).
    harmony_.parts[part] = dataWord(static_cast<uint16_t>(0x1700 + part * 8 + choice * 2));
    if (part != 0) harmonyRiffs();
}

void RockBach::harmonyStart() {
    // f11_0288: sound 9 (11 for the other styles), then every part's
    // channels' volumes ("AC channel volume") through sound B.
    if (!options_.music) return;
    ctx_.platform.sendFm(harmony_.style == 0 ? 9 : 0x11);
    std::vector<uint8_t> bytes = {0x09, 0x00};
    for (int part = 0; part < 4; ++part) {
        const uint16_t p = harmony_.parts[part];
        for (int i = 0; i < dataWord(p); ++i) bytes.insert(bytes.end(), {0xAC, data_[p + 4 + i], harmony_.volume[part]});
    }
    bytes.push_back(0x88);
    harmonyCommand(bytes);
}

void RockBach::harmonyVolume(int part, int volume) {
    // f11_03e0.
    harmony_.volume[part] = static_cast<uint8_t>(volume);
    if (!options_.music) return;
    const uint16_t p = harmony_.parts[part];
    std::vector<uint8_t> bytes = {0x09, 0x00};
    for (int i = 0; i < dataWord(p); ++i) bytes.insert(bytes.end(), {0xAC, data_[p + 4 + i], static_cast<uint8_t>(volume)});
    bytes.push_back(0x88);
    harmonyCommand(bytes);
}

void RockBach::harmonyTempo(int t) {
    // f11_04a2.
    harmonyCommand({0x09, 0x00, 0xA6, static_cast<uint8_t>(t), 0x88});
}

void RockBach::harmonyRiffs() {
    // f11_05e2: sounds C-10 get the style's riffs: three of the rhythm
    // part's, the lead's and the bass's (none for an empty part); then,
    // unless it's C major or one of the last styles, every note moves to
    // the key (DS:1750) and the chord (DS:175C: 12 semitones each).
    if (!options_.music) return;
    const HarmonyPlayer& h = harmony_;
    const std::vector<uint8_t>& data = data_;
    ctx_.platform.withFm([&h, &data](ArtechFmDriver& d) {
        for (int i = 0; i < 5; ++i) {
            const uint16_t part = h.parts[i == 3 ? 2 : i == 4 ? 3 : 1];
            if (part == kNoPart) continue;
            const uint8_t source = data[part + 7 + h.style * 3 + (i < 3 ? i : 0)];
            const uint16_t to = d.soundAddress(static_cast<uint16_t>(0xC + i)), from = d.soundAddress(source);
            for (int k = 0; k < 0x100; ++k) d.poke(static_cast<uint16_t>(to + k), d.peek(static_cast<uint16_t>(from + k)));
            if ((h.key == 0 && h.chord == 0) || h.style >= 9) continue;
            // f11_0790: note bytes (to 7B; 4 bytes an event) moved,
            // commands (2 bytes) skipped, until an 84 or 88.
            uint16_t p = static_cast<uint16_t>(to + 0xC);
            for (int guard = 0; guard < 0x100; ++guard) {
                const uint8_t b = d.peek(p);
                if (b > 0x7B) {
                    p = static_cast<uint16_t>(p + 2);
                } else {
                    int note = (b >> 4) * 12 + (b & 0xF);
                    if (b != 0x30) note += static_cast<int8_t>(data[0x175C + h.chord * 12 + (b & 0xF)]);
                    note += static_cast<int8_t>(data[0x1750 + h.key]);
                    d.poke(p, static_cast<uint8_t>((note / 12) << 4 | note % 12));
                    p = static_cast<uint16_t>(p + 4);
                }
                const uint8_t last = d.peek(static_cast<uint16_t>(p - 2));
                if (last == 0x84 || last == 0x88) break;
            }
        }
    });
}

// --- Harmony Hall (segment 7) ------------------------------------------------------

void RockBach::harmonyWidgets() {
    // f07_0000: 45 widgets.
    auto w = [](int x0, int y0, int x1, int y1, uint16_t flags, uint16_t down, uint16_t up, uint16_t ov = 0x2000,
                int group = 0) {
        Widget r{x0, y0, x1, y1, flags};
        r.group = group;
        r.bitmapDown = down;
        r.bitmapUp = up;
        r.overlayUp = r.overlayDown = ov;
        return r;
    };
    std::vector<Widget>& l = harmonyWidgets_;
    l.clear();
    l.push_back(w(586, 284, 617, 385, 0x100, 0x23C5, 0x23C6));  // 0 exit
    for (int i = 0; i < 12; ++i)                                // 1-12 the keys
        l.push_back(w(16 + 52 * (i % 3), 32 + 58 * (i / 3), 61 + 52 * (i % 3), 83 + 58 * (i / 3), i ? 0x140 : 0x142,
                      0x2369, 0x236A, static_cast<uint16_t>(0x236B + i), 1));
    for (int i = 0; i < 8; ++i)  // 13-20 the chords
        l.push_back(w(i < 4 ? 470 : 552, 32 + 58 * (i % 4), (i < 4 ? 470 : 552) + 75, 83 + 58 * (i % 4),
                      i ? 0x140 : 0x142, 0x2377, 0x2378, static_cast<uint16_t>(0x2379 + i), 2));
    l.push_back(w(186, 226, 245, 255, 0x140, 0x2009, 0x200A, 0x2000, 11));  // 21 go
    l.push_back(w(376, 226, 455, 255, 0x142, 0x200B, 0x200C, 0x2000, 11));  // 22 stop
    for (int i = 0; i < 8; ++i)                                           // 23-30 the styles
        l.push_back(w(18 + 38 * (i % 4), 332 + 34 * (i / 4), 49 + 38 * (i % 4), 357 + 34 * (i / 4), i ? 0x140 : 0x142,
                      0x2381, 0x2382, static_cast<uint16_t>(0x2383 + i), 4));
    static const int kFader[4] = {202, 276, 424, 350};  // 31-34 drums, rhythm, lead, bass
    for (int i = 0; i < 4; ++i) {
        Widget f = w(kFader[i], 290, kFader[i] + 13, 377, 0x110, 0x23C4, 0x23C4, 0x238C);
        f.slider = &harmonySliders_[i];
        l.push_back(f);
    }
    l.push_back(w(52, 278, 127, 315, 0x104, 0x238E, 0x238F, 0x2000, 8));   // 35 riff
    l.push_back(w(248, 226, 373, 255, 0x124, 0x2390, 0x2390, 0x2000, 8));  // 36 the style's name
    for (int i = 0; i < 4; ++i)                                          // 37-40 the lights
        l.push_back(w(518 + 26 * (i % 2), 286 + 26 * (i / 2), 537 + 26 * (i % 2), 305 + 26 * (i / 2), 0x500,
                      static_cast<uint16_t>(0x2392 + i), static_cast<uint16_t>(0x2396 + i)));
    l.push_back(w(518, 346, 563, 375, 0x100, 0x239A, 0x239B));  // 41 reset
    Widget tempo = w(264, 16, 387, 43, 0x108, 0x239C, 0x239C, 0x212D);  // 42 tempo
    tempo.slider = &harmonySliders_[4];
    tempo.mode = 'c';
    tempo.hotkey = 0;
    l.push_back(tempo);
    Widget slower = w(192, 2, 245, 35, 0x104, 0x23A1, 0x23A1, 0x239D);  // 43, 44
    slower.overlayDown = 0x239E;
    Widget faster = w(390, 2, 443, 35, 0x104, 0x23A2, 0x23A2, 0x239F);
    faster.overlayDown = 0x23A0;
    for (int i = 0; i < 42; ++i) l[i].hotkey = 1;
    slower.hotkey = faster.hotkey = 0;
    l.push_back(slower);
    l.push_back(faster);
}

void RockBach::showChord(int key, int chord) {
    // f07_1278: on screen 2, the guitar's fingers (DS:0D12 into the places
    // at DS:1072), the key's flats (23A8), the notes on the staff (DS:0A12
    // into DS:10EA; the note's sprite by its place); then the piano (its
    // keys are colours 1-23: f07_1048 puts them back, f07_112a lights the
    // chord's, DS:0892).
    static const int kFlats[12] = {1, 0, 0, 0, 0, 0, 0, 2, 2, 2, 1, 1};
    static const uint8_t kWhite[23] = {1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 1};
    select(2);
    drawOpaque(0xBB, 0x2B, 0x238D);
    auto at = [this](uint16_t table, int n) {
        return std::pair<int, int>(static_cast<int16_t>(dataWord(static_cast<uint16_t>(table + 4 * n))),
                                   static_cast<int16_t>(dataWord(static_cast<uint16_t>(table + 4 * n + 2))));
    };
    for (int i = 0; i < 9; ++i) {
        const int8_t n = static_cast<int8_t>(data_[0xD12 + key * 0x48 + chord * 9 + i]);
        if (n < 0) continue;
        const auto [x, y] = at(0x1072, n);
        drawScaledCentred(x, y, 0x100, 0x100, 0x238B);
    }
    if (kFlats[key] > 0) drawOpaque(0xFA, 0x5F, 0x23A8);
    if (kFlats[key] == 2) drawOpaque(0xFA, 0x65, 0x23A8);
    for (int i = 0; i < 8; ++i) {
        const int8_t n = static_cast<int8_t>(data_[0xA12 + key * 64 + chord * 8 + i]);
        if (n < 0) continue;
        uint16_t id = 0;
        if (n <= 0xD) id = 0x23A3;
        else if (n <= 0x1B) id = 0x23A5;
        else if (n <= 0x29) id = 0x23A4;
        else if (n <= 0x37) id = 0x23A3;
        else if (n <= 0x3A) id = 0x23A6;
        else if (n <= 0x3C) id = 0x23A7;
        else continue;
        const auto [x, y] = at(0x10EA, n);
        drawScaledCentred(x, y, 0x100, 0x100, id);
    }
    const Rgb white = ctx_.displayPalette[0x18], black = ctx_.displayPalette[0x19];
    for (int k = 0; k < 23; ++k) setColours({kWhite[k] ? white : black}, k + 1);  // f07_1048
    select(1);
    copyArea(2, 1, 0xBB, 0x2B, 0x10E, 0xB2);
    const Rgb litWhite = ctx_.displayPalette[0x1A], litBlack = ctx_.displayPalette[0x1B];
    for (int i = 0; i < 4; ++i) {
        const int8_t n = static_cast<int8_t>(data_[0x892 + key * 32 + chord * 4 + i]);
        if (n >= 0) setColours({kWhite[n] ? litWhite : litBlack}, n + 1);
    }
}

int RockBach::harmonyHall() {
    // f07_1908.
    static const uint8_t kChordType[8] = {0, 1, 4, 6, 2, 3, 5, 7};  // the chord buttons' order for the player
    static const uint16_t kStyles[8] = {0x11E7, 0x11F6, 0x1201, 0x120C, 0x1219, 0x1225, 0x122F, 0x123C};
    std::vector<Widget>& w = harmonyWidgets_;
    int key = 0, chord = 0;  // [bp-32], [bp-28]
    bool playing = false;    // [bp-30]
    int riff = -1, hold = 0;  // [bp-26], [bp-1A]: the riff button's frames
    harmonyWidgets();
    w[36].flags |= Widget::kLabel;
    w[36].label = dataString(kStyles[0]);
    select(2);
    blackout();
    backdrop(0x100C);
    show(2);
    for (int i = 0; i < 4; ++i) harmonySliders_[i] = Slider{0, 0, 0x48, 0xE, 0xC, 0xB, 5, 0x59};
    harmonySliders_[4] = Slider{0, 0x46, 0x94, 0xC, 0xC, 8, 6, 0x72};
    initWidgets(w, {0xAB, 0xA9, 0xA8, 0xAA});
    drawOpaque(0xBB, 0x2B, 0x238D);
    showChord(key, chord);
    show(2);
    const std::vector<Rgb> saved(ctx_.displayPalette.begin() + 0x1C, ctx_.displayPalette.begin() + 0x1C + 0xE4);  // DS:83DE
    select(1);
    clearInput();
    harmonyKey(0);
    harmonyChord(kChordType[chord]);
    int riffArea = saveArea(0x20, 0x104, 0x5C, 0x42);
    std::array<bool, 4> due{};  // [88C..88F]
    bool riffDue = false;       // [890]
    ctx_.timer.setPeriodic(kSlowSlot, 10, [&due] { due[0] = due[1] = true; });
    ctx_.timer.setPeriodic(kFastSlot, 15, [&due] { due[2] = due[3] = true; });
    auto turn = [this](int a, int b) {  // f07_1896: colours a-b turn one step
        const std::vector<Rgb> most(ctx_.displayPalette.begin() + a, ctx_.displayPalette.begin() + b);
        const Rgb last = ctx_.displayPalette[b];
        setColours(most, a + 1);
        setColours({last}, a);
    };
    for (bool done = false; !done;) {
        if (riff >= 0 && riffDue) {
            // The riff button's frames (23A9 + n), holding at some.
            select(2);
            restoreArea(riffArea);
            riffArea = saveArea(0x20, 0x104, 0x5C, 0x42);
            drawLogo(0x20, 0x104, static_cast<uint16_t>(0x23A9 + riff));
            select(1);
            copyArea(2, 1, 0x20, 0x104, 0x5C, 0x42);
            ++riff;
            if ((riff == 7 || riff == 12 || riff == 14 || riff == 16 || riff == 18 || riff == 24) && hold < 3) {
                --riff;
                ++hold;
            } else {
                hold = 0;
            }
            if (riff >= 0x1B) {
                restoreArea(riffArea);
                riffArea = saveArea(0x20, 0x104, 0x5C, 0x42);
                ctx_.timer.setPeriodic(kRiffSlot, 0, nullptr);
                hold = 0;
                riff = -1;
            }
            riffDue = false;
        }
        const int r = pollWidgets();
        // The lights: four ranges of colours turning while their button is on.
        static const int kRange[4][2] = {{0xB0, 0xC5}, {0xF0, 0xF9}, {0x90, 0x96}, {0xA0, 0xA8}};
        for (int i = 0; i < 4; ++i)
            if (due[i] && (w[37 + i].flags & Widget::kPressed)) {
                due[i] = false;
                turn(kRange[i][0], kRange[i][1]);
            }
        if (r == 0) done = true;
        if (r >= 1 && r <= 12) {
            harmonyKey(r - 1);
            key = r - 1;
            showChord(key, chord);
        }
        if (r >= 13 && r <= 20) {
            chord = r - 13;
            showChord(key, chord);
            harmonyChord(kChordType[chord]);
        }
        if (r == 21) {
            playing = true;
            harmonyPart(1, 0);
            harmonyStart();
            for (int part = 0; part < 4; ++part) {
                harmonyVolume(part, 0);
                waitCountdown(2);
            }
        }
        if (r == 22) {
            playing = false;
            harmonyStop();
        }
        if (r >= 23 && r <= 30) {
            // A style: back to C major.
            harmonyStop();
            harmonyStyle(r - 22);
            harmonyPart(1, 0);
            pieceTempo(harmonySliders_[4].value + 0x7A);  // (the Music Library's tempo, as in the original)
            w[21].flags &= ~Widget::kPressed;  // f07_17e0
            w[22].flags |= Widget::kPressed;
            harmonyStop();
            drawWidget(w[21], false);
            drawWidget(w[22], true);
            w[36].label = dataString(kStyles[r - 23]);
            drawWidget(w[36], w[36].flags & Widget::kPressed);
            w[1].flags |= Widget::kPressed;
            drawWidget(w[1], true);
            for (int i = 2; i <= 12; ++i) {
                w[i].flags &= ~Widget::kPressed;
                drawWidget(w[i], false);
            }
            harmonyKey(0);
            key = 0;
            w[13].flags |= Widget::kPressed;
            drawWidget(w[13], true);
            for (int i = 14; i <= 20; ++i) {
                w[i].flags &= ~Widget::kPressed;
                drawWidget(w[i], false);
            }
            chord = 0;
            showChord(key, chord);
            harmonyChord(kChordType[chord]);
        }
        if (r >= 31 && r <= 34) harmonyVolume(r - 31, harmonySliders_[r - 31].value);
        if (r == 41) {
            // Reset: the lights off, their colours back.
            for (int i = 37; i <= 40; ++i) {
                w[i].toggle = false;
                w[i].flags &= ~Widget::kPressed;
                drawWidget(w[i], false);
            }
            setColours(std::vector<Rgb>(saved.begin(), saved.begin() + 0xE2), 0x1C);
        }
        if (r == 42) harmonyTempo(harmonySliders_[4].value + 0x7A);
        if (r == 43 || r == 44) {
            Slider& t = harmonySliders_[4];
            t.value = std::clamp(t.value + (r == 43 ? -8 : 8), 0, 0x85);
            placeSlider(w[42], true);
            harmonyTempo(t.value + 0x7A);
        }
        if (r == 35) {
            ctx_.timer.setPeriodic(kRiffSlot, 5, [&riffDue] { riffDue = true; });
            riff = hold = 0;
        }
    }
    (void)playing;
    freeArea(riffArea);
    ctx_.timer.setPeriodic(kSlowSlot, 0, nullptr);
    ctx_.timer.setPeriodic(kFastSlot, 0, nullptr);
    ctx_.timer.setPeriodic(kRiffSlot, 0, nullptr);
    return 0;
}

}  // namespace edison
