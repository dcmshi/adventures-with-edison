#pragma once

#include <cstdint>
#include <list>
#include <map>
#include <vector>

#include "shell/screens.h"

namespace edison {

struct ShellContext;
struct ScriptContext;

// Sprite animations (archive group 50), EDISON.EXE segment 8.
//
// File: u16 frames, s16 x, s16 y, u16 ?, then per frame
//       { s16 dx, s16 dy, u16 bitmap, u8 ticks } (7 bytes).
// Frames are drawn at (x + dx, y + dy) for `ticks` game ticks (16 Hz).
// Each tick, the rectangles that changed are redrawn: backdrop (screen 3)
// to screen 2, every overlapping anim on top in list order, then the
// rectangle is copied to the display (screen 1).
class Anims {
public:
    explicit Anims(ShellContext& ctx) : ctx_(ctx) {}

    // STARTANIM (g08_020a). x, y of -1 mean the anim's own position.
    // `waiter` (WAIT) is resumed near the end. delay: ticks before the
    // first frame. layer: drawing order (higher on top). loop: repeat
    // forever. sound: WAV entry played with the first frame.
    void start(uint16_t anim, int x, int y, ScriptContext* waiter, uint8_t delay, uint8_t layer,
               bool loop, uint16_t sound);
    void stop(uint16_t anim);  // STOPANIM
    void stopAll();
    // A script ended: anims it was waiting on are stopped (f08_04da).
    void stopWaitedOnBy(const ScriptContext* script);
    void clear();

    // Advances every anim by one game tick if the clock has moved (f08_0b3e).
    void tick();

private:
    struct Frame {
        int16_t dx, dy;
        uint16_t bitmap;
        uint8_t ticks;
    };
    struct Def {
        uint16_t id = 0;
        int16_t x = 0, y = 0;
        std::vector<Frame> frames;
    };
    enum Flags : uint8_t { kStop = 1, kDead = 2, kLoop = 8, kFirst = 0x10 };
    struct Instance {
        const Def* def;
        int frame = 0;
        uint8_t counter = 0;
        uint8_t delay = 0;
        int x = 0, y = 0;
        ScriptContext* waiter = nullptr;
        uint8_t layer = 0;
        uint8_t flags = 0;
        uint16_t sound = 0;
        uint16_t bitmap = 0;  // frame shown now (0: none)
        Rect rect;
    };

    const Def* load(uint16_t anim);
    void step(Instance& a);
    void redraw(const Rect& r);

    ShellContext& ctx_;
    std::map<uint16_t, Def> defs_;
    std::list<Instance> list_;  // drawing order
    std::vector<Rect> dirty_;
    uint32_t nextTick_ = 0;
};

}  // namespace edison
