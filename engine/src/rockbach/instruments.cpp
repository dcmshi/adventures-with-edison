// WINMAIN's segment 8: the Instrument Room. Sixteen instruments, each with
// a picture, two pages about it (INSTINFO.HI), its sound (and its waveform,
// with a marker that follows the sound), its range on the keyboard and its
// place on the gauge.

#include <algorithm>
#include "formats/paths.h"
#include <fstream>

#include "rockbach/rockbach.h"

namespace edison {
namespace {

constexpr int kWaveX = 0xBE, kWaveY = 0xAC, kWaveW = 0x104, kWaveH = 0x50;
constexpr int kWaveEnd = 0x1C1;  // DS:64BA: the waveform's right end
constexpr int kAnimX = 0x204, kAnimY = 0x156, kAnimW = 0x7A, kAnimH = 0x3A;
constexpr int kMarkerSlot = 7, kAnimSlot = 8;  // g08_088c (15/s), g08_0016

std::vector<std::string> readText(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream in(findPath(path), std::ios::binary);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line.substr(0, 0x4F));
    }
    return lines;
}

}  // namespace

void RockBach::instrumentWidgets() {
    // f08_002c: 21 widgets.
    auto w = [](int x0, int y0, int x1, int y1, uint16_t flags, uint16_t down, uint16_t up, uint16_t ov = 0x2000,
                int group = 0) {
        Widget r{x0, y0, x1, y1, flags};
        r.group = group;
        r.bitmapDown = down;
        r.bitmapUp = up;
        r.overlayUp = r.overlayDown = ov;
        r.hotkey = 1;
        return r;
    };
    std::vector<Widget>& l = instrumentWidgets_;
    l.clear();
    static const int kColumn[4] = {20, 92, 480, 552};
    for (int i = 0; i < 16; ++i)  // 0-15 the instruments
        l.push_back(w(kColumn[i / 4], 22 + 54 * (i % 4), kColumn[i / 4] + 69, 73 + 54 * (i % 4), i == 5 ? 0x142 : 0x140,
                      0x2290, 0x2291, static_cast<uint16_t>(0x2280 + i), 9));
    l.push_back(w(402, 140, 443, 163, 0x100, 0x2273, 0x2274));   // 16 its sound
    l.push_back(w(56, 272, 587, 295, 0x104, 0x2278, 0x2278));    // 17 the keyboard
    l.push_back(w(196, 140, 237, 163, 0x100, 0x2275, 0x2276));   // 18 the other page
    l.push_back(w(574, 304, 623, 327, 0x700, 0x2279, 0x227A, 0x2000, 11));  // 19
    l.push_back(w(579, 316, 618, 373, 0x100, 0x227B, 0x227C));   // 20 exit
}

void RockBach::instrumentText(const std::vector<std::string>& text, int i, int page, bool viaScreen2) {
    // f08_0802: block 2i + page of INSTINFO.HI in the box at (182, 304).
    textBox(text, TextBox{i * 2 + page, true, 0x2277, 0, 0xB6, 0x130, 0x17E, 0x5A, 0xB6 + 0x1F, 0x130 + 0xA, 0}, viaScreen2);
}

void RockBach::showInstrument(int i, const std::vector<std::string>& text, int page, bool viaScreen2) {
    // f08_0e0e: the waveform (f08_0c0e), the range on the keyboard
    // (f08_0a1a), the gauge (f08_0940), the page, the picture.
    select(2);
    for (int y = kWaveY; y < kWaveY + kWaveH; ++y)  // f37_0c82
        std::fill_n(ctx_.screens[2].pixels.begin() + static_cast<size_t>(y) * Screen::kWidth + kWaveX, kWaveW, uint8_t{0xE2});
    // f08_0c0e: the instrument's WAV (sound 600D + i), one line a column:
    // the sample (8 bits, from after the header) halved around 40, up from
    // y 248 (no higher than 173). The marker moves by DS:<table>/size every
    // 1/15 s.
    static const uint32_t kLength[16] = {0x25D78, 0x2E630, 0x3D090, 0x249F0, 0x3D090, 0x3D090, 0x3D090, 0x3D090,
                                         0x249F0, 0x3D090, 0x33450, 0x3D090, 0x29810, 0x3D090, 0x3E418, 0x445C0};
    sample_ = options_.music ? soundData(static_cast<uint16_t>(0x600D + i)) : std::vector<uint8_t>{};
    const uint32_t size = sample_.size() > 0 ? static_cast<uint32_t>(sample_.size()) : 0x100;
    const size_t start = sample_.size() > 0x16 && (sample_[0x14] & 1) ? 0x2C : 0x2A;
    const int width = kWaveEnd - kWaveX - 6;
    const uint32_t step = size / static_cast<uint32_t>(width);
    markerStep_ = static_cast<int>(kLength[i] / size);
    if (options_.music)
        for (int x = 0; x < width; ++x) {
            const size_t at = start + static_cast<size_t>(x) * step;
            const int s = at < sample_.size() ? sample_[at] : 0x80;
            const int y = std::max(0xAD, 0xF8 - (((s - 0x80) >> 1) + 0x28));
            const int cx = kWaveX + x + 3;
            for (int yy = std::min(y, 0xD4); yy <= std::max(y, 0xD4); ++yy)
                ctx_.screens[2].pixels[static_cast<size_t>(yy) * Screen::kWidth + cx] = 0xD0;  // f37_29e4
        }
    if (viaScreen2) copyArea(2, 1, kWaveX, kWaveY, kWaveW, kWaveH);
    // f08_0a1a: keys 10-44 in colour 45, the instrument's range in 46.
    static const int kRange[16][2] = {{0x15, 0x1E}, {0xA, 0x1B}, {5, 0x18},  {0x13, 0x12}, {0xC, 0x15}, {0x18, 0x16},
                                      {9, 0x19},    {0x12, 0x1A}, {9, 0x1A}, {9, 0x27},    {1, 0x34},   {5, 0x17},
                                      {1, 0},       {0x17, 0x14}, {0x11, 0x1B}, {1, 0}};
    const Rgb off = ctx_.screens[1].palette[0x45], on = ctx_.screens[1].palette[0x46];
    setColours(std::vector<Rgb>(0x35, off), 0x10);
    if (kRange[i][1] > 0) setColours(std::vector<Rgb>(kRange[i][1], on), 0x10 + kRange[i][0]);
    // f08_0940: the gauge's seven parts in colour 8, the instrument's in 9.
    static const int kGauge[15] = {1, 3, 4, 6, 6, 5, 5, 5, 6, 2, 7, 6, 7, 5, 2};
    const Rgb dim = ctx_.screens[1].palette[8], lit = ctx_.screens[1].palette[9];
    for (int k = 1; k <= 7; ++k) setColours({dim}, k);
    if (i != 15) setColours({lit}, kGauge[i]);
    instrumentText(text, i, page, viaScreen2);
    if (viaScreen2) select(2);
    drawOpaque(0xBE, 0xC, 0x2293);
    drawLogo(0xBE, 0xC, static_cast<uint16_t>(0x2263 + i));
    std::vector<Widget>& w = instrumentWidgets_;
    drawWidget(w[16], w[16].flags & Widget::kPressed);
    drawWidget(w[18], w[18].flags & Widget::kPressed);
    if (viaScreen2) {
        select(1);
        copyArea(2, 1, 0xBE, 0xC, 0x104, 0x9C);
    }
}

int RockBach::instrumentRoom() {
    // f08_0f2e.
    std::vector<Widget>& w = instrumentWidgets_;
    int instrument = -1, page = 0;   // [bp-40], [bp-E]
    int offset = 0;                  // [bp-48]: the marker
    bool animating = false;          // [bp-6]
    int frame = 0;                   // [bp-4]
    instrumentWidgets();
    blackout();
    backdrop(0x1009);
    show(2);
    const std::vector<std::string> text = readText(options_.cdDir + "/INSTINFO.HI");
    if (text.empty()) logLine("Rock and Bach: File: instinfo.hi Not found");
    select(2);
    int marker = saveArea(0, 0, 2, 2);
    drawOpaque(0xBE, 0xAC, 0x227D);
    drawOpaque(0xC, 0x130, 0x2292);
    drawOpaque(0xB6, 0x130, 0x2277);
    drawOpaque(0xBE, 0xC, 0x2293);
    initWidgets(w, {0xFB, 0xFC, 0xFD, 0xFE});
    const int wave = saveArea(kWaveX, kWaveY, kWaveW, kWaveH);
    showInstrument(5, text, page, false);
    instrument = 5;
    copyArea(2, 1, 0, 0, Screen::kWidth, Screen::kHeight);
    select(1);
    int anim = saveArea(kAnimX, kAnimY, kAnimW, kAnimH);
    bool tick = false, animDue = false;  // [8E18], [8D10]
    ctx_.timer.setPeriodic(kMarkerSlot, 15, [&tick] { tick = true; });
    auto play = [&] {  // f48_0172: the sample from the start
        offset = 0;
        if (!sample_.empty() && sample_.size() <= 0x10000) ctx_.platform.playWav(sample_);
    };
    clearInput();
    for (bool done = false; !done;) {
        const int r = pollWidgets();
        if (animating && animDue) {
            // The conductor (22E3 + n) at the bottom right, then the page again.
            if (frame < 21) {
                select(2);
                restoreArea(anim);
                anim = saveArea(kAnimX, kAnimY, kAnimW, kAnimH);
                drawLogo(kAnimX, kAnimY, static_cast<uint16_t>(0x22E3 + frame));
                select(1);
                copyArea(2, 1, kAnimX, kAnimY, kAnimW, kAnimH);
                ++frame;
            } else {
                animating = false;
                frame = 0;
                ctx_.timer.setPeriodic(kAnimSlot, 0, nullptr);
                select(1);
                restoreArea(anim);
                instrumentText(text, instrument, page, true);
                anim = saveArea(kAnimX, kAnimY, kAnimW, kAnimH);
            }
            animDue = false;
        }
        // A click on the picture (not on its buttons) plays the sound.
        if (r < 0 && lastClick_.on && lastClick_.x >= 0xBE && lastClick_.x <= 0x1C2 && lastClick_.y >= 0xC &&
            lastClick_.y <= 0xA8 && !(lastClick_.x >= 0xC4 && lastClick_.x <= 0xEE && lastClick_.y >= 0x8C && lastClick_.y <= 0xA4))
            play();
        if (tick && ctx_.platform.wavPlaying()) {
            // f08_08a2: the marker (227E, 1 x 78) over the waveform.
            offset = std::min(offset, kWaveEnd - kWaveX - 6);
            const int x = offset + kWaveX + 3 - 1;
            restoreArea(marker);
            marker = saveArea(x, 0xAD, 1, 0x4E);
            drawOpaque(x, 0xAD, 0x227E);
            offset += markerStep_;
            tick = false;
        }
        if (r >= 0 && r <= 15) {
            ctx_.platform.stopWav();  // f48_0000
            page = page + 1 >= 2 ? 0 : page + 1;  // f08_07d6: the page turns too, as in the original
            showInstrument(r, text, page, true);
            instrument = r;
        }
        if (r == 16 || r == 17) {
            play();
            if (r == 17) {
                animDue = false;
                animating = true;
                frame = 0;
                restoreArea(anim);
                anim = saveArea(kAnimX, kAnimY, kAnimW, kAnimH);
                ctx_.timer.setPeriodic(kAnimSlot, 5, [&animDue] { animDue = true; });
            }
        }
        if (r == 18) {
            page = page + 1 >= 2 ? 0 : page + 1;
            instrumentText(text, instrument, page, true);
        }
        if (r == 20) done = true;
    }
    ctx_.timer.setPeriodic(kAnimSlot, 0, nullptr);
    ctx_.timer.setPeriodic(kMarkerSlot, 0, nullptr);
    freeArea(anim);
    freeArea(marker);
    freeArea(wave);
    return 0;
}

}  // namespace edison
