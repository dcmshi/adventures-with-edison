// Wild Science Arcade: the room's other objects (segments 2-5 and 7, made
// by f61_011d / f61_09bd), as far as they're ported. See docs/SCIENCE.md.

#include <algorithm>
#include <cstdlib>

#include "science/science.h"

namespace edison {

void Science::thingsBuilt() {
    // Each object's own setup as the builder makes it (f61_011d, f61_09bd).
    int power = -1;  // the room's +F94: the last type 6, 12 or 13 made
    // (Testing: Borland's rand() seed (DS:8454) as the room is built, as
    // read from the original; a second number, the seed once it's built.)
    if (const char* s = std::getenv("SCI_RANDSEED")) randSeed_ = static_cast<uint32_t>(std::strtoul(s, nullptr, 0));
    int lastHole = -1;  // the room's +F96: the last hole the file made
    for (size_t i = 0; i < table_.objects.size(); ++i) {
        Object& o = table_.objects[i];
        if (o.type == 8) {
            // A door's second half (e, f61_011d → f28_0391; without a hole
            // before it, "improper door construction"): its +C 0, its +0E
            // the hole before it.
            if (o.args[4] != 0) o.partner = lastHole, o.args[0] = 0;
            lastHole = static_cast<int>(i);
        }
        if (o.type == 14) {
            // The hot field (f02_1234): its sphere (+2) at (x, y) on the
            // ground there, radius a (+8); a 6 cube of a core at (x, y) on
            // the ground, its mass 0 (out of the step's reach), its draw
            // only a debugging outline (f13_0000, [12E6]). Its first spot
            // (f02_110b): at its centre, its full radius a / 2 + rand() * (a
            // - a / 2) / 8000h, growing from 0. (Its colour cycle, 4D-4F
            // every 10 ticks, f32_0e7f, only on a 256-colour display.)
            const int a = o.args[0];
            const int half = static_cast<uint16_t>(a + a) >> 2;
            Object::HotSpot s;
            s.x = o.x, s.y = o.y, s.z = heightUnder(o.x, o.y);
            s.full = static_cast<int>(static_cast<int32_t>(borlandRand()) * (a - half) / 0x8000) + half;
            o.hotSpots.assign(1, s);
            o.hotGrowing = true;
            continue;
        }
        if (o.type == 6 || o.type == 12 || o.type == 13) {
            // Its power part (f04_0000: off).
            o.powered = false;
            power = static_cast<int>(i);
        }
        if (o.type == 13) {
            // The fan (f02_05f2): a its way (+14: 0-3), its blades still.
            o.fanFrame = -1;
            continue;
        }
        if (o.type == 12) {
            // The electromagnet (f02_00c2): a its period (+1A); its box
            // (f02_0000) 44 wide, 2 deep, 52 high at (x, y, the ground), its
            // head up (+16 0, the way down: +1C 1), no ball caught (+7C).
            o.emDrop = 0, o.emMax = 52 - 12, o.emWay = 1, o.emFrame = -1, o.emCaught = false;
            continue;
        }
        if (o.type == 16) {
            // A block (f07_17f2): a its size (a byte), b its sprite, c its
            // drag (+E: the generic f08_07c6 when set), d its step (+10:
            // the generic body step f08_1a42 when set), e its mass (+34 / +38,
            // when above 0; else 1). Its box a cube of side 2a, its corner at
            // (x, y) on the ground (f08_0384); its sphere at the box's centre
            // a unit lower, radius a (f07_0ed0); its type the core's own
            // (Rubber); not magnetic.
            const int a = static_cast<int8_t>(o.args[0]);
            Ball& b = o.body;
            b = Ball{};
            b.r = a;
            b.cx = o.x + a, b.cy = o.y + a, b.cz = heightUnder(o.x, o.y) + a - 1;
            b.kind = 2, b.mass = o.args[4] > 0 ? o.args[4] : 1, b.self = static_cast<int>(i);
            continue;
        }
        if (o.type == 2) {
            // A magnetic ball (f61_09bd → f05_11c1; no room has one): type
            // 0's ball (f07_0000, a its radius, resting on the ground at (x,
            // y), no shadow, its tick and draw) with a magnetic part (+2,
            // f05_0003): strength 200 * [27B2] / ([27B0] * 2), as the type-3
            // ball's; its type Iron (f07_05bf with 28DE: mass 20, Iron's
            // frames), so the field pulls it (its +30, f05_0c0b). It takes
            // no mouse (f05_08f3).
            Ball& b = o.body;
            b = Ball{};
            b.r = o.args[0];
            b.cx = o.x, b.cy = o.y, b.cz = heightUnder(o.x, o.y) + b.r;
            b.kind = 3, b.mass = ballKinds_[3].mass, b.self = static_cast<int>(i);
            b.rollThreshold = (b.r * b.r) >> 4;
            b.magnetic = true;
            b.strengthNum = 200L * kTimerK, b.strengthDen = kTimerRate * 2L;
            continue;
        }
        if (o.type == 0) {
            // Another ball (f07_0000, the balls' own class): a its radius,
            // resting on the ground at (x, y) (f10_1582); its type the
            // core's own (+52 28B8: Rubber), so Rubber's frames (+20 1300,
            // breaking +22 1390); its mass 10 (+34 / +38); no shadow object
            // (+16 0: it draws none); its roll a frame every r * r / 16 of
            // squared move.
            Ball& b = o.body;
            b = Ball{};
            b.r = o.args[0];
            b.cx = o.x, b.cy = o.y, b.cz = heightUnder(o.x, o.y) + b.r;
            b.kind = 2, b.mass = 10, b.self = static_cast<int>(i);
            b.rollThreshold = (b.r * b.r) >> 4;  // ([DFA] / [DFE] is 9 / 9, set at start-up)
            continue;
        }
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
        // e kind 2's bits, f its +2. Kinds 0-2 work the power (+10: +F94 as
        // it is now; table.cpp makes none without one); d 3 (f04_05b8,
        // RETRY) needs none.
        o.state = 0, o.ticks = 0, o.hiddenSwitch = false;
        o.sprites = o.args[3] == 3 ? 0x15B2 : o.args[3] == 1 || o.args[3] == 2 ? 0x15BA : 0x15AA;
        o.power = o.args[3] == 3 ? -1 : power;
        o.blinkBits = o.args[3] == 2 ? o.args[4] : 0, o.blinkAt = -1;
        if (o.args[3] == 3) {
            // f04_05b8: the game's score (f06_0000: [BD8]) and the player's
            // balls (+BA, +BC) as they are now.
            o.savedScore = totalScore_;
            o.savedBalls[0] = leftBalls_, o.savedBalls[1] = rightBalls_;
        }
        if (o.args[2] != 0) switchTurn(o, 1);
    }
}

bool Science::thingBox(const Object& o, int box[6]) const {
    // An object's box (its core's +6E: x, y, z, w, d, h).
    if (o.type == 15 || o.type == 6) {
        for (int k = 0; k < 6; ++k) box[k] = o.coreBox[k];
        return true;
    }
    if (o.type == 4 || o.type == 5 || o.type == 16) {
        // A magnet's (and a block's) follows its sphere (f07_04d5).
        const Ball& b = o.body;
        box[3] = box[4] = box[5] = 2 * b.r;
        box[0] = b.cx - b.r, box[1] = b.cy - b.r, box[2] = b.cz - b.r + 1;
        return true;
    }
    if (o.type == 13) {
        // f02_0530: a 26 cube at (x, y, the ground under it).
        box[0] = o.x, box[1] = o.y, box[2] = heightUnder(o.x, o.y);
        box[3] = box[4] = box[5] = 26;
        return true;
    }
    if (o.type == 12) {
        // f02_0000: at (x, y, the ground under it), 44 x 2 x 52.
        box[0] = o.x, box[1] = o.y, box[2] = heightUnder(o.x, o.y);
        box[3] = 44, box[4] = 2, box[5] = 52;
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

void Science::thingDraw(Object& o, const Rect& r) {
    // Its core's +04, r its rectangle (+0): most a sprite at its centre.
    const int cx = r.x + (r.w >> 1), cy = r.y + (r.h >> 1);
    if (o.type == 13) {
        // f13_0f2b: the fan by its way (+1D: DS:16DC + 2a) at the centre;
        // turning (+1B), its wind (+19: DS:16E4 + 8a, by the frame) with
        // its corner at the rectangle's plus +1F, +21 (f14_1179).
        const int way = std::clamp(o.args[0], 0, 3);
        static const int kWind[4][2] = {{18, -17}, {-47, 11}, {22, 0}, {-60, 0}};
        const size_t body = 0x16DC + 2u * static_cast<size_t>(way);
        objectSprite(cx, cy, static_cast<uint16_t>(data_[body] | data_[body + 1] << 8));
        if (o.fanFrame >= 0) {
            const size_t at = 0x16E4 + 8u * static_cast<size_t>(way) + 2u * static_cast<size_t>(o.fanFrame);
            panelSprite(r.x + kWind[way][0], r.y + kWind[way][1], static_cast<uint16_t>(data_[at] | data_[at + 1] << 8));
        }
        return;
    }
    if (o.type == 16) {
        // f13_0589: its sprite (b) with its corner at the rectangle's (f14_1179).
        panelSprite(r.x, r.y, static_cast<uint16_t>(o.args[1]));
        return;
    }
    if (o.type == 12) {
        // f13_0dc1: caught, 115F + its frame (+1E, from 0) at the centre;
        // else the head (13D4, f14_1179) at the rectangle's corner lowered
        // by +16, the frame (13D3) over it at the centre.
        if (o.emCaught) {
            if (o.emFrame < 0) o.emFrame = 0;
            objectSprite(cx, cy, static_cast<uint16_t>(0x115F + o.emFrame));
        } else {
            panelSprite(r.x, r.y + o.emDrop, 0x13D4);
            objectSprite(cx, cy, 0x13D3);
        }
        return;
    }
    objectSprite(cx, cy, thingSprite(o));
}

void Science::switchSet(Object& o, int on) {
    // f04_008e: its state (+4), and its area redrawn (its core's +10).
    o.state = on;
    viewDirty_ = true;
}

void Science::switchTurn(Object& o, int on) {
    // f04_0321 (kinds 0-2; no room starts a RETRY on): the switch set
    // (f04_008e), then its power's +0C with on or off. Sound 6026 when
    // switched off, and on unless the power's +10 says '\r' (type 6's,
    // f05_29f2; 12's and 13's say 2). Type 6's +0C (f05_29c1) sounds 6027
    // when switched on; then its power part set (+26: its field, f05_2933,
    // and its picture) and redrawn.
    switchSet(o, on);
    if (std::getenv("SCI_DEBUG")) logLine("switch " + std::to_string(&o - table_.objects.data()) + (on ? " on" : " off") + " at t" + std::to_string(timerTicks_));
    if (o.args[3] == 3 || o.power < 0) return;
    Object& p = table_.objects[static_cast<size_t>(o.power)];
    if (on && p.type != 6) sound(0x6026);
    if (on && p.type == 6) sound(0x6027);
    p.powered = on != 0;
    viewDirty_ = true;
    if (!on) sound(0x6026);
}

void Science::hotMark(const Object::HotSpot& s) {
    // f13_16f9: from radius 2, 140D at the spot's centre projected, at the
    // radius (6 at least) over 26, on screen 3 and on the display.
    // f14_0d69: only where the sprite at 1:1 lies wholly on the screen
    // ([1706]); at 1:1 when the scale's whole part is 1 (radii 26-51), else
    // f72_02cd and f73_0324: (w * scale) >> 8 wide about the point, each
    // pixel the source's at a 16.16 step of 256 / scale.
    if (s.r < 2) return;
    const Bitmap& bmp = ctx_.bitmap(0x140D);
    const auto [px, py] = project(s.x, s.y, s.z);
    const int x0 = px - (bmp.width >> 1), y0 = py - (bmp.height >> 1);
    const Rect screen{0, 0, Screen::kWidth, Screen::kHeight};
    if (!inside(screen, x0, y0) || !inside(screen, x0 + bmp.width, y0 + bmp.height)) return;
    const int e = std::max(s.r, 6);
    const int was = current();
    for (int screenNo : {3, 1}) {
        select(screenNo);
        if (e / 0x1A == 1) {
            objectSprite(px, py, 0x140D);
            continue;
        }
        const uint32_t scale = static_cast<uint32_t>(e) * 0x100 / 0x1A;
        const int w = static_cast<int>((bmp.width * scale) >> 8), h = static_cast<int>((bmp.height * scale) >> 8);
        const uint32_t step = (0x100 / scale) << 16 | ((0x100 % scale) << 16) / scale;
        const int left = px - (w >> 1), top = py - (h >> 1);
        Screen& scr = ctx_.screens[screenNo];
        for (int r = 0; r < h; ++r)
            for (int c = 0; c < w; ++c) {
                const uint8_t p = bmp.at(static_cast<int>(c * step >> 16), static_cast<int>(r * step >> 16));
                const int x = left + c, y = top + r;
                if (p && x >= 0 && y >= 0 && x < Screen::kWidth && y < Screen::kHeight)
                    scr.pixels[static_cast<size_t>(y) * Screen::kWidth + x] = p;
            }
    }
    select(was);
    viewDirty_ = true;
}

Science::Object::HotSpot Science::hotSpawn(const Object::HotSpot& from, const int field[4]) {
    // f02_0e4a: a point a half to a whole of the spot's radius off its
    // centre each way (rand() for x, y, then their signs), at the field's
    // height; inside the field (f11_1732 with radius 1), a spot there of
    // three quarters to all of what's left of the field's radius
    // (f11_0000); else one of radius 0, which never grows.
    Object::HotSpot n;
    const int half = from.r / 2;
    int ox = static_cast<int>(static_cast<int32_t>(borlandRand()) * half / 0x8000);
    int oy = static_cast<int>(static_cast<int32_t>(borlandRand()) * half / 0x8000);
    ox += half, oy += half;
    if (static_cast<int32_t>(borlandRand()) * 2 / 0x8000 != 0) ox = -ox;
    if (static_cast<int32_t>(borlandRand()) * 2 / 0x8000 != 0) oy = -oy;
    const int x = from.x + ox, y = from.y + oy, z = field[2];
    int16_t d[3] = {static_cast<int16_t>(x - field[0]), static_cast<int16_t>(y - field[1]), static_cast<int16_t>(z - field[2])};
    const int16_t reach = static_cast<int16_t>(field[3] + 1);
    const int32_t d2 = static_cast<int32_t>(d[0]) * d[0] + static_cast<int32_t>(d[1]) * d[1] + static_cast<int32_t>(d[2]) * d[2];
    if (d2 > static_cast<int32_t>(reach) * reach) return n;
    const int left = static_cast<int16_t>(field[3] - static_cast<int16_t>(libLength(d)));
    if (left <= 0) return n;
    const int lo = left * 3 / 4;
    n.full = static_cast<int>(static_cast<int32_t>(borlandRand()) * (left - lo) / 0x8000) + lo;
    n.x = x, n.y = y, n.z = z;
    return n;
}

void Science::hotGrow(Object& o) {
    // f02_1481: each spot (the list in order, new ones too) not yet full a
    // unit bigger and marked (f13_16f9); then, with at most 4 spots and
    // its radius above 5, at rand() * r / 8000h above r * 16 / 20 a new
    // one off it (f02_0e4a). All full, the field stops growing (+16).
    const int field[4] = {o.x, o.y, heightUnder(o.x, o.y), o.args[0]};
    size_t full = 0;
    for (size_t i = 0; i < o.hotSpots.size(); ++i) {
        if (static_cast<uint16_t>(o.hotSpots[i].full) <= static_cast<uint16_t>(o.hotSpots[i].r)) {
            ++full;
            continue;
        }
        ++o.hotSpots[i].r;
        const Object::HotSpot s = o.hotSpots[i];  // (a copy: the list may grow)
        hotMark(s);
        if (o.hotSpots.size() > 4 || s.r <= 5) continue;
        const int roll = static_cast<int>(static_cast<int32_t>(borlandRand()) * s.r / 0x8000);
        if (roll <= static_cast<int16_t>(s.r << 4) / 20) continue;
        o.hotSpots.push_back(hotSpawn(s, field));
        if (std::getenv("SCI_DEBUG")) {
            const Object::HotSpot& n = o.hotSpots.back();
            logLine("hot spot " + std::to_string(o.hotSpots.size() - 1) + " at " + std::to_string(n.x) + "," + std::to_string(n.y) +
                    " r " + std::to_string(n.full) + " at t" + std::to_string(timerTicks_));
        }
    }
    if (full == o.hotSpots.size()) o.hotGrowing = false;
}

void Science::thingTick(Object& o) {
    if (o.type == 14) {
        // The hot field's step (f02_1627): growing, every 6 room ticks
        // ([FFE]) its spots grow (f02_1481). Then the player's ball on the
        // ground (+5E) and not breaking (+7C): its foot (its centre less its
        // radius in z) within the field (radius 1 each, f11_1732) and
        // within a spot's radius so far, it's heated with 2000 (f07_030e):
        // broken, whatever its type (sound 6028; Ice melting).
        if (o.hotGrowing && roomTicks_ % 6 == 0) hotGrow(o);
        if (!hasBall_ || !ball_.onGround || ball_.state != 0) return;
        const int foot[3] = {ball_.cx, ball_.cy, ball_.cz - ball_.r};
        auto meets = [&](int x, int y, int z, int r) {
            const int16_t dx = static_cast<int16_t>(x - foot[0]), dy = static_cast<int16_t>(y - foot[1]), dz = static_cast<int16_t>(z - foot[2]);
            const int16_t reach = static_cast<int16_t>(r + 1);
            return static_cast<int32_t>(dx) * dx + static_cast<int32_t>(dy) * dy + static_cast<int32_t>(dz) * dz <= static_cast<int32_t>(reach) * reach;
        };
        if (!meets(o.x, o.y, heightUnder(o.x, o.y), o.args[0])) return;
        for (const Object::HotSpot& s : o.hotSpots) {
            if (!meets(s.x, s.y, s.z, s.r)) continue;
            ball_.state = 1, ball_.heated = true;
            if (std::getenv("SCI_DEBUG")) logLine("hot field burns the ball at t" + std::to_string(timerTicks_));
            return;
        }
        return;
    }
    if (o.type == 4) {
        // A loose magnet's step (its core's +00: f05_1f76 → f08_1a42).
        ballStep(o.body);
        return;
    }
    if (o.type == 16) {
        // A block's (f07_1920): the body's step, if d (+10) is set.
        if (o.args[3] != 0) ballStep(o.body);
        return;
    }
    if (o.type == 13) {
        // The fan's step (f02_08e9), while powered (+10) or its blades still
        // turn: d the ball's centre from its sphere's (radius r). Every 10
        // room ticks ([FFE]): powered and d within 8r, the next frame (0-3)
        // and sound 601B; else, turning, the next till still (-1). Each
        // tick, d within 4r, |dz| at most r, and d's heading (f87_0804) in
        // its quarter (+15 to +17: from 2000h, A000h, E000h, 6000h by its
        // way; way 2's across 0), the ball heated (f07_030e with 200).
        if (!o.powered && o.fanFrame < 0) return;
        if (!hasBall_) return;
        Contact c;
        contactOf(o, c);
        const int r = c.s[3];
        const int dx = static_cast<int16_t>(ball_.cx - c.s[0]), dy = static_cast<int16_t>(ball_.cy - c.s[1]),
                  dz = static_cast<int16_t>(ball_.cz - c.s[2]);
        const int32_t r2 = r * r;
        const int32_t d2 = static_cast<int32_t>(dx) * dx + static_cast<int32_t>(dy) * dy + static_cast<int32_t>(dz) * dz;
        if (roomTicks_ % 10 == 0) {
            if (d2 > r2 * 64 || !o.powered) {
                if (o.fanFrame >= 0) {
                    if (++o.fanFrame > 3) o.fanFrame = -1;
                    viewDirty_ = true;
                }
            } else {
                if (++o.fanFrame > 3) o.fanFrame = 0;
                viewDirty_ = true;
                sound(0x601B);
            }
        }
        if (d2 > r2 * 16 || std::abs(dz) > r) return;
        static const uint16_t kFrom[4] = {0x2000, 0xA000, 0xE000, 0x6000};
        const uint16_t from = kFrom[std::clamp(o.args[0], 0, 3)], to = static_cast<uint16_t>(from + 0x4000);
        const uint16_t heading = dx == 0 && dy == 0 ? 0 : static_cast<uint16_t>(libAtan2(dx, dy));
        const bool in = o.args[0] == 2 ? !(heading < from && heading > to) : !(heading < from || heading > to);
        if (in && ballKinds_[ball_.kind].heat <= 200 && ball_.state == 0) ball_.state = 1, ball_.heated = true;
        return;
    }
    if (o.type == 12) {
        // The electromagnet's step (f02_03fb), while powered (+10): every a
        // room ticks ([FFE]) the way turned; every 6, caught, the next
        // frame (to 3), else the head 2 lower or higher, within 0 and below
        // +18 (redrawn unless it stopped there).
        if (!o.powered) return;
        const int period = static_cast<uint16_t>(o.args[0]);
        if (period != 0 && roomTicks_ % period == 0) o.emWay = o.emWay == 0;
        if (roomTicks_ % 6 != 0) return;
        if (o.emCaught) {
            if (o.emFrame < 3) ++o.emFrame, viewDirty_ = true;
            return;
        }
        o.emDrop += o.emWay ? 2 : -2;
        if (o.emDrop < 0) o.emDrop = 0;
        else if (o.emDrop >= o.emMax) o.emDrop = o.emMax - 1;
        else viewDirty_ = true;
        return;
    }
    if (o.type == 7 && o.args[3] == 2) {
        // Kind 2's step (f04_08e1): every 30 room ticks ([FFE]) the next of
        // e's 16 bits; set, it's shown (+1C, f08_04b3), else hidden (+18,
        // f08_0469: its rectangle empty, so not drawn; it stays solid, but
        // met it doesn't switch, f04_04c9).
        if (roomTicks_ % 30 != 0) return;
        if (++o.blinkAt > 15) o.blinkAt = 0;
        const bool shown = (o.blinkBits >> o.blinkAt & 1) != 0;
        if (shown == o.hiddenSwitch) o.hiddenSwitch = !shown, viewDirty_ = true;
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
    // f04_04bf: kinds 1 and 2 take no clicks.
    if (o.args[3] == 1 || o.args[3] == 2) return false;
    // f04_03ab: a press switches it over (its method 3), and is taken.
    if (!m.click) return false;
    const int on = o.state ? 0 : 1;
    if (o.args[3] == 0) switchTurn(o, on);
    else retry(o, on);
    return true;
}

void Science::retry(Object& o, int on) {
    // RETRY's method 3 (f04_070e): only while neither column waits for its
    // PUSH (+132): the switch, the area redrawn; the player's balls and
    // the game's score as they were (f06_028a: the room's box too), no
    // completion bonus (+F7B), and event 9 to this room again.
    if (!columns_[0].ballOut || !columns_[1].ballOut) return;
    switchSet(o, on);
    leftBalls_ = o.savedBalls[0], rightBalls_ = o.savedBalls[1];
    totalScore_ = o.savedScore, score_ = o.savedScore;
    completionBonus_ = 0;
    exitRoom_ = currentRoom_;
}

bool Science::contactOf(Object& o, Contact& c) {
    // An object as a body's step meets it (f08_1a42): its sphere (+4C), its
    // mass ratio (+34 / +38; 0: out of reach), its body if it has one.
    if (isHole(o)) {
        // A hole: soft; while the ball leaves it, out of reach (f28_0671).
        holeSphere(o, c.s);
        c.ratio = o.leaving ? 0 : 1;
        return true;
    }
    if (o.type == 10 || o.type == 11) {
        // A point target (a suckhole and type 11 too: its target part's, f02_0dfb): its
        // box's centre a unit lower, radius its kind's size (f03_002c's f07_1082); soft
        // while live. (A suckhole's spark: soft and no points, so nothing.)
        const int g = heightUnder(o.x, o.y);
        c.s[0] = o.x + o.size, c.s[1] = o.y + o.size, c.s[2] = g + o.size - 1, c.s[3] = o.size;
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
    if (o.type == 16) {
        // A block: its sphere and mass (e); its velocity takes the change
        // (its core's +2C, f08_1371).
        const Ball& b = o.body;
        c.s[0] = b.cx, c.s[1] = b.cy, c.s[2] = b.cz, c.s[3] = b.r;
        c.ratio = b.mass;
        c.body = &o.body, c.movable = true;
        return true;
    }
    if (o.type == 0 || o.type == 2) {
        // Another ball: its sphere and mass (10; type 2's 20); out of reach once gone.
        const Ball& b = o.body;
        c.s[0] = b.cx, c.s[1] = b.cy, c.s[2] = b.cz, c.s[3] = b.r;
        c.ratio = b.hidden ? 0 : b.mass;
        c.body = &o.body, c.movable = true;
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
    if (o.type == 13) {
        // The fan: solid (+34 / +38 25); its sphere (f07_0ed0) at its box's
        // centre a unit lower, radius 13. It doesn't move.
        c.s[0] = o.x + 13, c.s[1] = o.y + 13, c.s[2] = heightUnder(o.x, o.y) + 12, c.s[3] = 13;
        c.ratio = 25;
        return true;
    }
    if (o.type == 12) {
        // The electromagnet: soft (+34 / +38 1); its sphere (f02_00c2's
        // end, f10_1582) radius 13 at its box's centre in x and y, resting
        // on the ground there.
        c.s[0] = o.x + 22, c.s[1] = o.y + 1, c.s[2] = heightUnder(o.x + 22, o.y + 1) + 13, c.s[3] = 13;
        c.ratio = 1;
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
    if (isHole(o)) {
        // f28_13fe: an idle hole takes the player's ball (f28_14a5, unless
        // shut, +2F): stopped (f07_0ead → f08_0721), hidden with its shadow
        // (f07_03be), the room busy (+F6F). (Its push, +4C, stays: only a
        // pulling hole's tick sets or clears it, f28_18fd.)
        if (!player || o.swallow || o.leaving || o.spit || o.closed) return;
        Ball& b = ball_;
        for (int k = 0; k < 3; ++k) b.v[k] = 0, b.kick[k] = 0;
        b.kickTicks = 0;
        b.hidden = true, shadowShown_ = false;
        o.swallow = 1, o.spit = 0, o.swallowDone = false;
        if (std::getenv("SCI_DEBUG")) logLine("hole " + std::to_string(o.args[0]) + " takes the ball at t" + std::to_string(timerTicks_));
        roomBusy_ = true;
        viewDirty_ = true;
        return;
    }
    if (o.type == 11) {
        if (player) creatureMet(o);
        return;
    }
    if (o.type == 16) {
        // A block: the generic +34 (f08_122b), nothing. (Testing: counted.)
        if (player) ++o.ticks;
        return;
    }
    if (o.type == 10) {
        // f03_0593: only the player's ball scores (kind 3 hides another
        // body, its mass 0).
        if (player) pointHit(o);
        else if (o.kind == 3) by.hidden = true, by.mass = 0;
        return;
    }
    if (o.type == 12) {
        // f02_02a0: the player's ball (+2A 1), none caught yet, and the head
        // down to its top (the head's bottom, its sphere's bottom (the
        // ground) plus +18 less +16, at most the ball's top + 1, unsigned): Iron
        // (its record's first byte 3) caught (+7C, redrawn, frame -1); any
        // other breaks (+7C 1), Rubber (2) zapped (its motion part's +2A).
        if (!player || o.emCaught) return;
        const uint16_t head = static_cast<uint16_t>(heightUnder(o.x + 22, o.y + 1) + o.emMax - o.emDrop);
        if (head > static_cast<uint16_t>(2 * by.r + (by.cz - by.r) + 1)) return;
        if (by.kind == 3) {
            o.emCaught = true, o.emFrame = -1;
            viewDirty_ = true;
        } else {
            by.state = 1;
            if (by.kind == 2) by.zapped = true;
        }
        return;
    }
    if (o.type == 7 && (o.args[3] == 1 || o.args[3] == 2)) {
        // f04_04c9: a bullseye shown switches over (its method 3).
        if (!o.hiddenSwitch) switchTurn(o, o.state ? 0 : 1);
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
        else if ((o.type == 4 || o.type == 5 || o.type == 2) && &o.body != self) round(o.body);
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
