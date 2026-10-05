// Wild Science Arcade: the room's other objects (segments 2-5 and 7, made
// by f61_011d / f61_09bd), as far as they're ported. See docs/SCIENCE.md.

#include <algorithm>
#include <cstdlib>

#include "science/science.h"

namespace edison {

void Science::thingsBuilt() {
    // Each object's own setup as the builder makes it (f61_011d, f61_09bd).
    for (size_t i = 0; i < table_.objects.size(); ++i) {
        Object& o = table_.objects[i];
        if (o.type == 4 || o.type == 5) {
            // A magnet (f05_1749, type 5 f05_210d: the same, but it doesn't
            // move): a its size and strength (a byte), b its pole (+1 N, -1
            // S). Its box a cube of side 2a, its corner at (x, y) on the
            // ground (f08_0384); its sphere at the box's centre a unit lower,
            // radius a (f07_0ed0); its level (f05_1e60: 0-4 by a above 7, 9,
            // 12, 16), its mass 3 level + 3; its strength (f05_1902): the
            // level's of DS:728 (70, 210, 420, 700, 980: made at start-up
            // with [27B0] still 30) times [27B2] times b, over [27B0]; its
            // type Iron (+52 28DE, f08_077f).
            const int a = static_cast<int8_t>(o.args[0]), pole = o.args[1];
            o.level = a > 16 ? 4 : a > 12 ? 3 : a > 9 ? 2 : a > 7 ? 1 : 0;
            static const int32_t kStrength[5] = {70, 210, 420, 700, 980};
            Ball& b = o.body;
            b = Ball{};
            b.r = a;
            const int z = heightUnder(o.x, o.y);
            b.cx = o.x + a, b.cy = o.y + a, b.cz = z + a - 1;
            b.kind = 3, b.mass = 3 * o.level + 3, b.self = static_cast<int>(i);
            b.magnetic = true;
            b.strengthNum = kStrength[o.level] * kTimerK * pole, b.strengthDen = kTimerRate;
            continue;
        }
        if (o.type == 15 || o.type == 6) {
            // A magnet on a wall (f05_233e; type 6 f05_26f7, the same on a
            // switch's power): a its size and strength, b its pole, c its
            // wall (+20), d its height (-1: on the ground). Its core's box
            // (f05_2243) at (x, y, the ground): d 2a + 6 deep, 2a high, 2a
            // + 6 wide (2a + 18 on the left wall, c 0); its sphere (its
            // motion part's, f07_0ed0, made after the core: the core box's
            // centre a unit lower) radius a - 1 (its own box made with a -
            // 1); then, with a height, its centre at d + half its core's
            // height (its +8). Level, strength, mass and type as type 4's
            // (f05_1749).
            const int a = static_cast<int8_t>(o.args[0]), pole = o.args[1], wall = o.args[2], height = o.args[3];
            o.level = a > 16 ? 4 : a > 12 ? 3 : a > 9 ? 2 : a > 7 ? 1 : 0;
            static const int32_t kStrength[5] = {70, 210, 420, 700, 980};
            o.wall = wall, o.powered = false;
            const int g = heightUnder(o.x, o.y);
            o.coreBox[0] = o.x, o.coreBox[1] = o.y, o.coreBox[2] = g;
            o.coreBox[3] = 2 * a + 6 + (wall == 0 ? 12 : 0), o.coreBox[4] = 2 * a + 6, o.coreBox[5] = 2 * a;
            Ball& b = o.body;
            b = Ball{};
            b.r = a - 1;
            b.cx = o.x + (o.coreBox[3] >> 1), b.cy = o.y + (o.coreBox[4] >> 1), b.cz = g + (o.coreBox[5] >> 1) - 1;
            if (height != -1) {
                const int cz = o.coreBox[5] / 2 + height;
                o.coreBox[2] += cz - b.cz;
                b.cz = cz;
            }
            b.kind = 3, b.mass = 3 * o.level + 3, b.self = static_cast<int>(i);
            b.magnetic = true;
            b.strengthNum = kStrength[o.level] * kTimerK * pole, b.strengthDen = kTimerRate;
            continue;
        }
        if (o.type != 7) continue;
        // Type 7 (f04_01a1 and its kinds by d): a, b its height (-1 the
        // face's under it), c on at the start (method 3 with 1), d the kind,
        // f its +2. Only d = 3 (f04_05b8, RETRY) needs no power (+F94).
        o.state = 0, o.ticks = 0;
        o.sprites = o.args[3] == 3 ? 0x15B2 : o.args[3] == 1 ? 0x15BA : o.args[3] == 2 ? 0 : 0x15AA;
        if (o.args[3] == 3) {
            // f04_05b8: the game's score (f06_0000: [BD8]) and the player's
            // balls (+BA, +BC) as they are now.
            o.savedScore = totalScore_;
            o.savedBalls[0] = leftBalls_, o.savedBalls[1] = rightBalls_;
        }
    }
}

bool Science::thingBox(const Object& o, int box[6]) const {
    // An object's box (its core's +6E: x, y, z, w, d, h).
    if (o.type == 15 || o.type == 6) {
        for (int k = 0; k < 6; ++k) box[k] = o.coreBox[k];
        return true;
    }
    if (o.type == 4 || o.type == 5) {
        // A magnet's follows its sphere (f07_04d5).
        const Ball& b = o.body;
        box[3] = box[4] = box[5] = 2 * b.r;
        box[0] = b.cx - b.r, box[1] = b.cy - b.r, box[2] = b.cz - b.r + 1;
        return true;
    }
    if (o.type == 7) {
        // f04_00ba: a 34 cube at (x, y, b), b -1 the face's under it.
        box[0] = o.x, box[1] = o.y, box[2] = o.args[1] == -1 ? heightUnder(o.x, o.y) : o.args[1];
        box[3] = box[4] = box[5] = 0x22;
        return true;
    }
    return false;
}

uint16_t Science::thingSprite(const Object& o) const {
    if (o.type == 15 || o.type == 6) {
        // f13_0823 (DS:1532) / f13_0937 (DS:155A, powered 1582): by the wall
        // (10 on for c 1), the pole and the level.
        const size_t table = o.type == 15 ? 0x1532u : o.powered ? 0x1582u : 0x155Au;
        const size_t at = table + 0x14u * static_cast<size_t>(o.wall != 0) + (o.body.strengthNum < 0 ? 10u : 0u) + 2u * static_cast<size_t>(o.level);
        return static_cast<uint16_t>(data_[at] | data_[at + 1] << 8);
    }
    if (o.type == 4 || o.type == 5) {
        // f13_0720: DS:151E, by the pole (S: 5 on) and the level.
        const size_t at = 0x151E + (o.body.strengthNum < 0 ? 10u : 0u) + 2u * static_cast<size_t>(o.level);
        return static_cast<uint16_t>(data_[at] | data_[at + 1] << 8);
    }
    // f13_0a5b / f13_0b03: the switch's +14 table, a's pair, by its state.
    const size_t at = static_cast<size_t>(o.sprites) + 4u * static_cast<size_t>(o.args[0]) + 2u * static_cast<size_t>(o.state != 0);
    return static_cast<uint16_t>(data_[at] | data_[at + 1] << 8);
}

void Science::switchSet(Object& o, int on) {
    // f04_008e: its state (+4), and its area redrawn (its core's +10).
    o.state = on;
    viewDirty_ = true;
}

void Science::thingTick(Object& o) {
    if (o.type == 4) {
        // A loose magnet's step (its core's +00: f05_1f76 → f08_1a42).
        ballStep(o.body);
        return;
    }
    if (o.type == 7 && o.args[3] == 3) {
        // RETRY's step (f04_06e3): its +16 counts the ticks; on the sixth
        // it's off again.
        if (o.ticks++ == 5) switchSet(o, 0);
    }
}

bool Science::thingClick(Object& o, const Mouse& m) {
    // The room's mouse (f27_2d15): the first object whose rectangle has the
    // point, its core's +08; one that takes it ends there.
    if (o.type != 7) return false;
    int b[6];
    thingBox(o, b);
    const Rect r = objectRect(b[0], b[1], b[2], b[3], b[4], b[5]);
    if (m.x < r.x || m.x >= r.x + r.w || m.y < r.y || m.y >= r.y + r.h) return false;
    // f04_04bf: kinds 1 and 2 take no clicks; kind 0's (a switch on the
    // power, +F94) isn't ported yet.
    if (o.args[3] != 3) return false;
    // f04_03ab: a press switches it over (its method 3), and is taken.
    if (!m.click) return false;
    const int on = o.state ? 0 : 1;
    if (o.args[3] == 3) {
        // RETRY (f04_070e): only while neither column waits for its PUSH
        // (+132): the switch, the area redrawn; the player's balls and the
        // game's score as they were (f06_028a: the room's box too), no
        // completion bonus (+F7B), and event 9 to this room again.
        if (!columns_[0].ballOut || !columns_[1].ballOut) return true;
        switchSet(o, on);
        leftBalls_ = o.savedBalls[0], rightBalls_ = o.savedBalls[1];
        totalScore_ = o.savedScore, score_ = o.savedScore;
        completionBonus_ = 0;
        exitRoom_ = currentRoom_;
    }
    return true;
}

bool Science::contactOf(Object& o, Contact& c) {
    // An object as a body's step meets it (f08_1a42): its sphere (+4C), its
    // mass ratio (+34 / +38; 0: out of reach), its body if it has one.
    if (o.type == 8) {
        // A hole: soft; while the ball leaves it, out of reach (f28_0671).
        holeSphere(o, c.s);
        c.ratio = o.leaving ? 0 : 1;
        return true;
    }
    if (o.type == 10 && o.args[2] != 6) {
        // A point target: its box's centre a unit lower, radius 13 (f03_002c's
        // f07_1082); soft while live.
        const int g = heightUnder(o.x, o.y);
        c.s[0] = o.x + 13, c.s[1] = o.y + 13, c.s[2] = g + 12, c.s[3] = 13;
        c.ratio = o.live ? 1 : 0;
        return true;
    }
    if ((o.type == 1 || o.type == 3) && hasBall_) {
        // The player's ball: its mass by its type (f07_05bf).
        c.s[0] = ball_.cx, c.s[1] = ball_.cy, c.s[2] = ball_.cz, c.s[3] = ball_.r;
        c.ratio = ballKinds_[ball_.kind].mass;
        c.body = &ball_, c.movable = true;
        return true;
    }
    if (o.type == 4 || o.type == 5 || o.type == 15 || o.type == 6) {
        // A magnet: its mass 3 level + 3; a loose one's velocity takes the
        // change (types 5, 15 and 6's +2C do nothing).
        const Ball& b = o.body;
        c.s[0] = b.cx, c.s[1] = b.cy, c.s[2] = b.cz, c.s[3] = b.r;
        c.ratio = b.mass;
        c.body = &o.body, c.movable = o.type == 4;
        return true;
    }
    if (o.type == 7) {
        // A switch: its sphere at its box's centre a unit lower, radius 10
        // (f04_01a1's f10_1523); the bullseyes (kinds 1 and 2) 20, the lever
        // and RETRY 0. It doesn't move (its +2C does nothing).
        int b[6];
        thingBox(o, b);
        c.s[0] = b[0] + (b[3] >> 1), c.s[1] = b[1] + (b[4] >> 1), c.s[2] = b[2] + (b[5] >> 1) - 1, c.s[3] = 10;
        c.ratio = o.args[3] == 1 || o.args[3] == 2 ? 20 : 0;
        return true;
    }
    return false;
}

void Science::contactMet(Object& o, Ball& by) {
    // The object's +34: what it does when a body meets it.
    const bool player = &by == &ball_;
    if (o.type == 8) {
        // f28_13fe: an idle hole takes the player's ball (f28_14a5, unless
        // shut, +2F): stopped (f07_0ead → f08_0721), hidden with its shadow
        // (f07_03be), the room busy (+F6F).
        if (!player || o.swallow || o.leaving || o.spit || o.closed) return;
        Ball& b = ball_;
        for (int k = 0; k < 3; ++k) b.v[k] = 0, b.kick[k] = 0, b.push[k] = 0;
        b.kickTicks = 0;
        b.hidden = true, shadowShown_ = false;
        o.swallow = 1, o.spit = 0, o.swallowDone = false;
        if (std::getenv("SCI_DEBUG")) logLine("hole " + std::to_string(o.args[0]) + " takes the ball at t" + std::to_string(timerTicks_));
        roomBusy_ = true;
        viewDirty_ = true;
        return;
    }
    if (o.type == 10) {
        // f03_0593: only the player's ball scores (kind 3 hides another
        // body, its mass 0).
        if (player) pointHit(o);
        else if (o.kind == 3) by.hidden = true, by.mass = 0;
        return;
    }
    if (o.type == 7 && (o.args[3] == 1 || o.args[3] == 2)) {
        // f04_04c9: a bullseye shown switches over (its method 3; the power
        // it works isn't ported yet).
        if (!o.hiddenSwitch) switchSet(o, o.state ? 0 : 1);
    }
}

void Science::fieldAt(const int p[3], const Ball* self, int16_t out[3]) {
    // f26_02e2: each source of the field but `self` (the room's list,
    // +F98, in the order made), its +20 at the point, summed.
    out[0] = out[1] = out[2] = 0;
    auto round = [&](const Ball& s) {
        // f05_0a1b: none unless the source's type is magnetic (its record's
        // +11: Iron) and the point within 16 of its radii; else its
        // strength k (+6 / +A) less k times the distance / 256, away from
        // it (towards it when k is below 0).
        if (s.kind != 3) return;
        const int16_t r2 = static_cast<int16_t>(s.r * s.r);
        int16_t d[3] = {static_cast<int16_t>(p[0] - s.cx), static_cast<int16_t>(p[1] - s.cy), static_cast<int16_t>(p[2] - s.cz)};
        const int32_t dist = static_cast<int32_t>(d[0]) * d[0] + static_cast<int32_t>(d[1]) * d[1] + static_cast<int32_t>(d[2]) * d[2];
        if (static_cast<int32_t>(r2) * 256 < dist) return;
        const int16_t k = static_cast<int16_t>(s.strengthNum / s.strengthDen);
        const int32_t len = libLength(d);
        const int16_t m = static_cast<int16_t>(k - static_cast<int16_t>((static_cast<int32_t>(k) * len) >> 8));
        libScaleTo(d, m);
        for (int j = 0; j < 3; ++j) out[j] = static_cast<int16_t>(out[j] + d[j]);
    };
    for (Object& o : table_.objects) {
        if (o.type == 3 && hasBall_ && ball_.magnetic && &ball_ != self) round(ball_);
        else if ((o.type == 4 || o.type == 5) && &o.body != self) round(o.body);
        else if ((o.type == 15 || (o.type == 6 && o.powered)) && &o.body != self) {
            // f05_252f (type 6 f05_2933: only while powered): as round,
            // without the type's test, its pole turned on one side of it
            // (the left wall's for x past its centre, the back wall's for y).
            const Ball& s = o.body;
            const int16_t r2 = static_cast<int16_t>(s.r * s.r);
            int16_t d[3] = {static_cast<int16_t>(p[0] - s.cx), static_cast<int16_t>(p[1] - s.cy), static_cast<int16_t>(p[2] - s.cz)};
            const int32_t dist = static_cast<int32_t>(d[0]) * d[0] + static_cast<int32_t>(d[1]) * d[1] + static_cast<int32_t>(d[2]) * d[2];
            if (static_cast<int32_t>(r2) * 256 < dist) continue;
            int16_t k = static_cast<int16_t>(s.strengthNum / s.strengthDen);
            if ((o.wall != 0 && d[1] > 0) || (o.wall == 0 && d[0] > 0)) k = static_cast<int16_t>(-k);
            const int32_t len = libLength(d);
            const int16_t m = static_cast<int16_t>(k - static_cast<int16_t>((static_cast<int32_t>(k) * len) >> 8));
            libScaleTo(d, m);
            for (int j = 0; j < 3; ++j) out[j] = static_cast<int16_t>(out[j] + d[j]);
        }
    }
}

}  // namespace edison
