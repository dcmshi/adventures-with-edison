// Wild Science Arcade: the point targets (object type 10, segment 3) and
// the score. See docs/SCIENCE.md.

#include <algorithm>
#include <cstdlib>

#include "artech/context.h"
#include "science/science.h"

namespace edison {

namespace {

constexpr int kTargetPeriod = 11;  // +1C: ticks a frame
constexpr uint16_t kKinds = 0x15CC;  // 24 bytes a kind: its idle frames' count, then sprites
constexpr uint16_t kHitAnims = 0x2CC;  // 4 bytes a kind: the hit's first frame, its length

struct RoomConfig {
    int room;
    bool shotBonus;
    long completion;
    uint8_t targetBonus[6], completionShare[6];
};

// From each room's builder (segments 41-60): [30C], +F7B, +F8D (FUN_10d0_088b,
// a value from a shot count on), +F87 (FUN_10d0_0859); the others keep the
// base's (f27_03da: off, 1500, all 128). (tools/testing/roompics.py --config)
const RoomConfig kRoomConfigs[] = {
    {1, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {2, false, 2000, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 64, 32, 0}},
    {3, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {4, false, 2000, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 64, 32, 32}},
    {5, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 96, 64, 32, 32}},
    {6, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {7, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {9, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {10, false, 3000, {128, 128, 128, 128, 128, 128}, {128, 128, 85, 43, 6, 6}},
    {12, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {13, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {14, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {15, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {16, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {17, false, 2000, {128, 128, 128, 128, 128, 128}, {128, 128, 64, 0, 0, 0}},
    {18, false, 4000, {128, 128, 128, 128, 128, 128}, {128, 128, 64, 32, 16, 16}},
    {20, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {21, true, 2000, {128, 128, 64, 28, 12, 12}, {128, 128, 64, 6, 3, 3}},
    {22, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 64, 32, 32, 32}},
    {23, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {26, false, 1000, {128, 128, 128, 128, 128, 128}, {128, 128, 64, 32, 16, 16}},
    {28, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 85, 43, 21, 21}},
    {29, false, 2000, {128, 128, 128, 128, 128, 128}, {128, 128, 32, 0, 0, 0}},
    {30, false, 2000, {128, 128, 128, 128, 128, 128}, {128, 128, 96, 64, 32, 32}},
    {31, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {32, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {33, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {34, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 64, 32, 16, 16}},
    {35, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 85, 4}},
    {36, false, 2000, {128, 128, 128, 128, 128, 128}, {128, 128, 0, 0, 0, 0}},
    {37, false, 1000, {128, 128, 128, 128, 128, 128}, {128, 128, 64, 0, 0, 0}},
    {38, false, 1000, {128, 128, 128, 128, 128, 128}, {128, 128, 64, 0, 0, 0}},
    {39, false, 2000, {128, 128, 128, 128, 128, 128}, {128, 128, 64, 32, 16, 16}},
    {40, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 64, 32, 32, 32}},
    {41, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 85, 64, 0, 0}},
    {42, false, 1000, {128, 128, 128, 128, 128, 128}, {128, 128, 64, 13, 0, 0}},
    {43, false, 3000, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 85, 4}},
    {44, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {45, false, 1000, {128, 128, 128, 128, 128, 128}, {128, 128, 64, 0, 0, 0}},
    {46, false, 1000, {128, 128, 128, 128, 128, 128}, {128, 128, 76, 38, 0, 0}},
    {47, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {48, false, 2000, {128, 128, 128, 128, 128, 128}, {128, 128, 64, 32, 12, 12}},
    {49, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 64, 32, 0, 0}},
    {50, false, 0, {128, 128, 128, 128, 128, 128}, {128, 85, 85, 85, 85, 85}},
    {51, false, 3000, {128, 128, 128, 128, 128, 128}, {128, 128, 85, 43, 3, 3}},
    {52, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {53, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {54, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {55, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 96, 96, 32}},
    {56, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {57, false, 3000, {128, 128, 128, 128, 128, 128}, {128, 128, 115, 106, 85, 43}},
    {58, false, 1500, {128, 128, 128, 128, 128, 128}, {128, 128, 43, 0, 0, 0}},
    {59, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 25, 12, 0, 0}},
    {60, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {64, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {65, false, 3000, {128, 128, 128, 128, 128, 128}, {128, 128, 85, 43, 0, 0}},
    {66, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 64, 32}},
    {67, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 64, 0, 0, 0}},
    {69, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {70, false, 5000, {128, 128, 128, 128, 128, 128}, {128, 128, 51, 26, 13, 13}},
    {71, false, 2000, {128, 128, 128, 128, 128, 128}, {128, 128, 32, 7, 7, 7}},
    {72, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 85, 85, 43, 21}},
    {73, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {74, false, 2000, {128, 128, 128, 128, 128, 128}, {128, 128, 0, 0, 0, 0}},
    {75, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 64, 0, 0}},
    {76, false, 2000, {128, 128, 128, 128, 128, 128}, {128, 128, 0, 0, 0, 0}},
    {77, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 64, 0}},
    {91, false, 1500, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 0, 0, 0}},
    {92, false, 2000, {128, 128, 128, 128, 128, 128}, {128, 128, 96, 64, 32, 32}},
    {93, false, 1500, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 9, 9, 9}},
    {94, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 64, 32}},
    {95, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 64, 0, 0}},
    {96, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
    {97, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 0, 0, 0, 0}},
    {98, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 85, 43, 21, 21}},
    {99, false, 0, {128, 128, 128, 128, 128, 128}, {128, 128, 128, 128, 128, 128}},
};

}  // namespace

void Science::roomConfig(int room) {
    shotBonus_ = false, completionBonus_ = 1500;
    std::fill(std::begin(targetBonus_), std::end(targetBonus_), uint8_t{128});
    std::fill(std::begin(completionShare_), std::end(completionShare_), uint8_t{128});
    for (const RoomConfig& c : kRoomConfigs)
        if (c.room == room) {
            shotBonus_ = c.shotBonus, completionBonus_ = c.completion;
            std::copy(std::begin(c.targetBonus), std::end(c.targetBonus), targetBonus_);
            std::copy(std::begin(c.completionShare), std::end(c.completionShare), completionShare_);
        }
}

void Science::addScore(long points) {
    // f06_0208: the game's score ([BD8], not below 0) more, sound 602B, shown
    // in the room's box (+F35).
    // While [171C] is set (after a lesson, till a room's end clears it)
    // neither the sound nor the box.
    totalScore_ = std::max(0L, totalScore_ + points);
    if (quietScore_) return;
    sound(0x602B);
    score_ = totalScore_;
    viewDirty_ = true;
}

void Science::pointHit(Object& o) {
    // f03_0593. Kind 3 (the lips), whatever it's done before: hit, its box
    // 8 bigger each way; the player's ball, if Ice (its type's first byte
    // 0), caught (+10 0: put out of the way at 0, 0, f08_056e; stopped,
    // f07_0ead; hidden, f07_03be), else broken (+7C); its hit frames
    // (f03_01bf); sound 6019 (Glass) or 601A unless caught.
    const int first = static_cast<int16_t>(data_[kHitAnims + 4 * o.kind] | data_[kHitAnims + 4 * o.kind + 1] << 8);
    const int count = static_cast<int16_t>(data_[kHitAnims + 4 * o.kind + 2] | data_[kHitAnims + 4 * o.kind + 3] << 8);
    auto hitFrames = [&](int sequence, int frames) {
        if (o.sequence != sequence) viewDirty_ = true;
        o.sequence = sequence, o.frames = frames, o.frame = -1;
    };
    if (o.kind == 3) {
        o.hit = 1;
        o.grow += 8;
        bool caught = false;
        if (ball_.kind == 0) {
            o.grip = 0, caught = true;
            ballHold();
        } else {
            ball_.state = 1;
        }
        if (std::getenv("SCI_DEBUG")) logLine("lips at " + std::to_string(o.x) + "," + std::to_string(o.y) + (caught ? " catch" : " break") + " at t" + std::to_string(timerTicks_));
        hitFrames(kKinds + 0x18 * o.kind + 2 * first, count);
        if (!caught) sound(ball_.kind == 4 ? 0x6019 : 0x601A);
        viewDirty_ = true;
        return;
    }
    // The others: live, with points and not yet hit, by the player's ball:
    // hit; one target more down ([308]); no longer met (+34 0); its box 8
    // bigger each way; its kind's hit frames (from DS:2CC, the first and
    // how many); sound 601A.
    if (o.points == 0 || o.hit != 0) return;
    o.hit = 1;
    ++targetsHit_;
    if (std::getenv("SCI_DEBUG")) logLine("target at " + std::to_string(o.x) + "," + std::to_string(o.y) + " hit at t" + std::to_string(timerTicks_));
    o.live = false;
    o.grow = 8;
    hitFrames(kKinds + 0x18 * o.kind + 2 * first, count);
    sound(0x601A);
    viewDirty_ = true;
}

void Science::ballHold() {
    // The lips' hold (f03_0593, f03_0207): the ball put on the ground at 0,
    // 0 (f08_056e: sound 6026 if that's far), stopped (f07_0ead) and
    // hidden (f07_03be).
    Ball& b = ball_;
    if (b.cx * b.cx + b.cy * b.cy > 15 * 15) sound(0x6026);
    b.cx = 0, b.cy = 0, b.cz = heightUnder(0, 0) + b.r;
    for (int k = 0; k < 3; ++k) b.v[k] = 0, b.rem[k] = 0, b.disp[k] = 0, b.kick[k] = 0, b.push[k] = 0;
    b.kickTicks = 0;
    b.hidden = true, shadowShown_ = false;
    viewDirty_ = true;
}

void Science::pointTick(Object& o) {
    // f03_0207, every 11 ticks (by [FFE] and its +1E): idle, the next of its
    // kind's frames; hit, counting to its hit frames' length + 1, then its
    // points (with [30C] by the shots: +F8D in 128ths) to the score, in
    // hundreds, and shown (sprite 1236 + hundreds - 1); 5 counts on, gone.
    if ((o.id + roomTicks_) % kTargetPeriod != 0) return;
    if (o.scored == 0) {
        if (o.hit == 0) {
            if (++o.frame >= o.frames) o.frame = 0;
        } else {
            const int length = static_cast<int16_t>(data_[kHitAnims + 4 * o.kind + 2] | data_[kHitAnims + 4 * o.kind + 3] << 8);
            // Kind 3 holding the ball keeps it out of the way.
            if (o.grip == 0) ballHold();
            if (++o.hit > (o.grip == 1 ? 3 : length + 1)) {
                if (o.kind == 3) {
                    lipsTurn(o);
                } else {
                    if (shotBonus_) o.points = static_cast<int>(static_cast<long>(o.points) * targetBonus_[std::min(shots_, 5)] / 0x80);
                    addScore(o.points / 100 * 100);
                    o.scored = 1;
                }
            }
            if (++o.frame >= o.frames) o.frame = o.frames - 1;
        }
    } else {
        if (++o.scored >= kTargetPeriod / 2) {
            if (o.scored < kTargetPeriod / 2 + 2) o.shown = false, viewDirty_ = true;
            return;
        }
    }
    viewDirty_ = true;
}

void Science::lipsTurn(Object& o) {
    // f03_0207, kind 3 when its frames are done: holding the ball, it
    // spits (DS:15C4, four frames: +10 1); spitting, the ball comes out
    // 34 in front of its sphere's centre, on the ground (f08_056e), shown
    // (f07_03fb) and shot 20 further forward at the centre's height
    // (f27_27a3; +10 -1); then (and after a ball it broke) idle again:
    // soft, its box back, its idle frames.
    if (o.grip == 0) {
        if (o.sequence != 0x15C4) viewDirty_ = true;
        o.sequence = 0x15C4, o.frames = 4, o.frame = -1;
        o.hit = 1, o.grip = 1;
        return;
    }
    if (o.grip == 1) {
        // Its sphere (f03_002c's f07_1082): the box's centre a unit lower.
        const int cx = o.x + 13, cy = o.y + 13, cz = heightUnder(o.x, o.y) + 12;
        Ball& b = ball_;
        const int x = cx, y = cy - 0x22;
        const int dx = b.cx - x, dy = b.cy - y;
        if (dx * dx + dy * dy > 15 * 15) sound(0x6026);
        b.cx = x, b.cy = y, b.cz = heightUnder(x, y) + b.r;
        b.hidden = false;
        lastCentre_[0] = shadowSeen_[0] = b.cx, lastCentre_[1] = shadowSeen_[1] = b.cy, lastCentre_[2] = shadowSeen_[2] = b.cz;
        if (std::getenv("SCI_DEBUG")) logLine("lips throw from " + std::to_string(b.cx) + "," + std::to_string(b.cy) + "," + std::to_string(b.cz) + " at " + std::to_string(x) + "," + std::to_string(y - 0x14) + "," + std::to_string(cz));
        ballLaunch(x, y - 0x14, cz);
        o.grip = -1;
    }
    o.hit = 0, o.sequence = 0;
    o.frames = static_cast<int16_t>(data_[kKinds + 0x18 * o.kind] | data_[kKinds + 0x18 * o.kind + 1] << 8);
    o.frame = 0;
    o.grow -= 8;
    viewDirty_ = true;
}

}  // namespace edison
