// WMAIN.EXE segment 15: the professor's lessons (rooms 505-510,
// f38_0fb5): their scripts and bubbles.

#include "science/science.h"

#include <algorithm>

namespace edison {

namespace {

// The bubble's pieces in GRAFX.DAT: edges stretched to the box (top 1,
// bottom 2, right 3, left 4) and the tails (5, 7, 6 for tail types 0-2).
constexpr uint16_t kTop = 1, kBottom = 2, kRight = 3, kLeft = 4;

// The MORE button (f15_0fee: the lesson's rectangle).
constexpr int kMoreX = 0x210, kMoreY = 0x172, kMoreW = 0x58, kMoreH = 0x18;

// The lessons' scripts (each class's +44 runner: g15_1260, g15_1796,
// g15_1c46, g15_202d, g15_2433, g15_2858): a bubble each step, at an
// anchor with its tail, the width, the text and the narration the next
// bubble plays. The anchors: A (1E0, AE) and B (21C, 7C) by the
// professor, C (A6, 11A) by Edison, the others lessons 9 and 10's own.
struct Step {
    int x, y, tail, width;
    uint16_t text, nextSound;
};
// The lessons' two animated objects (the professor: f15_0bde, f15_0f19,
// f15_0e43; Edison: the builders' second child; each an animation,
// f15_0003, its x, y and frame generators given by f15_00dc, f15_00f3
// and f15_010a, added to the lesson by f15_09a6), each a sprite cycling
// through `count` frames over `period` ticks (f16_010e on f16_008f: frame
// = (tick mod period) x count / period) at a fixed place (f16_0000).
struct Anim {
    uint16_t sprite;
    int count, period, x, y;
};
struct Lesson {
    uint16_t picture, firstSound;  // f15_076a's picture, the builder's [171A]
    int nextRoom;                  // +138: the room after (-1 none)
    std::vector<Step> steps;
    Anim professor, edison;
};
constexpr Anim kProfessor{0x13BB, 3, 50, 0x1F4, 0x82}, kEdison{0x13AF, 3, 100, 0x60, 0xFC};
const Lesson kLessons[6] = {
    {0x2003, 0x6169, 1,  // 5: f15_0fee
     {{0x1E0, 0xAE, 2, 200, 0x75F1, 0x616C}, {0x1E0, 0xAE, 2, 200, 0x75F4, 0x6173}, {0x1E0, 0xAE, 2, 200, 0x75FB, 0x6174},
      {0x21C, 0x7C, 2, 200, 0x75FC, 0x616D}, {0x1E0, 0xAE, 2, 230, 0x75F5, 0x6175}, {0x21C, 0x7C, 2, 200, 0x75FD, 0x6176},
      {0x1E0, 0xAE, 2, 200, 0x75FE, 0x616E}, {0x21C, 0x7C, 2, 200, 0x75F6, 0}}, kProfessor, kEdison},
    {0x2004, 0x6177, 50,  // 6: f15_1524
     {{0x1E0, 0xAE, 2, 200, 0x75FF, 0x6178}, {0x21C, 0x7C, 2, 200, 0x7600, 0x6179}, {0x1E0, 0xAE, 2, 200, 0x7601, 0x617A},
      {0x1E0, 0xAE, 2, 240, 0x7602, 0x617B}, {0x1E0, 0xAE, 2, 200, 0x7603, 0x6172}, {0x21C, 0x7C, 2, 200, 0x75FA, 0}}, kProfessor, kEdison},
    {0x2003, 0x617C, 2,  // 7: f15_19d4
     {{0x1E0, 0xAE, 2, 200, 0x7604, 0x617D}, {0x1E0, 0xAE, 2, 240, 0x7605, 0x6172}, {0x21C, 0x7C, 2, 200, 0x75FA, 0}}, kProfessor, kEdison},
    {0x2003, 0x617E, 57,  // 8: f15_1dbb
     {{0x1E0, 0xAE, 2, 200, 0x7606, 0x617F}, {0x1E0, 0xAE, 2, 200, 0x7607, 0x618A}, {0xA6, 0x11A, 0, 200, 0x7612, 0}}, kProfessor, kEdison},
    {0x2006, 0x6180, 67,  // 9: f15_21a2
     {{0x64, 0xFA, 0, 200, 0x7608, 0x6181}, {0x160, 0x12C, 2, 200, 0x7609, 0x6182}, {0x64, 0xFA, 0, 200, 0x760A, 0}}, {0x13C7, 6, 50, 0x0E, 0xD2}, {0x13B8, 3, 100, 0x16C, 0x110}},
    {0x2005, 0x6183, -1,  // 10: f15_2591 (then the game's won: science.cpp)
     {{0x212, 0xBA, 2, 200, 0x760B, 0x6184}, {0xD8, 0xD2, 1, 150, 0x760C, 0x6185}, {0x226, 0x92, 2, 200, 0x760D, 0x6189},
      {0x212, 0xBA, 2, 200, 0x7611, 0}}, {0x13C1, 6, 50, 0x21C, 0x92}, {0x13B2, 6, 100, 0xCA, 0xDA}},
};

}  // namespace

std::vector<std::string> Science::wrapText(const std::string& text, int width, int estimate) const {
    // Segment 23 (f23_02a4): from an estimate of the characters a line
    // holds, back to the space before (f23_0193, which leaves the space for
    // the next line) while too wide, or on to the next space (f23_0218,
    // the space kept) while it fits. Each line laid out in turn (f23_05d0,
    // its record f23_0769), measured in the text's font (f23_0000,
    // f23_001c: f32_073e's), the characters read through the resource's
    // window (f22_012f, f22_00e7).
    const int len = static_cast<int>(text.size());
    auto w = [&](int start, int n) { return font_->width(text.substr(static_cast<size_t>(start), static_cast<size_t>(n))); };
    auto back = [&](int start, int n) {
        if (start + n > len) return len - start;
        if (start + n - 1 == start) return 0;
        int v = start + n - 2;
        for (; v > start; --v)
            if (text[static_cast<size_t>(v)] == ' ') break;
        return v - start;
    };
    auto on = [&](int start, int n) {
        if (start + n >= len) return len - start;
        int v = start + n;
        while (v < len && text[static_cast<size_t>(v)] != ' ') ++v;
        return v - start + 1;
    };
    // The first estimate (f23_051c) is (width x width, in 16 bits) / the
    // text's width (unless the caller gives one); then each line starts
    // from the last one's length.
    const int total = std::max(1, font_->width(text));
    if (estimate <= 0) estimate = std::max(1, static_cast<int>((static_cast<unsigned>(width * width) & 0xFFFFu) / static_cast<unsigned>(total)));
    std::vector<std::string> lines;
    for (int start = 0; start < len;) {
        int n = back(start, estimate);
        const bool shrink = n != 0;
        if (!shrink) n = on(start, estimate);
        if (w(start, n) > width) {
            if (shrink)
                while (w(start, n) > width) {
                    const int m = back(start, n);
                    if (m == 0 || m == n) break;
                    n = m;
                }
        } else {
            int previous = n;
            for (;;) {
                if (w(start, n) > width) break;
                previous = n;
                const int m = on(start, n);
                if (m == n) break;
                n = m;
            }
            n = previous;
        }
        n = std::max(1, std::min(n, len - start));
        estimate = n;
        lines.push_back(text.substr(static_cast<size_t>(start), static_cast<size_t>(n)));
        start += n;
    }
    return lines;
}

void Science::drawStretched(int x, int y, int w, int h, uint16_t id) {
    // f14_148a: the bitmap scaled (256ths) to the rectangle, about its centre.
    const Bitmap& b = ctx_.bitmap(id);
    if (b.width == 0 || b.height == 0) return;
    scaledSprite((w >> 1) + x, (h >> 1) + y, w * 256 / b.width, h * 256 / b.height, id);
}

Science::Bubble Science::bubble(int ax, int ay, int width, int tail, const std::string& text) {
    // g15_0467 (entered at 15:0464) / f15_31fa / f15_2a35: the box is `width`
    // wide and as tall as the lines (f15_2ea4: its text, wrapped and laid
    // out, f23_0068), placed by the tail (0: right of it, 1: centred over
    // it, 2: left of it); filled with colour F, edged with the stretched
    // pieces (their rectangles f15_2c75), the lines in colour 0 (each by
    // the text's f17_0003: f22_0325 → f22_0156 → f22_024c), then the
    // tail; drawn with the lesson (f15_058a). ([8CE4] = 100 is the width
    // used when a step gives none.)
    Bubble b;
    const std::vector<std::string> lines = wrapText(text, width);
    const int h = static_cast<int>(lines.size()) * font_->height();
    const Bitmap& t = ctx_.bitmap(static_cast<uint16_t>(tail == 0 ? 5 : tail == 2 ? 6 : 7));
    int x = ax, y = ay;
    if (tail == 0) x += t.width, y -= t.height + h;
    else if (tail == 2) x -= t.width + width, y -= t.height + h;
    else x -= width / 2, y -= t.height + h;
    const int lw = ctx_.bitmap(kLeft).width, rw = ctx_.bitmap(kRight).width;
    const int th = ctx_.bitmap(kTop).height, bh = ctx_.bitmap(kBottom).height;
    fill(x, y, width, h, 0x0F);
    drawStretched(x - lw, y - 1, lw, h + 2, kLeft);
    drawStretched(x + width - 1, y - 1, rw, h + 2, kRight);
    drawStretched(x - lw, y - th, lw + width + rw, th, kTop);
    drawStretched(x - lw, y + h - 1, lw + width + rw, bh, kBottom);
    for (size_t i = 0; i < lines.size(); ++i)
        font_->draw(ctx_.screens[current()], x, y + static_cast<int>(i) * font_->height(), lines[i], 0);
    int tx = ax, ty = ay - t.height;
    if (tail == 1) tx -= t.width / 2;
    else if (tail == 2) tx -= t.width;
    drawLogo(tx, ty, static_cast<uint16_t>(tail == 0 ? 5 : tail == 2 ? 6 : 7));
    // What it covers, to take it away again (f15_2fee: the box, the tail
    // and the edges).
    b.x = std::min(x - lw, tx);
    b.y = std::min(y - th, ty);
    b.w = std::max(x + width + rw, tx + t.width) - b.x;
    b.h = std::max(y + h - 1 + bh, ty + t.height) - b.y;
    return b;
}

std::string Science::textResource(uint16_t id) {
    // f22_0000: the resource and its size; f22_0041 reads it 200 bytes at a
    // time into DS:8D44 (here all of it at once).
    std::vector<uint8_t> bytes;
    if (!ctx_.read(id, bytes)) return {};
    std::string s(bytes.begin(), bytes.end());
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == '\0')) s.pop_back();
    return s;
}

bool Science::waitMore() {
    // A click on MORE (the lesson's rectangle); false if the window closes.
    int x, y;
    for (;;) {
        ctx_.pump();
        if (ctx_.platform.takeClick(&x, &y) && x >= kMoreX && y >= kMoreY && x < kMoreX + kMoreW && y < kMoreY + kMoreH)
            return true;
    }
}

void Science::lessonStart(uint16_t picture) {
    // f15_076a: the lesson's picture with the look on screen 2 (a clean
    // copy kept on 3 to take bubbles away), FM sound 25; the builder's end
    // puts screen 2's palette into the others and the display (f20_0000).
    fmSound(0x25);
    select(1);
    applyLook();
    clearDisplay();
    select(2);
    showScreenWithLook(picture, 2);
    copyArea(2, 3, 0, 0, Screen::kWidth, Screen::kHeight);
    ctx_.screens[3].palette = ctx_.screens[2].palette;
    toDisplay(2);
    applyLook();
}

int Science::lesson(int n) {
    // f38_0fb5 (n 5-10): the lesson's picture shown, its two animations
    // and its lines one by one, MORE going on; then the room it leads to
    // (-1 none). The scene is drawn again from the clean picture (screen
    // 3) whenever a frame changes: the animations, then the bubble.
    const Lesson& l = kLessons[n - 5];
    lessonStart(l.picture);
    Anim anims[2] = {l.professor, l.edison};
    // Each animation's counter (f16_01a8: its own, from 0 when made): the
    // tick it started at.
    uint64_t began[2] = {0, 0};
    const uint64_t start = ctx_.platform.milliseconds();
    int shown[2] = {-1, -1};
    const Step* current = nullptr;
    std::string currentText;
    // A redraw (the lesson's f15_0b77: screen 2 saved and clipped,
    // f15_071c, entered at 15:0719; then f15_0ba7: the area to the display,
    // screen 2 restored, f15_0746, entered at 15:0743).
    auto compose = [&] {
        select(2);
        copyArea(3, 2, 0, 0, Screen::kWidth, Screen::kHeight);
        for (int i = 0; i < 2; ++i) drawLogo(anims[i].x, anims[i].y, static_cast<uint16_t>(anims[i].sprite + shown[i]));
        if (current) bubble(current->x, current->y, current->width, current->tail, currentText);
        copyArea(2, 1, 0, 0, Screen::kWidth, Screen::kHeight);
        select(1);
    };
    auto tick = [&] {
        // The game's ticks, 50 a second (f32_0777(50)); each a step of the
        // generators (the places' f16_007e: only their count). A frame
        // changed, its rectangle (f15_015b: the place, the frame's size) is
        // redrawn (f15_0124).
        const uint64_t ticks = (ctx_.platform.milliseconds() - start) / 20;
        bool changed = false;
        for (int i = 0; i < 2; ++i) {
            const uint64_t period = static_cast<uint64_t>(anims[i].period);
            const int frame = static_cast<int>(((ticks - began[i]) % period) * static_cast<uint64_t>(anims[i].count) / period);
            if (frame != shown[i]) shown[i] = frame, changed = true;
        }
        if (changed) compose();
    };
    // f32_0e7f(70, 7F, 8): colours 70-7F turn a step every 8 ticks
    // (f32_1113, f14_0148: each takes the next one's colour). The original
    // only does it on a palette display ([61F9]: 256 colours, as most
    // were in 1995; not under winevdm on a true-colour desktop).
    const Palette base = ctx_.displayPalette;
    uint64_t turned = 0;
    static const bool trueColour = std::getenv("SCI_TRUECOLOR") != nullptr;  // (as under winevdm: none)
    auto turn = [&] {
        if (trueColour) return;
        const uint64_t steps = (ctx_.platform.milliseconds() - start) / 20 / 8;
        if (steps == turned) return;
        turned = steps;
        for (int i = 0x70; i <= 0x7F; ++i)
            ctx_.displayPalette[i] = base[0x70 + static_cast<int>((static_cast<uint64_t>(i - 0x70) + steps) % 16)];
    };
    tick();
    // Lessons 5-8's professor (f15_0bde, its +08 g15_0cb7): pressed, he
    // takes the mouse ([27AC], [27AE]) and his animation becomes 13BE, 3
    // frames over 25 ticks; at the button's release, 13BB, 3 over 50 again
    // (f32_00cf lets the mouse go). Each a new cycle, from its first frame.
    // (Lessons 9 and 10's professors take no clicks: f10_00c2.)
    bool grabbed = false;
    auto swap = [&](const Anim& a) {
        anims[0].sprite = a.sprite, anims[0].count = a.count, anims[0].period = a.period;
        began[0] = (ctx_.platform.milliseconds() - start) / 20;
        shown[0] = -1;
        tick();
    };
    auto onProfessor = [&](int x, int y) {
        if (l.professor.sprite != kProfessor.sprite) return false;
        const Bitmap& b = ctx_.bitmap(static_cast<uint16_t>(anims[0].sprite + std::max(shown[0], 0)));
        return x >= anims[0].x && y >= anims[0].y && x < anims[0].x + b.width && y < anims[0].y + b.height;
    };
    // A key (event 8, f32_09a0) goes to the player first (f31_1d70: p, q,
    // m, s, S), then to the lesson (its +0C, g15_0b28): Esc (scan code 1)
    // ends it (+48, g15_0939: event 9 to its room, none in lesson 10),
    // any other, Shift alone too, is MORE (+44, the script's next step).
    enum { kNone, kMore, kEnd } byKey = kNone;
    exitRoom_ = 0;
    auto keys = [&] {
        const int k = ctx_.platform.takeKey();
        if (!ctx_.platform.takeKeyDown() && !k) return;
        if (k == 'p' || k == 'P' || k == 'q' || k == 'Q' || k == 'm' || k == 'M' || k == 's' || k == 'S') {
            keyEvent(k);
            compose();
        } else if (k == Platform::kEscape) {
            if (l.nextRoom >= 0) byKey = kEnd;
        } else {
            byKey = kMore;
        }
    };
    uint16_t pending = l.firstSound;
    for (const Step& step : l.steps) {
        // f15_0a70 / g15_0a1d: the last bubble goes, the new one comes.
        current = &step;
        currentText = textResource(step.text);
        compose();
        if (pending) narration(pending, false);
        pending = step.nextSound;
        int x, y;
        for (;;) {
            ctx_.pump();
            tick();
            turn();
            keys();
            if (exitRoom_) {
                ctx_.platform.stopWav();
                return exitRoom_;
            }
            if (byKey == kEnd) {
                ctx_.platform.stopWav();
                return l.nextRoom;
            }
            if (byKey == kMore) {
                byKey = kNone;
                break;
            }
            int mx, my;
            bool down = false;
            ctx_.platform.mouse(&mx, &my, &down);
            if (grabbed && !down) grabbed = false, swap(kProfessor);
            if (!ctx_.platform.takeClick(&x, &y)) continue;
            if (!grabbed && onProfessor(x, y)) {
                grabbed = true, swap({0x13BE, 3, 25, 0, 0});
                continue;
            }
            // A press in the lesson's rectangle (its +08: f15_1235, f15_176b,
            // f15_1c1b, f15_2002, f15_2408, f15_282d; while +13E, f15_0ae4,
            // entered at 15:0ae1: its children first) is MORE.
            if (x >= kMoreX && y >= kMoreY && x < kMoreX + kMoreW && y < kMoreY + kMoreH) break;
        }
    }
    ctx_.platform.stopWav();
    return l.nextRoom;
}

}  // namespace edison
