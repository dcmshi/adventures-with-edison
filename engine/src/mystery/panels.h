#pragma once

#include <functional>
#include <vector>

#include "artech/platform.h"
#include "artech/screens.h"

namespace edison {

// MALL.EXE segment 7: clickable panels. A game registers up to 10 panels;
// each has a rectangle and buttons (relative to the panel) and callbacks
// for a press, a release and each poll while held. The button rectangles
// usually come from tables in the data segment.
class Panels {
public:
    struct Button {
        int x, y, w, h;
    };
    struct Panel {
        int x = 0, y = 0, w = 0, h = 0;
        std::vector<Button> buttons;
        // Called with the button index, or -1 for the panel outside buttons.
        std::function<void(int)> onPress, onRelease, whileHeld;
    };

    void clear();              // f07_01d4
    int add(Panel panel);      // f07_0150; returns its slot
    Panel& operator[](int slot) { return panels_[slot]; }
    // One poll (f07_01fe): dispatches a new click, then release or held.
    // Returns true if there was a click (at *x, *y) that hit no panel.
    bool poll(Platform& platform, int* x = nullptr, int* y = nullptr);
    // P at a poll pauses the game (f06_219c: the key held, after each message).
    std::function<void()> onPause;

private:
    int hitPanel(int x, int y) const;
    int hitButton(int panel, int x, int y) const;

    std::vector<Panel> panels_;
    int active_ = -1;       // panel pressed ([81CA])
    int activeButton_ = -1;
};

}  // namespace edison
