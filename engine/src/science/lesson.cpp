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
// g15_1c46, g15_202d, g15_23b9, g15_27ca): a bubble each step, at an
// anchor with its tail, the width, the text and the narration the next
// bubble plays. The anchors: A (1E0, AE) and B (21C, 7C) by the
// professor, C (A6, 11A) by Edison, the others lessons 9 and 10's own.
struct Step {
    int x, y, tail, width;
    uint16_t text, nextSound;
};
struct Lesson {
    uint16_t picture, firstSound;  // f15_076a's picture, the builder's [171A]
    int nextRoom;                  // +138: the room after (-1 none)
    std::vector<Step> steps;
};
const Lesson kLessons[6] = {
    {0x2003, 0x6169, 1,  // 5: f15_0fee
     {{0x1E0, 0xAE, 2, 200, 0x75F1, 0x616C}, {0x1E0, 0xAE, 2, 200, 0x75F4, 0x6173}, {0x1E0, 0xAE, 2, 200, 0x75FB, 0x6174},
      {0x21C, 0x7C, 2, 200, 0x75FC, 0x616D}, {0x1E0, 0xAE, 2, 230, 0x75F5, 0x6175}, {0x21C, 0x7C, 2, 200, 0x75FD, 0x6176},
      {0x1E0, 0xAE, 2, 200, 0x75FE, 0x616E}, {0x21C, 0x7C, 2, 200, 0x75F6, 0}}},
    {0x2004, 0x6177, 50,  // 6: f15_1524
     {{0x1E0, 0xAE, 2, 200, 0x75FF, 0x6178}, {0x21C, 0x7C, 2, 200, 0x7600, 0x6179}, {0x1E0, 0xAE, 2, 200, 0x7601, 0x617A},
      {0x1E0, 0xAE, 2, 240, 0x7602, 0x617B}, {0x1E0, 0xAE, 2, 200, 0x7603, 0x6172}, {0x21C, 0x7C, 2, 200, 0x75FA, 0}}},
    {0x2003, 0x617C, 2,  // 7: f15_19d4
     {{0x1E0, 0xAE, 2, 200, 0x7604, 0x617D}, {0x1E0, 0xAE, 2, 240, 0x7605, 0x6172}, {0x21C, 0x7C, 2, 200, 0x75FA, 0}}},
    {0x2003, 0x617E, 57,  // 8: f15_1dbb
     {{0x1E0, 0xAE, 2, 200, 0x7606, 0x617F}, {0x1E0, 0xAE, 2, 200, 0x7607, 0x618A}, {0xA6, 0x11A, 0, 200, 0x7612, 0}}},
    {0x2006, 0x6180, 67,  // 9: f15_21a2
     {{0x64, 0xFA, 0, 200, 0x7608, 0x6181}, {0x160, 0x12C, 2, 200, 0x7609, 0x6182}, {0x64, 0xFA, 0, 200, 0x760A, 0}}},
    {0x2005, 0x6183, -1,  // 10: f15_2591 (then f31_001a, f31_1728: not ported)
     {{0x212, 0xBA, 2, 200, 0x760B, 0x6184}, {0xD8, 0xD2, 1, 150, 0x760C, 0x6185}, {0x226, 0x92, 2, 200, 0x760D, 0x6189},
      {0x212, 0xBA, 2, 200, 0x7611, 0}}},
};

}  // namespace

std::vector<std::string> Science::wrapText(const std::string& text, int width) const {
    // Segment 23 (f23_02a4): from an estimate of the characters a line
    // holds, back to the space before (f23_0193, which leaves the space for
    // the next line) while too wide, or on to the next space (f23_0218,
    // the space kept) while it fits.
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
    // text's width; then each line starts from the last one's length.
    const int total = std::max(1, font_->width(text));
    int estimate = std::max(1, static_cast<int>((static_cast<unsigned>(width * width) & 0xFFFFu) / static_cast<unsigned>(total)));
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
    drawScaledCentred(x + w / 2, y + h / 2, w * 256 / b.width, h * 256 / b.height, id);
}

Science::Bubble Science::bubble(int ax, int ay, int width, int tail, const std::string& text) {
    // g15_0467 / f15_31fa / f15_2a35: the box is `width` wide and as tall as
    // the lines, placed by the tail (0: right of it, 1: centred over it,
    // 2: left of it); filled with colour F, edged with the stretched
    // pieces, the lines in colour 0, then the tail. ([8CE4] = 100 is the
    // width used when a step gives none.)
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
    // What it covers, to take it away again.
    b.x = std::min(x - lw, tx);
    b.y = std::min(y - th, ty);
    b.w = std::max(x + width + rw, tx + t.width) - b.x;
    b.h = std::max(y + h - 1 + bh, ty + t.height) - b.y;
    return b;
}

std::string Science::textResource(uint16_t id) {
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
    // copy kept on 3 to take bubbles away), FM sound 25.
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
    // f38_0fb5 (n 5-10): the lesson's picture shown, its lines one by
    // one, MORE going on; then the room it leads to (-1 none).
    const Lesson& l = kLessons[n - 5];
    lessonStart(l.picture);
    uint16_t pending = l.firstSound;
    Bubble last{};
    for (const Step& step : l.steps) {
        // f15_0a70: the last bubble taken away.
        select(2);
        if (last.w > 0) copyArea(3, 2, last.x, last.y, last.w, last.h);
        last = bubble(step.x, step.y, step.width, step.tail, textResource(step.text));
        copyArea(2, 1, 0, 0, Screen::kWidth, Screen::kHeight);
        select(1);
        if (pending) narration(pending, false);
        pending = step.nextSound;
        waitMore();
    }
    ctx_.platform.stopWav();
    return l.nextRoom;
}

}  // namespace edison
