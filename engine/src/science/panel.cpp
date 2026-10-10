// WMAIN.EXE: the arcade's controls (segment 30): the panel under the table
// (f30_1268; its sliders, value boxes (f30_2aa7), the ball type and the
// shoot button (buttons, f30_283c); made from a room's PANEL line by
// f61_0f76) and the two columns of balls at the sides (f30_0000 left,
// f30_0860 right). See docs/SCIENCE.md.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "artech/context.h"

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
constexpr SliderKind kGravity{0x50, 299, -16, 4, 0x1188, 0x117C};   // f30_37f1
constexpr SliderKind kFriction{0xC0, 299, 0, 16, 0x1191, 0x117D};   // f30_3a88
constexpr SliderKind kPower{0x1A8, 299, 0, 16, 0x119A, 0x117E};     // f30_360f

}  // namespace

bool Science::panelSprite(int x, int y, uint16_t id) {
    // f14_1179: a sprite only if it lies wholly on the screen.
    if (recording_) {
        recording_->insert(recording_->end(), {current(), x, y, id, 1});
        return true;
    }
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
    // f30_1af3 (30:1af3; a box's draw, f30_2e8b): the box (+5B: its
    // rectangle and the text's, f14_180e, joined) in colour 0, the text at
    // its corner a pixel right in colour 16, then in colour 23 (font 103).
    fill(x, y, std::max(w, font_->width(text)), std::max(h, font_->height()), 0);
    textAt(x + 1, y, text, 0x16);
    textAt(x, y, text, 0x23);
}

void Science::drawPanel(const Rect* changed) {
    // The panel's draw (f30_15f2, its method +40) of an area (the whole of
    // it, or what its controls marked changed, f29_0313): screen 3 to
    // screen 2 over it, its controls in the order they were added
    // (f30_13e4), the walking figure (off the screen at rest), and the area
    // to the display. The panel's area (its gamearea's, f29_00a9: within
    // the screen) is all under the view.
    const Rect whole{0, table_.view.y + table_.view.h, Screen::kWidth, Screen::kHeight - (table_.view.y + table_.view.h)};
    const Rect area = changed ? intersect(*changed, whole) : whole;
    if (area.w <= 0 || area.h <= 0) return;
    copyArea(3, 2, area.x, area.y, area.w, area.h);
    select(2);
    setPolygonClip(area.x, area.y, area.w, area.h);
    const PanelState& p = panel_;
    // Gravity: its knob, its box (f30_39fc: -value / 4.0, a box of a
    // number with tenths, f30_2b45 / f30_2d43, as "%c%d.%d": f30_1c59).
    drawKnob(0, p.gravity);
    {
        const int tenths = -p.gravity * 10 / 4;
        char s[16];
        std::snprintf(s, sizeof s, "%c%d.%d", tenths < 0 ? '-' : ' ', std::abs(tenths) / 10, std::abs(tenths) % 10);
        valueBox(0x4A, 0x172, 0x28, 0x10, s);
    }
    drawKnob(1, p.friction);
    valueBox(200, 0x172, 0x20, 0x10, std::to_string(p.friction));
    // The ball type (f30_244e: DS:22D2, the first when down; a button's
    // draw, f30_2a22) and its name (DS:22E2: a box of text, f30_2be0).
    panelSprite(0x124, 0x150, ballTypePressed_ ? 0x11BE : 0x11BF);
    valueBox(0x134, 0x140, 0x4C, 0x11, dataString(static_cast<uint16_t>(0x22E2 + 10 * p.ballType)));
    drawKnob(2, p.power);
    valueBox(0x1C6, 0x172, 0x20, 0x10, std::to_string(p.power));
    // The shoot button (f30_2730: DS:231E, the first when down).
    panelSprite(0x1F0, 0x13B, shootPressed_ ? 0x11C0 : 0x11C1);
    // Locked controls' signs (drawn with each, +2C): a slider's 134C at its
    // frame's middle a pixel right (f30_20ec, 30:20e9), the ball type's
    // 134D 6 right and 4 down of its middle (f30_25fd).
    for (int c = 0; c < 4; ++c) {
        if (!p.sign[c]) continue;
        if (c == 2) {
            const Bitmap& b = ctx_.bitmap(0x11BF);
            const Bitmap& sign = ctx_.bitmap(0x134D);
            panelSprite(0x124 + b.width / 2 + 6 - sign.width / 2, 0x150 + b.height / 2 + 4 - sign.height / 2, 0x134D);
        } else {
            const SliderKind& k = c == 0 ? kGravity : c == 1 ? kFriction : kPower;
            const Bitmap& f = ctx_.bitmap(k.frame0);
            const Bitmap& sign = ctx_.bitmap(0x134C);
            panelSprite(k.x + f.width / 2 + 1 - sign.width / 2, k.y + f.height / 2 - sign.height / 2, 0x134C);
        }
    }
    // Edison (f30_1120, 30:1120): his frame (131C on) on the bottom of his
    // 96 x 104 rectangle (f30_0bc1) round his middle (f30_0b1c, 30:0b1c:
    // its last row; drawn by f14_12e9 → f72_02cd).
    if (runner_.flags != 0) {
        const Bitmap& b = ctx_.bitmap(static_cast<uint16_t>(0x131C + std::max(runner_.frame, 0)));
        const int top = runner_.y - 52 + 104 - 1 - b.height;
        ctx_.screens.drawSprite(current(), b, runner_.x - 48, top);
    }
    clearPolygonClip();
    copyArea(2, 1, area.x, area.y, area.w, area.h);
}

void Science::drawColumn(bool right) {
    // f30_0599: the column's tube (140E, 140F), its balls from the bottom up
    // every 24 rows, each a random frame of the rolling ball (Borland's
    // rand), then the PUSH button (1421 at (0, 70h), 1422 at (25Eh, D0h)).
    const int x = right ? 0x256 : 0, y = right ? 0x8C : 6, h = right ? 0x96 : 0xFA, w = right ? 0x28 : 0x23;
    // (Its count the player's +BA / +BC, given it as it's made: f31_046c,
    // f31_04ac.)
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

Science::Rect Science::controlArea(int c) {
    // The control's rectangle (+12): a slider's where it takes the mouse,
    // the ball type's its picture.
    if (c == 0) return {0, 300, 0xAE, 100};
    if (c == 1) return {0xAE, 300, 0x68, 100};
    if (c == 3) return {0x18A, 300, 0x6A, 100};
    const Bitmap& b = ctx_.bitmap(0x11BF);
    return {0x124, 0x150, b.width, b.height};
}

void Science::signAt(int c) {
    // f30_2097: locked (+2E) with its sign (+2C), drawn again.
    panel_.sign[c] = true;
    markPanel(controlArea(c));
}

void Science::lockControls() {
    // f61_0f76: a control whose flags are 2 is locked with its sign at
    // once; 1, locked and listed (in the order made: gravity, friction, the
    // ball type, power) for Edison to sign (f30_1569: from the right, the
    // list backwards).
    runner_ = Runner{};
    // Each control made unlocked, without its sign (f30_1ed1).
    // f30_0910: he starts off the panel on a random side, at its middle's
    // height + 4 (the panel (0, 286, 640, 114)).
    bool right = static_cast<long>(borlandRand()) * 2 / 0x8000 == 0;
    // (Testing: SCI_RUNNERSIDE=left or right, as the original's rand may have.)
    if (const char* side = std::getenv("SCI_RUNNERSIDE")) right = std::strcmp(side, "left") != 0;
    runner_.x = right ? 640 - 1 + 80 : -60;
    runner_.dx = right ? -17 : 17;
    for (int c = 0; c < 4; ++c) panel_.sign[c] = false;
    const int flags[4] = {panel_.gravityFlags, panel_.frictionFlags, panel_.ballTypeFlags, panel_.powerFlags};
    int list[4], n = 0;
    for (int c = 0; c < 4; ++c) {
        if ((flags[c] & 3) == 2) panel_.sign[c] = true;
        else if ((flags[c] & 3) == 1) list[n++] = c;
    }
    for (int i = 0; i < n; ++i) runner_.queue[runner_.dx < 1 ? n - 1 - i : i] = list[i];
    runner_.queued = n;
    panelBusy_ = n > 0;
}

void Science::runnerMode(int mode) {
    // f30_0c6e (30:0c6e): stopped (and the controls free again), running
    // (1), putting a sign up (2) or carrying it (3), facing his way.
    Runner& r = runner_;
    r.pending = false;
    const bool left = r.dx < 1;
    switch (mode) {
    case 0: r.first = 0, r.count = 0, panelBusy_ = false; break;
    case 1: r.period = 2, r.first = left ? 14 : 38, r.count = 10; break;
    case 2: r.period = 5, r.first = left ? 10 : 34, r.count = 4; break;
    case 3: r.period = 2, r.first = left ? 0 : 24, r.count = 10; break;
    default: break;
    }
    r.frame = r.first - 1;
}

void Science::runnerTick() {
    // f30_1430: the walking figure's tick (f30_0d7c, 30:0d7c), then the
    // next control to sign, if he's free (f30_148f: he stops 48 to its
    // right, coming from there, else 48 to its left).
    Runner& r = runner_;
    if (std::getenv("SCI_RUNNER"))  // (testing: his state, for tracecmp.py)
        logLine("runner t" + std::to_string(runnerTicks_) + " x " + std::to_string(r.x) + " f " + std::to_string(r.frame) + " per " + std::to_string(r.period) + " dx " + std::to_string(r.dx) + " tgt " + std::to_string(r.target) + " fl " + std::to_string(r.flags) + " pend " + std::to_string(r.pending));
    const Rect before{r.x - 48, r.y - 52, 96, 104};  // f30_0b1c
    if (++runnerTicks_ % r.period == 0) {
        bool moved = true;
        if (r.flags == 0) {
            r.pending = true, r.target = 0, r.dx = 0;
            moved = false;
        } else if (r.flags & 1) {
            if (r.pending) runnerMode(r.flags);
            if ((r.dx < 0 && r.x <= r.target) || (r.dx > 0 && r.x >= r.target)) {
                // +44: arrived (f30_152c: a mode change; with 2, the sign;
                // first the base's, f30_1096 (30:1096): +144 set).
                // A place off the panel (left of it, or at its right edge
                // and on) only stops his frames (f30_0c6e with 0): the
                // sign, if one's still to go up, then goes up at once
                // (gravity's, coming from the left: 48 left of it).
                r.pending = true;
                r.flags &= ~1;
                if (r.target < 0 || r.target >= 639) runnerMode(0);
            } else {
                r.x += r.dx;
            }
        } else if (r.flags & 2) {
            if (r.pending) {
                runnerMode(2);
                r.dx = r.dx < 0 ? -17 : 17;
            } else if (r.count <= r.frame - r.first + 1) {
                r.pending = true;
                if (r.current >= 0) signAt(r.current);
                ++r.done, r.current = -1;
                r.flags = (r.flags & ~2) | 1;
                r.target = r.dx < 0 ? -60 : 640 + 79;
            }
            r.x += r.dx;  // (he slides on while putting it up)
        }
        if (moved) {
            if (++r.frame >= r.first + r.count) r.frame = r.first;
            const int f = r.frame;
            if (f == 1 || f == 6 || f == 15 || f == 20 || f == 25 || f == 30 || f == 39 || f == 44) sound(0x6009);
            const Rect after{r.x - 48, r.y - 52, 96, 104};
            markPanel(before);
            markPanel(after);
        }
    }
    if (r.done < r.queued && r.current < 0) {
        panelBusy_ = true;
        const int c = r.queue[r.done];
        r.current = c;
        // f30_10fd → f30_10ad: the place (+148), the way (17 a step), +15E 3.
        const Rect a = controlArea(c);
        const int right = a.x + a.w - 1;
        r.target = r.x > right + 48 ? right + 48 : a.x - 48;
        r.dx = r.target < r.x ? -17 : 17;
        r.flags |= 3;
    }
}

}  // namespace edison
