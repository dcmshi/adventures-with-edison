// WMAIN.EXE: the arcade's controls (segment 30): the panel under the table
// (its sliders, value boxes, the ball type and the shoot button; made from
// a room's PANEL line by f61_0f76) and the two columns of balls at the
// sides. See docs/SCIENCE.md.

#include <algorithm>
#include <cstdio>

#include "science/science.h"

namespace edison {

namespace {

// The sliders (f30_2eb7 and its kinds): the value's range, the knob, and
// where the knob slides (f30_391b gravity, f30_3c42 friction, f30_3527
// power): off = (value - min) * (h - 19) / (max - min) up the slope, the
// rectangle's height that of the kind's first frame (DS:234E, 2362, 233A).
struct SliderKind {
    int x, y;           // where it's made (f61_0f76)
    int min, max;       // +3E, +3C
    uint16_t frame0;    // its frames' first (their size is the rectangle's)
    uint16_t knob;
};
constexpr SliderKind kGravity{0x50, 299, -16, 4, 0x1188, 0x117C};
constexpr SliderKind kFriction{0xC0, 299, 0, 16, 0x1191, 0x117D};
constexpr SliderKind kPower{0x1A8, 299, 0, 16, 0x119A, 0x117E};

}  // namespace

bool Science::panelSprite(int x, int y, uint16_t id) {
    // f14_1179: a sprite only if it lies wholly on the screen.
    const Bitmap& bmp = ctx_.bitmap(id);
    if (x < 0 || y < 0 || x + bmp.width > Screen::kWidth || y + bmp.height > Screen::kHeight) return false;
    ctx_.screens.drawSprite(current(), bmp, x, y);
    return true;
}

void Science::drawKnob(int kind, int value) {
    const SliderKind& k = kind == 0 ? kGravity : kind == 1 ? kFriction : kPower;
    const Bitmap& frame = ctx_.bitmap(k.frame0);
    const int off = (value - k.min) * (frame.height - 19) / (k.max - k.min);
    const int y = k.y + frame.height - 19 - off;
    int x;
    if (kind == 0) x = k.x + 4 + off / 2;               // f30_391b
    else if (kind == 1) x = k.x + 6 + off / 3;          // f30_3c42
    else x = k.x + frame.width / 4 - off / 2;           // f30_3527
    panelSprite(x, y, k.knob);
}

void Science::valueBox(int x, int y, int w, int h, const std::string& text) {
    // f30_1af3 (a box's draw, f30_2e8b): the box (+5B: its rectangle and
    // the text's, f14_180e, joined) in colour 0, the text at its corner a
    // pixel right in colour 16, then in colour 23 (font 103).
    fill(x, y, std::max(w, font_->width(text)), std::max(h, font_->height()), 0);
    textAt(x + 1, y, text, 0x16);
    textAt(x, y, text, 0x23);
}

void Science::drawPanel(const Rect* changed) {
    // The panel's draw (f30_15f2, its method +40) of an area (the whole of
    // it, or what its controls marked changed, f29_0313): screen 3 to
    // screen 2 over it, its controls in the order they were added
    // (f30_13e4), the walking figure (off the screen at rest), and the area
    // to the display.
    const Rect whole{0, table_.view.y + table_.view.h, Screen::kWidth, Screen::kHeight - (table_.view.y + table_.view.h)};
    const Rect area = changed ? intersect(*changed, whole) : whole;
    if (area.w <= 0 || area.h <= 0) return;
    copyArea(3, 2, area.x, area.y, area.w, area.h);
    select(2);
    setPolygonClip(area.x, area.y, area.w, area.h);
    const PanelState& p = panel_;
    // Gravity: its knob, its box (f30_39fc: -value / 4.0 as "%c%d.%d").
    drawKnob(0, p.gravity);
    {
        const int tenths = -p.gravity * 10 / 4;
        char s[16];
        std::snprintf(s, sizeof s, "%c%d.%d", tenths < 0 ? '-' : ' ', std::abs(tenths) / 10, std::abs(tenths) % 10);
        valueBox(0x4A, 0x172, 0x28, 0x10, s);
    }
    drawKnob(1, p.friction);
    valueBox(200, 0x172, 0x20, 0x10, std::to_string(p.friction));
    // The ball type (f30_244e: DS:22D2, the first when down) and its name (DS:22E2).
    panelSprite(0x124, 0x150, ballTypePressed_ ? 0x11BE : 0x11BF);
    valueBox(0x134, 0x140, 0x4C, 0x11, dataString(static_cast<uint16_t>(0x22E2 + 10 * p.ballType)));
    drawKnob(2, p.power);
    valueBox(0x1C6, 0x172, 0x20, 0x10, std::to_string(p.power));
    // The shoot button (f30_2730: DS:231E, the first when down).
    panelSprite(0x1F0, 0x13B, shootPressed_ ? 0x11C0 : 0x11C1);
    clearPolygonClip();
    copyArea(2, 1, area.x, area.y, area.w, area.h);
}

void Science::drawColumn(bool right) {
    // f30_0599: the column's tube (140E, 140F), its balls from the bottom up
    // every 24 rows, each a random frame of the rolling ball (Borland's
    // rand), then the PUSH button (1421 at (0, 70h), 1422 at (25Eh, D0h)).
    const int x = right ? 0x256 : 0, y = right ? 0x8C : 6, h = right ? 0x96 : 0xFA, w = right ? 0x28 : 0x23;
    const int count = right ? rightBalls_ : leftBalls_;
    copyArea(3, 2, x, y, w, h);
    select(2);
    panelSprite(x, y, right ? 0x140F : 0x140E);
    int by = y + h - 0x1D;
    const int offset = columns_[right ? 1 : 0].offset;
    for (int i = 0; i < count; ++i, by -= 0x18) {
        // The top ball, while it's pushed (+138), that much higher.
        const int drawY = i == count - 1 ? by - offset : by;
        if (y + 10 >= drawY) continue;
        const int frame = static_cast<int>(static_cast<long>(borlandRand()) * 6 / 0x8000);
        if (!right) {
            panelSprite(x, drawY, static_cast<uint16_t>(0x1016 + frame));
        } else {
            panelSprite(x + 6, drawY, static_cast<uint16_t>(0x101C + frame));
            panelSprite(x + 8, drawY + 4, 0x1358);
        }
    }
    if (!right) panelSprite(0, 0x70, 0x1421);
    else panelSprite(0x25E, 0xD0, 0x1422);
    copyArea(2, 1, x, y, w, h);
}

int Science::borlandRand() {
    // f01_3309: rand(), Borland's.
    randSeed_ = randSeed_ * 0x015A4E35u + 1;
    return static_cast<int>((randSeed_ >> 16) & 0x7FFF);
}

}  // namespace edison
