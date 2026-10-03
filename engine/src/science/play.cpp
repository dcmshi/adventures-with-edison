// WMAIN.EXE: playing a room: the arcade's loop (f32_0c1d: a tick every
// 20 ms from the 50 Hz timer, mouse events when the mouse moves or its
// button is down or was pressed), the mouse handed out as the player does
// (f31_241a), and the panel's and columns' controls (segment 30). See
// docs/SCIENCE.md.

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "science/science.h"

namespace edison {

void Science::playRoom(int room) {
    enterRoom(room);
    Mouse last;
    ctx_.platform.mouse(&last.x, &last.y, &last.held);
    bool lastPressed = false;  // [27D2]: the last event had a press
    uint64_t next = ctx_.platform.milliseconds() + 20;
    for (;;) {
        ctx_.pump();
        if (escapePressed()) return;
        // f32_09a0: an event when the button is down or was pressed since
        // the last ([6EC5]), or the mouse moved, or (with it up) the last
        // event had a press. So a button let go without the mouse moving,
        // after an event without a press, isn't seen (as in the original).
        Mouse m;
        ctx_.platform.mouse(&m.x, &m.y, &m.held);
        int cx, cy;
        if (ctx_.platform.takeClick(&cx, &cy)) m.click = true, m.x = cx, m.y = cy;
        if ((lastPressed && !m.held) || m.held || m.click || m.x != last.x || m.y != last.y) {
            lastPressed = m.click;
            mouseEvent(m);
            last = m;
        }
        // The 50 Hz timer's tick (event 4).
        const uint64_t now = ctx_.platform.milliseconds();
        if (now >= next) {
            next += 20;
            if (next + 100 < now) next = now;  // fallen behind (a stall): no catching up
            tickRoom();
            if (shotHeld_) shoot();
        }
        flushRoom();
    }
}

void Science::mouseEvent(const Mouse& m) {
    // f31_241a: to whoever has the mouse ([27AC]); else the room if it's in
    // the view, the panel, the columns.
    if (captured_ == Control::Gravity || captured_ == Control::Friction || captured_ == Control::Power) {
        sliderClick(captured_, m);
        return;
    }
    if (captured_ == Control::BallType || captured_ == Control::Shoot) {
        buttonClick(captured_, m);
        return;
    }
    if (inside(table_.view, m.x, m.y)) {
        aim(m);
        return;
    }
    const int panelTop = table_.view.y + table_.view.h;
    if (m.y >= panelTop) {
        // f30_1879: the first control whose area has the point (and that
        // isn't locked, +2E), its click (+8).
        struct Area {
            Control c;
            Rect r;
            int flags;
        };
        const Area areas[] = {
            {Control::Gravity, {0, 300, 0xAE, 100}, panel_.gravityFlags},
            {Control::Friction, {0xAE, 300, 0x68, 100}, panel_.frictionFlags},
            {Control::BallType, {0x116, 300, 0x74, 100}, panel_.ballTypeFlags},
            {Control::Power, {0x18A, 300, 0x6A, 100}, panel_.powerFlags},
            {Control::Shoot, {500, 300, 0x8C, 100}, 0},
        };
        for (const Area& a : areas) {
            if (!inside(a.r, m.x, m.y) || (a.flags & 3) != 0) continue;
            if (a.c == Control::BallType || a.c == Control::Shoot) buttonClick(a.c, m);
            else sliderClick(a.c, m);
            return;
        }
        return;
    }
    // The columns (f30_0496): their PUSH buttons.
    columnClick(false, m);
    columnClick(true, m);
}

bool Science::sliderClick(Control c, const Mouse& m) {
    // f30_306a: a press takes the mouse; while it's held, every 2 panel
    // ticks (7 after the first step) the value steps towards the pointer
    // (up when it's above the knob's line, f30_33c7); a release lets go.
    if (captured_ == Control::None) {
        if (!m.click) return false;
        captured_ = c;
        sliderRepeats_ = 0;
        panelTicks_ = 0;
        return true;
    }
    if (m.held) {
        if (panelTicks_ < (sliderRepeats_ == 1 ? 7 : 2)) return true;
        panelTicks_ = 0;
        int& value = c == Control::Gravity ? panel_.gravity : c == Control::Friction ? panel_.friction : panel_.power;
        const int min = c == Control::Gravity ? -16 : 0, max = c == Control::Gravity ? 4 : 16;
        const uint16_t frame0 = c == Control::Gravity ? 0x1188 : c == Control::Friction ? 0x1191 : 0x119A;
        const int x0 = c == Control::Gravity ? 0x50 : c == Control::Friction ? 0xC0 : 0x1A8;
        (void)x0;
        const Bitmap& frame = ctx_.bitmap(frame0);
        const int index = (value - min) * (9 - 1) / (max - min);
        const int line = frame.height - index * frame.height / 9 + 299 - 1;  // f30_2fdf
        int v = value;
        if (m.y < line - 2) ++sliderRepeats_, ++v, sound(0x602A);
        else if (m.y > line + 2) ++sliderRepeats_, --v, sound(0x6029);
        setSlider(c, v);
        return true;
    }
    captured_ = Control::None;  // f32_00cf
    sliderRepeats_ = 0;
    return true;
}

void Science::setSlider(Control c, int value) {
    // f30_3339: kept in the range, then the kind's own (+28): gravity
    // (f30_39fc: the room's gravity, f27_108d; its box -value / 4.0),
    // friction (f30_3bb2: the room's +F07), power (f30_3739: the shot's
    // speed, f27_26e3). The physics isn't ported yet: only the values.
    if (c == Control::Gravity) panel_.gravity = std::clamp(value, -16, 4);
    else if (c == Control::Friction) panel_.friction = std::clamp(value, 0, 16);
    else panel_.power = std::clamp(value, 0, 16);
    applyPanelPhysics();
    // f29_0313: its sprite's rectangle (its first frame's size) and its box.
    const uint16_t frame0 = c == Control::Gravity ? 0x1188 : c == Control::Friction ? 0x1191 : 0x119A;
    const int x0 = c == Control::Gravity ? 0x50 : c == Control::Friction ? 0xC0 : 0x1A8;
    const Bitmap& f = ctx_.bitmap(frame0);
    markPanel({x0, 299, f.width, f.height});
    const Rect box = c == Control::Gravity ? Rect{0x4A, 0x172, 0x28, 0x12} : c == Control::Friction ? Rect{200, 0x172, 0x20, 0x12} : Rect{0x1C6, 0x172, 0x20, 0x12};
    markPanel(box);
}

void Science::markPanel(const Rect& r) {
    // f29_0313: a changed rectangle of the panel, joined to the others.
    if (!panelDirty_) {
        panelDirtyRect_ = r;
    } else {
        const int x0 = std::min(panelDirtyRect_.x, r.x), y0 = std::min(panelDirtyRect_.y, r.y);
        const int x1 = std::max(panelDirtyRect_.x + panelDirtyRect_.w, r.x + r.w);
        const int y1 = std::max(panelDirtyRect_.y + panelDirtyRect_.h, r.y + r.h);
        panelDirtyRect_ = {x0, y0, x1 - x0, y1 - y0};
    }
    panelDirty_ = true;
}

void Science::markButton(Control c) {
    if (c == Control::BallType) {
        markPanel({0x124, 0x150, 80, 13});
        markPanel({0x134, 0x140, 0x4C, 0x12});
    } else {
        markPanel({0x1F0, 0x13B, 79, 27});
    }
}

void Science::applyPanelPhysics() {
    // Gravity (f30_39fc → f27_108d): p = (-value * [2360] (4) * 42) /
    // (50 * 2), the room's ratio p * -200 / 10. Friction (f30_3bb2): +F07 =
    // value * +F0B (255) * 8 / 256. Power (f30_3739 → f27_26e3): value *
    // (168 * 3 / 8) / 16 (2000 at the top), at most 7FFFh / 168, times 168.
    const int16_t p = static_cast<int16_t>(static_cast<int16_t>(-panel_.gravity * 4 * kTimerK) / (kTimerRate * 2));
    gravity_ = p * -200 / 10;
    frictionDen_ = 255;
    frictionNum_ = static_cast<int32_t>(panel_.friction) * frictionDen_ * 8 / 0x100;
    const int unit = kTimerK * 200 / kTimerRate;  // 168
    int speed = panel_.power >= 16 ? 2000 : static_cast<int16_t>(static_cast<int32_t>(panel_.power) * (unit * 3 / 8) / 0x10);
    speed = std::min(speed, 0x7FFF / unit);
    power_ = unit * speed;
}

void Science::shoot() {
    // f27_27a3 → f06_09a1: with a ball and its target, at the target's
    // middle (its box's x + w / 2, y + d / 2, its foot); sound 6003; a shot
    // more.
    if (!hasBall_) return;
    if (const char* when = std::getenv("SCI_SHOOT_WHEN")) {
        // (Testing: the shot held till the ball is in this state, "cx,cy,cz,
        // vx,vy,vz", the original's when its shot came, so a trace taken
        // while the ball moves can be compared from there.)
        const Ball& b = ball_;
        const std::string now = std::to_string(b.cx) + "," + std::to_string(b.cy) + "," + std::to_string(b.cz) + "," +
                                std::to_string(b.v[0]) + "," + std::to_string(b.v[1]) + "," + std::to_string(b.v[2]);
        shotHeld_ = now != when;
        if (shotHeld_) return;
    }
    const int z = heightUnder(targetX_, targetY_) + (targetMoved_ ? 1 : 0);
    ballLaunch(targetX_, targetY_, z);
    sound(0x6003);
    ++shots_;
    viewDirty_ = true;
}

void Science::crackGlass() {
    // f27_0772: an impact mark on the glass (not yet).
}

bool Science::buttonClick(Control c, const Mouse& m) {
    // f30_28f1: a press (not while another control has the mouse, nor
    // [22C8]) takes the next state and shows the button down; a release
    // shows it up.
    if (captured_ == Control::None) {
        if (!m.click) return false;
        captured_ = c;
        if (c == Control::BallType) {
            ballTypePressed_ = true;
            setBallType((panel_.ballType + 1) % 6);
        } else {
            shootPressed_ = true;
            shoot();  // f30_281d → f27_27a3
        }
        markButton(c);
        return true;
    }
    if (!m.held) {
        captured_ = Control::None;
        ballTypePressed_ = shootPressed_ = false;
        markButton(c);
    }
    return false;
}

void Science::setBallType(int type) {
    // f30_2574: Magic (5) goes back to Ice; the ball told (its +8); the
    // name in the box; sound 6007.
    panel_.ballType = type == 5 ? 0 : type;
    ball_.kind = panel_.ballType;  // f07_05bf (its mass, its record)
    sound(0x6007);
    markButton(Control::BallType);
    viewDirty_ = true;  // the ball's look (f07_04d5)
}

void Science::columnClick(bool right, const Mouse& m) {
    // f30_0496: in the PUSH button (left (0, 16), right (604, 145), 35 x
    // 43), held, with no ball of its own on the table and not already
    // pushing: the top ball starts up the tube (FM sound 0).
    const Rect push = right ? Rect{0x25C, 0x91, 0x23, 0x2B} : Rect{0, 0x10, 0x23, 0x2B};
    if (!inside(push, m.x, m.y)) return;
    Column& col = columns_[right ? 1 : 0];
    if (col.ballOut || col.pushing || !m.held) return;
    fmSound(0);
    col.pushing = true;
    col.offset = 1;
    columnDirty_[right ? 1 : 0] = true;
}

void Science::aim(const Mouse& m) {
    // f27_2d15: with the button held and a target, along the line of
    // sight from the point at depth 0 (direction (-100, 282, -100), steps
    // 1, 5, 9, ... long, f11_02f7) to where it meets the ground (within 1),
    // and the target there (f08_056e: sound 6026 if it moved far), sound
    // 6023. Past depth 314h, nothing.
    if (!m.held || !hasBall_) return;
    const Rect& v = table_.view;
    const int bottom = v.y + v.h - 1;
    const int baseX = m.x - v.x - table_.scrollX, baseH = bottom - m.y;
    static const int kSight[3] = {-100, 282, -100};
    int unit[3];
    libNormalize(kSight, unit);  // f11_02f7
    const int ux = unit[0], uy = unit[1], uh = unit[2];
    int x = baseX, y = 0, h = baseH;
    for (int step = 1;; step += 4) {
        if (y > 0x314) return;
        x = baseX + static_cast<int>(static_cast<long>(ux) * step / 0x7FFE);
        y = static_cast<int>(static_cast<long>(uy) * step / 0x7FFE);
        h = baseH + static_cast<int>(static_cast<long>(uh) * step / 0x7FFE);
        const int d = h - heightUnder(x, y);
        if (d <= 1 && d >= -1) break;
    }
    const int dx = targetX_ - x, dy = targetY_ - y;
    if (dx * dx + dy * dy > 15 * 15) sound(0x6026);
    targetX_ = x, targetY_ = y;
    targetMoved_ = true;
    sound(0x6023);
    viewDirty_ = true;
}

int Science::libAtan2(int adjacent, int opposite) const {
    // f87_0804: the table (seg87:0000, 400h words) by the smaller over the
    // larger, in 10000h a turn; then the quadrant.
    auto table = [&](uint32_t ratio) {
        const size_t at = (ratio >> 5) & 0xFFFE;
        return static_cast<int>(static_cast<int16_t>(atans_[at] | atans_[at + 1] << 8));
    };
    const bool negX = adjacent < 0, negY = opposite < 0;
    const uint32_t x = static_cast<uint16_t>(negX ? -adjacent : adjacent);
    const uint32_t y = static_cast<uint16_t>(negY ? -opposite : opposite);
    int a;
    if (y == 0) {
        if (x == 0) return 0;
        a = 0;
    } else if (y < x) {
        a = table((y << 16) / x);
    } else if (x == y) {
        a = 0x2000;
    } else if (x == 0) {
        a = 0x4000;
    } else {
        a = 0x4000 - table((x << 16) / y);
    }
    if (negX) a = -0x8000 - a;
    if (negY && y != 0) a = -a;
    return static_cast<int16_t>(a);
}

std::pair<int, int> Science::libSinCos(int angle) const {
    // f86_1000: a quarter of a sine (seg86:0000, 800h words, 65534ths)
    // halved; the cosine from the other end.
    const uint16_t a = static_cast<uint16_t>(angle);
    const size_t at = (a >> 2) & 0xFFE;
    auto word = [&](size_t o) { return static_cast<int>(static_cast<uint16_t>(sines_[o] | sines_[o + 1] << 8) >> 1); };
    int s = word(at), c = word(0xFFE - at);
    if (a & 0x4000) std::swap(s, c);
    if (((a >> 14) ^ (a >> 15)) & 1) c = -c;
    if (a & 0x8000) s = -s;
    return {s, c};
}

void Science::libNormalize(const int v[3], int out[3]) const {
    // f84_0000: the heading of (x, y), the length across, the elevation;
    // the unit vector from their sines and cosines.
    const auto [s1, c1] = libSinCos(libAtan2(v[0], v[1]));
    const long across = static_cast<long>(c1) * v[0] + static_cast<long>(s1) * v[1];
    const int acrossHigh = static_cast<int>(static_cast<uint16_t>((static_cast<uint32_t>(across) >> 16) << 1) |
                                            (static_cast<int16_t>(across & 0xFFFF) < 0 ? 1 : 0));
    const auto [s2, c2] = libSinCos(libAtan2(v[2], static_cast<int16_t>(acrossHigh)));
    out[2] = c2;
    auto mulHigh = [](long p) {
        return static_cast<int>(static_cast<int16_t>(((static_cast<uint32_t>(p) >> 16) << 1) |
                                                     (static_cast<int16_t>(p & 0xFFFF) < 0 ? 1 : 0)));
    };
    out[0] = mulHigh(static_cast<long>(s2) * c1);
    out[1] = mulHigh(static_cast<long>(s1) * s2);
}

void Science::tickRoom() {
    // f31_1c79: the room's tick (its objects; the physics isn't ported
    // yet), the panel's (f30_1430: [8E4E] counts), the columns' (f30_02d8:
    // a pushed ball goes up 6 a tick, sound 602A, till it's out: one ball
    // fewer, and the ball on the table (not yet)).
    ++panelTicks_;
    ++timerTicks_;
    if (hasBall_) ballTick();
    if (hasBall_ && std::getenv("SCI_DEBUG")) {
        // (Testing: where the ball's sprite is drawn, and which.)
        const Ball& b = ball_;
        const auto [bx, by] = objectCentre(b.cx - b.r, b.cy - b.r, b.cz - b.r, 2 * b.r + 1, 2 * b.r + 1, 2 * b.r + 1);
        logLine("t" + std::to_string(timerTicks_) + " sprite " + std::to_string(bx - 16) + "," + std::to_string(by - 10) + " f" +
                std::to_string(b.drawFrame >> 1) + " c " + std::to_string(b.cx) + "," + std::to_string(b.cy) + "," + std::to_string(b.cz) +
                " v " + std::to_string(b.v[0]) + "," + std::to_string(b.v[1]) + "," + std::to_string(b.v[2]));
    }
    for (int c = 0; c < 2; ++c) {
        Column& col = columns_[c];
        if (!col.pushing || col.offset == 0) {
            if (col.pushing) col.pushing = false, columnDirty_[c] = true;
            continue;
        }
        sound(0x602A);
        col.offset += 6;
        const int y = c ? 0x8C : 6, h = c ? 0x96 : 0xFA;
        const int count = c ? rightBalls_ : leftBalls_;
        const int topBall = y + h - 0x1D - 0x18 * (count - 1);
        columnDirty_[c] = true;
        if (col.offset < topBall - y) continue;
        col.offset = 0;
        if (c) rightBalls_ = std::max(rightBalls_ - 1, 0);
        else leftBalls_ = std::max(leftBalls_ - 1, 0);
        col.ballOut = true;
    }
}

void Science::flushRoom() {
    if (viewDirty_) redrawTable(table_.view), viewDirty_ = false;
    if (panelDirty_) drawPanel(&panelDirtyRect_), panelDirty_ = false;
    for (int c = 0; c < 2; ++c)
        if (columnDirty_[c]) drawColumn(c == 1), columnDirty_[c] = false;
}

}  // namespace edison
