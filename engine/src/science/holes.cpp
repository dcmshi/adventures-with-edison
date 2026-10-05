// Wild Science Arcade: the table's holes (segment 28): the ball swallowed
// and spat out, and where it goes. See docs/SCIENCE.md.

#include <algorithm>
#include <cstdlib>

#include "artech/context.h"
#include "science/science.h"

namespace edison {

namespace {

constexpr int kHoleTicks = 22;  // [212C]: the frames of a swallow

}  // namespace

void Science::holeSphere(const Object& o, int out[4]) const {
    // f28_00f3: the hole's sphere (its +2, read by f07_11b1): its box's
    // centre (the box a cube of side 2r, r to the left on the left wall;
    // a unit lower), radius r / 2.
    const int wall = o.args[1], big = o.args[2] != 0;
    const int r = big ? 17 : 11;
    const int z = o.args[3] != -1 ? o.args[3] : heightUnder(o.x, o.y);
    out[0] = o.x - (wall == 0 ? r : 0) + r;
    out[1] = o.y + r;
    out[2] = z + r - 1;
    out[3] = r >> 1;
}

void Science::holeTick(Object& o) {
    // f28_0671.
    Ball& b = ball_;
    if (o.leaving) {
        // The ball on its way out (+25) till it's clear of the hole (f11_1732).
        int s[4];
        holeSphere(o, s);
        const int64_t dx = s[0] - b.cx, dy = s[1] - b.cy, dz = s[2] - b.cz, r = s[3] + b.r;
        if (dx * dx + dy * dy + dz * dz > r * r) o.leaving = false;
        return;
    }
    if (o.swallow && hasBall_) {
        // Swallowing (+21): the ball stopped (its +2C's +10, f07_0ead →
        // f08_0721: velocity, remainder and kick, f08_13a2) and kept hidden
        // (f07_0381), a frame a tick (so a field doesn't move it); a tick
        // after the last (+27), the room's word (+20).
        if (!o.swallowDone) {
            for (int k = 0; k < 3; ++k) b.v[k] = 0, b.rem[k] = 0, b.kick[k] = 0;
            b.hidden = true, shadowShown_ = false;
            if (++o.swallow > kHoleTicks) o.swallowDone = true;
            viewDirty_ = true;
            return;
        }
        o.swallowDone = false, o.swallow = 0;
        holeEntered(o);
        // Then the ball out of the way (f08_056e: 0, 0, its bottom at 400).
        if (!o.spit) b.cx = 0, b.cy = 0, b.cz = 400 + b.r;
        viewDirty_ = true;
        return;
    }
    if (o.spit) {
        // Spitting out (+23): the ball stopped and hidden as above, the
        // frames backwards (only the first for modes 1 and 2), then the
        // ball out (+29).
        if (!o.spitDone) {
            for (int k = 0; k < 3; ++k) b.v[k] = 0, b.rem[k] = 0, b.kick[k] = 0;
            b.hidden = true, shadowShown_ = false;
            if (++o.spit > kHoleTicks || o.spitMode == 1 || o.spitMode == 2) o.spitDone = true;
            viewDirty_ = true;
            return;
        }
        roomBusy_ = false;  // f13_05d4, +F6F
        o.spitDone = false, o.spit = 0;
        // Then the ball out by the mode (+1F): 2 back where the room put it
        // (f27_287d); 1 set down in front of the hole (on the back wall
        // 4r towards the front, on the left wall 4r to the right; f08_056e
        // on the ground); 0 at the hole's mouth, its bottom the sphere's,
        // and shot 50 out of the wall (f27_27a3, no shot counted).
        int s[4];
        holeSphere(o, s);
        const int wall = o.args[1], r = o.args[2] ? 17 : 11, sr = s[3];
        Ball& ball = ball_;
        if (o.spitMode == 2) {
            ballToStart();
        } else {
            int x, y;
            if (o.spitMode == 1) x = wall ? s[0] - sr + r : s[0] + 4 * r, y = wall ? s[1] - sr - 4 * r : s[1] + 1;
            else x = wall ? s[0] - sr : s[0], y = wall ? s[1] - sr - 10 : s[1] + 1;
            ballToStart();
            ball.cx = x, ball.cy = y;
            ball.cz = (o.spitMode == 1 ? heightUnder(x, y) : s[2] - sr) + ball.r;
            lastCentre_[0] = shadowSeen_[0] = ball.cx, lastCentre_[1] = shadowSeen_[1] = ball.cy, lastCentre_[2] = shadowSeen_[2] = ball.cz;
            if (o.spitMode == 0) {
                // The hole's place (its method 5: its sphere's centre; the
                // original's launch is the same to the unit), 50 out of
                // its wall.
                int px = s[0], py = s[1];
                const int pz = s[2];
                if (wall) py -= 50;
                else px += 50;
                if (std::getenv("SCI_DEBUG")) logLine("spit 0 from " + std::to_string(ball.cx) + "," + std::to_string(ball.cy) + "," + std::to_string(ball.cz) + " at " + std::to_string(px) + "," + std::to_string(py) + "," + std::to_string(pz));
                ballLaunch(px, py, pz);
            }
        }
        o.leaving = true;
        viewDirty_ = true;
    }
}

void Science::holeEntered(Object& o) {
    // f28_156a: the room's method 8 with the hole and the ball (rooms.cpp),
    // which mostly ends in f27_2530 (holeGo).
    roomHole(o);
}

void Science::holeGo(Object& o) {
    // f27_2530: back out if it leads to this room (mode 0); a door within
    // the room (0, 100-500) passes the ball to its other half (not
    // ported); else event 9: that room, from this hole.
    const int room = o.args[0];
    if (room == currentRoom_) {
        spitBall(o, 0);
        return;
    }
    if (room == 0 || (room >= 100 && room <= 500)) {
        logLine("Wild Science Arcade: doors aren't ported yet");
        spitBall(o, 0);
        return;
    }
    exitRoom_ = room;
    exitHole_ = static_cast<int>(&o - table_.objects.data());
}

void Science::spitBall(Object& o, int mode) {
    // f28_1445: the ball stopped (f07_0ead) and hidden (f07_03be); the
    // hole spits it out (+1F the mode, +23 counting).
    Ball& b = ball_;
    for (int k = 0; k < 3; ++k) b.v[k] = 0, b.rem[k] = 0, b.kick[k] = 0, b.push[k] = 0;
    b.kickTicks = 0;
    b.hidden = true, shadowShown_ = false;
    o.spitMode = mode, o.swallow = 0, o.spit = 1, o.spitDone = false;
    roomBusy_ = true;
    viewDirty_ = true;
}

void Science::ballToStart() {
    // f27_287d: the ball where the room put it (its +4's +8), on the face
    // under it; shown.
    Ball& b = ball_;
    b.cx = b.startX, b.cy = b.startY;
    b.cz = faceHeight(faceUnder(b.cx, b.cy), b.cx, b.cy) + b.r;
    for (int k = 0; k < 3; ++k) b.v[k] = 0, b.rem[k] = 0, b.disp[k] = 0, b.kick[k] = 0, b.push[k] = 0;
    b.kickTicks = 0;
    b.hidden = false;
    lastCentre_[0] = shadowSeen_[0] = b.cx, lastCentre_[1] = shadowSeen_[1] = b.cy, lastCentre_[2] = shadowSeen_[2] = b.cz;
    shadowShown_ = false;
    viewDirty_ = true;
}

void Science::ballLost() {
    // f31_0504: the ball back to its place (f27_287d), then away (hidden,
    // f08_056e to 0, 0); no shots; the left column's PUSH ready (+132,
    // +134 cleared) if it has balls, else the right's; else the game's
    // over: the high scores (f40_068b, then event 9 to 502; not ported).
    ballToStart();
    Ball& b = ball_;
    b.cx = 0, b.cy = 0, b.cz = heightUnder(0, 0) + b.r;  // f08_056e: on the ground at 0, 0
    b.hidden = true, shadowShown_ = false;
    shots_ = 0;
    viewDirty_ = true;
    for (int c = 0; c < 2; ++c) {
        if ((c ? rightBalls_ : leftBalls_) == 0) continue;
        columns_[c].ballOut = false, columns_[c].pushing = false;
        columnDirty_[c] = true;
        return;
    }
    logLine("Wild Science Arcade: no balls left: the high scores aren't ported; a new game");
    leftBalls_ = 7, rightBalls_ = 0, totalScore_ = 0;
    std::fill(std::begin(gameFlag_), std::end(gameFlag_), 0);  // f31_1b48
    exitRoom_ = 1;
}

void Science::dropBall(bool right) {
    // f30_02d8, the pushed ball out of its column: the ball made its type
    // again and shown at the column's top (f08_056e: x 279 + 10 or
    // 809 - 10, y 0, its bottom at 250 or 140); then f27_293b slides it to
    // where the room puts it (+F73, +F75, on the ground): 2 a step on each
    // axis (1 when nearer), the room drawn again each step, nothing else
    // running meanwhile ([14E0] set: no shadows; [200A] clear).
    Ball& b = ball_;
    b.kind = panel_.ballType;
    b.state = 0, b.frame = 0, b.drawFrame = 0, b.rollAcc = 0;
    for (int k = 0; k < 3; ++k) b.v[k] = 0, b.rem[k] = 0, b.disp[k] = 0, b.kick[k] = 0, b.push[k] = 0;
    b.kickTicks = 0;
    b.hidden = false;
    int x = right ? 809 - 10 : 279 + 10, y = 0, z = right ? 140 : 250;
    const int gz = faceHeight(faceUnder(b.startX, b.startY), b.startX, b.startY);
    shadowShown_ = false;
    noShadow_ = true;
    auto toward = [](int& v, int to) {
        const int d = to - v;
        if (d > -2 && d < 2) v = to;
        else v += d > 0 ? 2 : -2;
    };
    // (Under winevdm a step takes about half a millisecond, so the slide is
    // over in some 60 ms: the port keeps that pace, showing a frame every
    // 16 ms.)
    const uint64_t start = ctx_.platform.milliseconds();
    uint64_t shown = 0;
    for (int step = 0;; ++step) {
        b.cx = x, b.cy = y, b.cz = z + b.r;
        if (std::getenv("SCI_DEBUG"))
            logLine("drop c " + std::to_string(b.cx) + "," + std::to_string(b.cy) + "," + std::to_string(b.cz) + " v 0,0,0");
        viewDirty_ = true;
        uint64_t now = ctx_.platform.milliseconds();
        while (now < start + static_cast<uint64_t>(step / 2)) now = ctx_.platform.milliseconds();
        if (now >= shown + 16 || (x == b.startX && y == b.startY && z == gz)) {
            flushRoom();
            ctx_.pump();
            shown = now;
        }
        if (x == b.startX && y == b.startY && z == gz) break;
        toward(x, b.startX), toward(y, b.startY), toward(z, gz);
    }
    noShadow_ = false;
    lastCentre_[0] = shadowSeen_[0] = b.cx, lastCentre_[1] = shadowSeen_[1] = b.cy, lastCentre_[2] = shadowSeen_[2] = b.cz;
    viewDirty_ = true;
}

void Science::growCracks() {
    // f27_2434, every 20 room ticks: each mark below stage 3 a stage on
    // (drawn as 1164 at stage 1, 1165 after).
    for (int i = 0; i < 5; ++i)
        if (crackStage_[i] > 0 && crackStage_[i] < 3) ++crackStage_[i], viewDirty_ = true;
}

}  // namespace edison
