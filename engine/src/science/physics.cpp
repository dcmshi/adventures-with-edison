// WMAIN.EXE: the ball's physics. A ball is a core (segment 8: its velocity,
// the forces on it, the step that moves it, f08_1a42), a motion part
// (segment 7: its sphere, the rolling frames, the launch) and the maths of
// segments 11, 34, 84, 86 and 87 (vectors in 7FFEhs, binary angles). All
// of it is integer arithmetic, 16-bit words and 32-bit products, kept as
// the original has it (each value stored to a word is cut to 16 bits). See
// docs/SCIENCE.md.

#include <algorithm>
#include <cstdlib>

#include "science/science.h"

namespace edison {

namespace {

int16_t w(int32_t v) { return static_cast<int16_t>(v); }
int32_t iabs32(int32_t v) { return v < 0 ? -v : v; }
int16_t iabs16(int16_t v) { return w(v < 0 ? -v : v); }

}  // namespace

// --- the library's maths --------------------------------------------------------

int Science::libCos4096(int angle) const {
    // f86_106d: the cosine in 4096ths (the sine table >> 4).
    uint16_t bx = static_cast<uint16_t>(angle) + 0x4000;
    const uint8_t dl = static_cast<uint8_t>(bx >> 8);
    size_t at = (bx >> 2) & 0xFFE;
    if (dl & 0x40) at = 0xFFE - at;
    int v = static_cast<uint16_t>(sines_[at] | sines_[at + 1] << 8) >> 4;
    if (dl & 0x80) v = -v;
    return w(v);
}

int32_t Science::libLength(const int16_t v[3]) const {
    // f11_0000: across (x, y) by the cosine of its heading, then with z.
    int32_t r;
    if (v[0] == 0) {
        r = iabs16(v[1]);
    } else {
        const int c = libCos4096(libAtan2(v[0], v[1]));
        r = c ? (static_cast<int32_t>(v[0]) << 12) / c : iabs16(v[1]);
    }
    r = std::clamp<int32_t>(r, -0x7FFF, 0x7FFF);
    const int16_t across = w(r);
    if (across == 0) return iabs16(v[2]);
    const int c2 = libCos4096(libAtan2(across, v[2]));
    if (c2 == 0) return iabs16(v[2]);
    return (static_cast<int32_t>(across) << 12) / c2;
}

void Science::libUnit(const int16_t v[3], int16_t out[3]) const {
    const int in[3] = {v[0], v[1], v[2]};
    int o[3];
    libNormalize(in, o);
    for (int k = 0; k < 3; ++k) out[k] = w(o[k]);
}

int16_t Science::libDot(const int16_t a[3], const int16_t b[3]) const {
    // f11_0651: b along a's direction (a normalised, f84_0000).
    if (!a[0] && !a[1] && !a[2]) return 0;
    int16_t u[3];
    libUnit(a, u);
    return w((static_cast<int32_t>(u[0]) * b[0] + static_cast<int32_t>(u[1]) * b[1] +
              static_cast<int32_t>(u[2]) * b[2]) / 0x7FFE);
}

void Science::libScaleTo(int16_t v[3], int length) const {
    // f11_02f7: v's direction, `length` long (a zero vector stays).
    if (!v[0] && !v[1] && !v[2]) return;
    int16_t u[3];
    libUnit(v, u);
    for (int k = 0; k < 3; ++k) v[k] = w(static_cast<int32_t>(u[k]) * length / 0x7FFE);
}

void Science::libCross(const int16_t a[3], const int16_t b[3], int16_t out[3]) const {
    // f11_043e: a (normalised) x b, in 7FFEhs.
    if (!a[0] && !a[1] && !a[2]) {
        out[0] = out[1] = out[2] = 0;
        return;
    }
    int16_t u[3];
    libUnit(a, u);
    out[0] = w((static_cast<int32_t>(u[1]) * b[2] - static_cast<int32_t>(u[2]) * b[1]) / 0x7FFE);
    out[1] = w((static_cast<int32_t>(u[2]) * b[0] - static_cast<int32_t>(u[0]) * b[2]) / 0x7FFE);
    out[2] = w((static_cast<int32_t>(u[0]) * b[1] - static_cast<int32_t>(u[1]) * b[0]) / 0x7FFE);
}

// --- the faces ----------------------------------------------------------------------

Science::Face Science::faceUnder(int x, int y) const {
    // f27_0903 (f12_4284, f12_443b): the deepest box whose bottom has the
    // point, the face of it there.
    const Box* box = &table_.root;
    for (bool deeper = true; deeper;) {
        deeper = false;
        for (const auto& child : box->children)
            if (inside(child->bottom, x, y)) {
                box = child.get(), deeper = true;
                break;
            }
    }
    return {box, faceAt(*box, x, y)};
}

int Science::faceHeight(const Face& f, int x, int y) const {
    // f34_07ec (none: the stand-in at DS:2950, height 0).
    if (!f.box || f.type == 0) return 0;
    return heightAt(*f.box, x, y);
}

void Science::faceNormal(const Face& f, int16_t n[3], int32_t* length, int32_t* cosine) const {
    // f34_016a / f34_0a10: flat (0, 0, 100); a slope (its rise, across its
    // width), normalised; its length; the cosine of its tilt.
    n[0] = n[1] = 0, n[2] = 100;
    bool alongY = false;
    if (f.box && f.type >= 2 && f.type <= 5) {
        const Box& b = *f.box;
        const Rect& T = b.top;
        const Rect& B = b.bottom;
        int edge, foot;
        switch (f.type) {
            case 2: edge = T.x, foot = B.x; break;
            case 3: edge = T.x + T.w, foot = B.x + B.w; break;
            case 4: edge = T.y + T.h, foot = B.y + B.h, alongY = true; break;
            default: edge = T.y, foot = B.y, alongY = true; break;
        }
        const int16_t d = w(foot - edge);
        int16_t rise = w(b.height - b.parentHeight());
        if (d < 0) rise = w(-rise);
        n[0] = alongY ? 0 : rise;
        n[1] = alongY ? rise : 0;
        n[2] = iabs16(d);
    } else if (!f.box || f.type == 0) {
        n[2] = 0;  // the stand-in: no face
    }
    if (n[0] || n[1] || n[2]) libUnit(n, n);  // f11_01f2
    if (length) *length = libLength(n);
    if (cosine) {
        int a = libAtan2(alongY ? n[1] : n[0], n[2]);
        a = a <= 0x4000 ? 0x4000 - a : w(a + 0xC000);
        *cosine = libSinCos(a).second;
    }
}

void Science::gravityAlongSlope(const Face& f, int32_t num, int32_t den, int16_t out[3]) const {
    // f34_0b8a: (0, 0, num / den) less its part across the face.
    int16_t n[3];
    int32_t len, cosine;
    faceNormal(f, n, &len, &cosine);
    const int32_t s = w((num * cosine) / (den * 0x7FFF));
    out[0] = w(-(n[0] * s / len));
    out[1] = w(-(n[1] * s / len));
    out[2] = w(num / den - (n[2] * s / len));
}

void Science::projectOnFace(const Face& f, const int16_t v[3], int16_t out[3]) const {
    // f34_0d40: v along the face (n x v, then that x n, v's part on it).
    int16_t n[3];
    faceNormal(f, n, nullptr, nullptr);
    int16_t t1[3], t2[3];
    libCross(n, v, t1);
    if (!t1[0] && !t1[1] && !t1[2]) {
        out[0] = out[1] = out[2] = 0;
        return;
    }
    libCross(t1, n, t2);
    const int16_t d = libDot(t2, v);
    libScaleTo(t2, d);
    out[0] = t2[0], out[1] = t2[1], out[2] = t2[2];
}

void Science::acrossFace(const Face& f, const int16_t v[3], int16_t out[3]) const {
    // f34_0e4c: v less its part along the face.
    int16_t along[3];
    projectOnFace(f, v, along);
    for (int k = 0; k < 3; ++k) out[k] = w(v[k] - along[k]);
}

// --- the ball ----------------------------------------------------------------------

void Science::ballSetForce(const int16_t f[3]) {
    // f08_13a2: the kick (+46), each within +-7FFFh.
    for (int k = 0; k < 3; ++k) ball_.kick[k] = w(std::clamp<int32_t>(f[k], -0x7FFF, 0x7FFF));
}

void Science::ballMove(const int16_t d[3]) {
    // f08_0aab → f07_04d5: the sphere's centre moved, its box after it.
    ball_.cx = w(ball_.cx + d[0]);
    ball_.cy = w(ball_.cy + d[1]);
    ball_.cz = w(ball_.cz + d[2]);
    viewDirty_ = true;
}

bool Science::ballDampsOthers() const {
    // f08_1802: not in rooms 14, 60, 64, 69 and 91.
    const int r = currentRoom_;
    return !(r == 14 || r == 60 || r == 64 || r == 69 || r == 91);
}

void Science::ballBounce(const int16_t normal[3], bool always) {
    // f08_1843 (the player's ball only): how hard it hit (along the
    // normal): too hard for its kind (+21) and it breaks (+7C); else, at
    // most every 2 timer ticks, a sound by its speed (6004, 6001, 6002).
    const int16_t v[3] = {ball_.v[0], ball_.v[1], ball_.v[2]};
    const int16_t along = iabs16(libDot(normal, v));
    const int32_t level = static_cast<int32_t>(kTimerRate) * (along / 50) / kTimerK;
    if (ballKinds_[ball_.kind].fragility * level > 0x100) {
        ball_.state = 1;
        return;
    }
    if (!always) {
        if (ball_.lastHit == lastHitSound_) return;
        lastHitSound_ = ball_.lastHit;
    }
    if (std::abs(timerTicks_ - lastBounceTick_) < 2) return;
    lastBounceTick_ = timerTicks_;
    const int32_t speed = libLength(v);
    int band = static_cast<int16_t>(speed / 100) * 2 / 4;
    if (band < 1) sound(0x6004);
    else if (band < 3) sound(0x6001);
    else sound(0x6002);
}

void Science::ballStep() {
    // f08_1a42: one tick of the ball. See docs/SCIENCE.md.
    Ball& b = ball_;
    if (b.state != 0) return;
    // The acceleration (f08_125c): the kick and the push, gravity on z.
    const int16_t accel[3] = {w(b.kick[0] + b.push[0]), w(b.kick[1] + b.push[1]), w(b.kick[2] + b.push[2] + gravity_)};
    int16_t a0[3] = {accel[0], accel[1], accel[2]};
    const int16_t bottom[3] = {w(b.cx), w(b.cy), w(b.cz - b.r)};
    const Face face0 = faceUnder(bottom[0], bottom[1]);
    int16_t n0[3];
    faceNormal(face0, n0, nullptr, nullptr);
    const int16_t ground0 = w(faceHeight(face0, bottom[0], bottom[1]));
    const int32_t pressing = static_cast<int32_t>(n0[0]) * accel[0] + static_cast<int32_t>(n0[1]) * accel[1] +
                             static_cast<int32_t>(n0[2]) * accel[2];
    if (bottom[2] > ground0 || pressing > 0) {
        // In the air (or pushed off): air friction, 1/100 across.
        b.v[0] = w(b.v[0] - w(static_cast<int32_t>(b.v[0]) * 1 / 100));
        b.v[1] = w(b.v[1] - w(static_cast<int32_t>(b.v[1]) * 1 / 100));
    } else {
        // On the face: the acceleration along it, the velocity along it
        // (when it isn't already), and the friction against the motion.
        if (n0[0] == 0 && n0[1] == 0) a0[2] = 0;
        else if (accel[0] == 0 && accel[1] == 0) gravityAlongSlope(face0, accel[2], 1, a0);
        else projectOnFace(face0, accel, a0);
        int16_t v[3] = {b.v[0], b.v[1], b.v[2]};
        if (iabs16(libDot(n0, v)) >= 50) projectOnFace(face0, v, v);
        if (v[0] || v[1] || v[2]) {
            const int16_t back[3] = {w(-v[0]), w(-v[1]), w(-v[2])};
            const int32_t fn = frictionNum_, fd = frictionDen_;
            const int16_t sumN = w(iabs16(w(accel[0] - a0[0])) + iabs16(w(accel[1] - a0[1])) + iabs16(w(accel[2] - a0[2])));
            const int16_t sumV = w(iabs16(back[0]) + iabs16(back[1]) + iabs16(back[2]));
            int16_t f[3];
            for (int k = 0; k < 3; ++k) {
                const int32_t p = static_cast<int32_t>(back[k]) * sumN;
                f[k] = w(p * fn / (static_cast<int32_t>(sumV) * fd));
                if (fn != 0 && p != 0 && f[k] == 0) f[k] = back[k] > 0 ? 1 : -1;
            }
            int16_t limit[3];
            for (int k = 0; k < 3; ++k) limit[k] = w(static_cast<int32_t>(iabs16(f[k])) * kStepNum / kStepDen);
            for (int k = 0; k < 3; ++k)
                if (iabs16(w(b.rem[k] + back[k])) < limit[k]) b.v[k] = 0, f[k] = 0;
            for (int k = 0; k < 3; ++k) a0[k] = w(a0[k] + f[k]);
        }
    }
    // The velocity, 10/100 of the acceleration a tick ([1010], [1014]),
    // within +-5000; the move in 50ths.
    int16_t nv[3], d[3];
    for (int k = 0; k < 3; ++k) {
        nv[k] = w((static_cast<int32_t>(b.v[k]) * kStepDen + static_cast<int32_t>(a0[k]) * kStepNum) / kStepDen);
        nv[k] = std::clamp<int16_t>(nv[k], -5000, 5000);
        d[k] = w(b.rem[k] + nv[k]);
    }
    // A long move is tried in steps of the radius * 75 + 1 and cut where
    // it would first go under the ground.
    const int16_t lim = w(w(b.r * 150) / 2 + 1);
    if (iabs16(d[0]) > lim || iabs16(d[1]) > lim) {
        const int16_t steps = w(libLength(d) / lim);
        for (int k = 1; k <= steps; ++k) {
            int16_t dk[3] = {d[0], d[1], d[2]};
            libScaleTo(dk, w(lim * k));
            const int16_t p[3] = {w(bottom[0] + dk[0] / 50), w(bottom[1] + dk[1] / 50), w(bottom[2] + dk[2] / 50)};
            const Face fp = faceUnder(p[0], p[1]);
            if (p[2] < faceHeight(fp, p[0], p[1])) {
                d[0] = dk[0], d[1] = dk[1], d[2] = dk[2];
                break;
            }
        }
    }
    int16_t q[3];
    for (int k = 0; k < 3; ++k) q[k] = w(d[k] / 50), b.rem[k] = w(d[k] % 50), b.disp[k] = q[k];
    for (int k = 0; k < 3; ++k) b.v[k] = nv[k];  // +2C (f08_1371)
    int16_t P[3] = {w(bottom[0] + q[0]), w(bottom[1] + q[1]), w(bottom[2] + q[2])};
    // The room's other objects (its list +18E, those whose +34 / +38 isn't
    // 0): one the new centre is within 35 of on each axis and within the
    // two radii of. A solid one (2 and up: other balls, not yet) bounces
    // it (f08_0d3e) and ends the step; a soft one, a hole (1), takes it
    // (its +34, f28_13fe: an idle hole swallows the player's ball,
    // f28_14a5) and the ball's move, remainder and kick are cleared (the
    // step goes on with its own copy of the move).
    if (!b.hidden)
        for (Object& o : table_.objects) {
            int s[4];
            if (o.type == 8) holeSphere(o, s);
            else if (o.type == 10 && o.live) {
                // A point target's sphere: its box's centre a unit lower,
                // radius 13 (f03_002c's f07_1082).
                const int g = heightUnder(o.x, o.y);
                s[0] = o.x + 13, s[1] = o.y + 13, s[2] = g + 12, s[3] = 13;
            } else continue;
            const int dx = s[0] - (b.cx + q[0]), dy = s[1] - (b.cy + q[1]), dz = s[2] - (b.cz + q[2]);
            if (dx >= 35 || dx <= -35 || dy >= 35 || dy <= -35 || dz >= 35 || dz <= -35) continue;
            const int64_t rr = static_cast<int64_t>(b.r + s[3]) * (b.r + s[3]);
            if (static_cast<int64_t>(dx) * dx + static_cast<int64_t>(dy) * dy + static_cast<int64_t>(dz) * dz > rr) continue;
            if (o.type == 10) {
                pointHit(o);
            } else if (!o.swallow && !o.leaving && !o.spit && !o.closed) {
                // f28_14a5: the ball stopped (f07_0ead → f08_0721) and
                // hidden with its shadow (f07_03be); the room busy (+F6F).
                for (int k = 0; k < 3; ++k) b.v[k] = 0, b.kick[k] = 0, b.push[k] = 0;
                b.kickTicks = 0;
                b.hidden = true, shadowShown_ = false;
                o.swallow = 1, o.spit = 0, o.swallowDone = false;
                if (std::getenv("SCI_DEBUG")) logLine("hole " + std::to_string(o.args[0]) + " takes the ball at t" + std::to_string(timerTicks_));
                roomBusy_ = true;
                viewDirty_ = true;
            }
            for (int k = 0; k < 3; ++k) b.disp[k] = 0, b.rem[k] = 0, b.kick[k] = 0;
        }
    if (!q[0] && !q[1] && !q[2]) {
        if (stepDepth_) --stepDepth_;
        return;
    }
    b.hitFlag = 0, b.lastHit = 0;
    const int32_t en = ballKinds_[b.kind].bounceNum, ed = ballKinds_[b.kind].bounceDen;
    auto scale = [&](int16_t v) { return w(static_cast<int32_t>(v) * en / ed); };
    auto flipScale = [&](int16_t v) { return w(static_cast<int32_t>(w(-v)) * en / ed); };
    // The world's sides: x, then y (the near one, the machine's glass, can
    // crack), else the ceiling (279).
    const int16_t wx[3] = {100, 0, 0}, wy[3] = {0, 100, 0}, wyb[3] = {0, -100, 0}, wz[3] = {0, 0, -100};
    if (P[0] < 0 || P[0] >= worldW_) {
        if (iabs16(b.v[0]) > 50) ballBounce(wx, true);
        P[0] = P[0] < 0 ? 0 : w(worldW_ - 1);
        b.v[0] = flipScale(b.v[0]), b.v[1] = scale(b.v[1]), b.v[2] = scale(b.v[2]);
        b.rem[0] = 0;
    }
    if (P[1] < 0) {
        if (iabs16(b.v[1]) > 50) {
            const int16_t v[3] = {b.v[0], b.v[1], b.v[2]};
            const int32_t hit = static_cast<int32_t>(iabs16(libDot(wy, v)) / 50) * kTimerRate / kTimerK;
            if (hit >= 16) crackGlass();
            ballBounce(wy, true);
        }
        P[1] = 0;
        b.v[1] = flipScale(b.v[1]), b.v[0] = scale(b.v[0]), b.v[2] = scale(b.v[2]);
        b.rem[1] = 0;
    } else if (P[1] >= worldD_) {
        if (iabs16(b.v[1]) > 50) ballBounce(wyb, true);
        P[1] = w(worldD_ - 1);
        b.v[1] = flipScale(b.v[1]), b.v[0] = scale(b.v[0]), b.v[2] = scale(b.v[2]);
        b.rem[1] = 0;
    } else if (P[2] >= 0x118) {
        if (iabs16(b.v[2]) > 50) ballBounce(wz, true);
        P[2] = 0x117;
        b.v[2] = flipScale(b.v[2]), b.v[0] = scale(b.v[0]), b.v[1] = scale(b.v[1]);
        b.rem[2] = 0;
    }
    const Face face = faceUnder(P[0], P[1]);
    const int16_t ground = w(faceHeight(face, P[0], P[1]));
    if (P[2] > ground) {
        if (ground + 1 < P[2]) b.onGround = false;
    } else {
        const int16_t centreZ = w(b.cz);
        if (ground > centreZ && !(face == face0)) {
            // A wall: which way, by the face under the move in y alone.
            int16_t v[3] = {b.v[0], b.v[1], b.v[2]};
            const int16_t ty = w(bottom[1] + q[1]);
            const int16_t g2 = w(faceHeight(faceUnder(bottom[0], ty), bottom[0], ty));
            if (g2 > centreZ) {
                if (iabs16(b.v[1]) > 50) {
                    const int16_t nrm[3] = {0, w(b.v[1] < 0 ? 100 : -100), 0};
                    ballBounce(nrm, true);
                }
                v[1] = flipScale(v[1]);
                if (ballDampsOthers()) v[0] = scale(v[0]), v[2] = scale(v[2]);
                b.rem[1] = 0;
            } else {
                if (iabs16(b.v[0]) > 50) {
                    const int16_t nrm[3] = {w(b.v[0] < 0 ? 100 : -100), 0, 0};
                    ballBounce(nrm, true);
                }
                v[0] = flipScale(v[0]);
                if (ballDampsOthers()) v[1] = scale(v[1]), v[2] = scale(v[2]);
                b.rem[0] = 0;
            }
            for (int k = 0; k < 3; ++k) b.v[k] = v[k], b.disp[k] = 0;
            ++stepDepth_;
            if (stepDepth_ < 2) ballStep();
            else --stepDepth_;
            if (stepDepth_) --stepDepth_;
            return;
        }
        P[2] = ground;
        const bool changed = !(face == face0);
        if (!b.onGround || changed) {
            // Onto another face, or landing: the velocity across the face
            // turned back (times the bounce; landing, at most 3/5).
            int32_t num = en, den = ed;
            if (!b.onGround) {
                b.onGround = true;
                if (num * 5 > den * 3) num = 3, den = 5;
            }
            const int16_t v[3] = {b.v[0], b.v[1], b.v[2]};
            int16_t n[3];
            acrossFace(faceUnder(P[0], P[1]), v, n);
            if (iabs16(b.v[2]) > 50) ballBounce(n, true);
            int16_t t[3], out[3];
            for (int k = 0; k < 3; ++k) t[k] = w(v[k] - n[k]), n[k] = w(-n[k]);
            for (int k = 0; k < 3; ++k) out[k] = w(w(static_cast<int32_t>(num) * n[k] / den) + t[k]);
            for (int k = 0; k < 3; ++k) b.v[k] = out[k], b.rem[k] = 0, b.disp[k] = 0;
            const int16_t delta[3] = {w(P[0] - bottom[0]), w(P[1] - bottom[1]), w(P[2] - bottom[2])};
            ballMove(delta);
            // (The room's method +24, told of the face: nothing in the base room.)
            if (stepDepth_) --stepDepth_;
            return;
        }
    }
    const int16_t delta[3] = {w(P[0] - bottom[0]), w(P[1] - bottom[1]), w(P[2] - bottom[2])};
    ballMove(delta);
    if (stepDepth_) --stepDepth_;
}

void Science::ballTick() {
    // f07_077e: the step, then the rolling frames (a turn of frames every
    // r * r / 16 * [DFA] / [DFE] of squared move across), the kick's last
    // tick.
    Ball& b = ball_;
    ballStep();
    if (b.state != 0) {
        // Breaking (+7C), on the room's even ticks ([FFE]): first its sound
        // (6019 glass, else 601A; 6028 with +28 set, not ported), then 13
        // frames (+1A, its +22 table: f13_01ce); then it's gone (+7C 0) and,
        // the player's ball, lost (f31_0504).
        if (roomTicks_ % 2 != 0) return;
        if (b.state == 1) {
            sound(b.kind == 4 ? 0x6019 : 0x601A);
            b.frame = 0, b.state = 2;
        } else {
            if (++b.frame > 13) b.frame = 0;
            if (++b.state > 13) {
                b.state = 0, b.frame = 0, b.drawFrame = 0;
                viewDirty_ = true;
                ballLost();
                return;
            }
        }
        b.drawFrame = b.frame;
        viewDirty_ = true;
        return;
    }
    b.rollAcc += static_cast<int16_t>(static_cast<int32_t>(b.disp[0]) * b.disp[0] + static_cast<int32_t>(b.disp[1]) * b.disp[1]);
    if (b.rollAcc > b.rollThreshold) {
        b.frame = b.frame + 1 > 11 ? 0 : b.frame + 1;
        b.rollAcc %= b.rollThreshold;
        b.drawFrame = b.frame;
        const int vx = b.v[0], vy = b.v[1];
        if ((vx < 1 && vy < 1) || (vx < 1 && vy >= 0 && vx <= -vy) || (vy < 1 && vx >= 0 && vy <= -vx))
            b.drawFrame = 11 - b.frame;
        viewDirty_ = true;
    }
    if (b.kickTicks != 0 && --b.kickTicks == 0) {
        const int16_t zero[3] = {0, 0, 0};
        ballSetForce(zero);
    }
}

void Science::ballLaunch(int tx, int ty, int tz) {
    // f07_0ca0: towards the point, `power` long, lifted by 42 * 9000 /
    // (its mass * 50); for a tick (two at full power).
    Ball& b = ball_;
    if (b.kickTicks != 0) {
        const int16_t zero[3] = {0, 0, 0};
        ballSetForce(zero);
    }
    int16_t dir[3] = {w(tx - b.cx), w(ty - b.cy), w(tz - (b.cz - b.r))};
    libScaleTo(dir, power_);
    dir[2] = w(dir[2] + static_cast<int32_t>(kTimerK) * 9000 / (ballKinds_[b.kind].mass * kTimerRate));
    b.kickTicks = power_ >= kMaxPower ? 2 : 1;
    ballSetForce(dir);
}

}  // namespace edison
