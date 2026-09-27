// WINMAIN's segment 34: buttons (bevelled or not, bitmap faces, filled
// faces, labels, toggles and radio groups) in a list the game polls.
// (Sliders, flags 08 and 10, aren't ported yet: no activity uses one so far.)

#include <algorithm>
#include <cctype>

#include "rockbach/rockbach.h"

namespace edison {

void RockBach::initWidgets(std::vector<Widget>& list, Bevel bevel) {
    // f34_0ebc: the list in use and its bevel colours; each widget drawn
    // (pressed when its 2 flag is set; a toggle then starts on).
    widgets_ = &list;
    bevel_ = bevel;
    for (Widget& w : list) {
        w.toggle = false;
        if (w.flags & Widget::kSlider) {
            placeSlider(w, false);
            continue;
        }
        if (w.flags & Widget::kPressed) {
            drawWidget(w, true);
            if (w.flags & Widget::kToggle) w.toggle = true;
        } else {
            drawWidget(w, false);
        }
    }
}

void RockBach::drawWidget(const Widget& w, bool pressed) {
    // f34_07b8.
    if (w.flags & Widget::kHidden) return;
    const uint8_t l = pressed ? bevel_.d : bevel_.a, t = pressed ? bevel_.c : bevel_.b;
    const uint8_t b = pressed ? bevel_.b : bevel_.c, r = pressed ? bevel_.a : bevel_.d;
    // The bevel only on the display.
    if (current() == 1 && !(w.flags & Widget::kNoBevel)) {
        line(w.x0 + 1, w.y0 + 1, w.x0 + 1, w.y1 - 1, l);
        line(w.x0, w.y0, w.x0, w.y1, l);
        line(w.x0 + 1, w.y0 + 1, w.x1 - 1, w.y0 + 1, t);
        line(w.x0, w.y0, w.x1, w.y0, t);
        line(w.x0 + 1, w.y1, w.x1, w.y1, b);
        line(w.x0 + 2, w.y1 - 1, w.x1 - 1, w.y1 - 1, b);
        line(w.x1 - 1, w.y0 + 2, w.x1 - 1, w.y1 - 1, r);
        line(w.x1, w.y0 + 1, w.x1, w.y1, r);
    }
    if (w.flags & Widget::kFace)
        fill(w.x0 + 2, w.y0 + 2, w.x1 - w.x0 - 3, w.y1 - w.y0 - 3, pressed ? w.faceDown : w.faceUp);
    if (w.flags & Widget::kBitmap) {
        const int x = w.x0 + 2, y = w.y0 + 2;
        const uint16_t face = pressed ? w.bitmapDown : w.bitmapUp;
        if (w.mode == 'c') drawLogo(x, y, face);
        else drawOpaque(x, y, face);
        drawLogo(x, y, pressed ? w.overlayDown : w.overlayUp);
        if (w.mode == 'b') drawLogo(x, y, pressed ? w.overlayUp : w.overlayDown);
    }
    if (w.flags & Widget::kLabel) {
        // f34_0000: centred, or cut to fit from the left.
        std::string s = w.label;
        int tw = font_->width(s);
        const int avail = w.x1 - w.x0 - 4, y = w.y0 + (w.y1 - w.y0) / 2 - 8 + 1;
        if (avail > tw + 2) {
            text(w.x0 + avail / 2 - tw / 2, y, s, 0);
        } else {
            while (!s.empty() && avail <= tw + 2) {
                s.pop_back();
                tw = font_->width(s);
            }
            text(w.x0 + 2, y, s, 0);
        }
    }
}

void RockBach::showWidgets(std::initializer_list<int> which) {
    for (int i : which) {
        Widget& w = (*widgets_)[i];
        w.flags &= ~Widget::kHidden;
        drawWidget(w, w.flags & Widget::kPressed);
    }
}

void RockBach::hideWidgets() {
    for (Widget& w : *widgets_) w.flags |= Widget::kHidden;
}

int RockBach::pollWidgets() {
    // f34_0fb6: one poll; the widget pressed (drawn pressed until the
    // button comes up), or -1. A key is a widget's hot key or is left in
    // lastKey_ for the caller.
    ctx_.pump();
    lastKey_ = 0;
    lastClick_.on = false;
    std::vector<Widget>& list = *widgets_;
    auto find = [&](auto hit) {
        for (size_t i = 0; i < list.size(); ++i)
            if (!(list[i].flags & Widget::kHidden) && hit(list[i])) return static_cast<int>(i);
        return -1;
    };
    int index = -1;
    bool byKey = false;
    if (const int key = ctx_.platform.takeKey()) {
        const int upper = key < 0x80 ? std::toupper(key) : key;
        index = find([&](const Widget& w) { return w.hotkey == upper; });
        if (index < 0) lastKey_ = key;
        byKey = index >= 0;
    }
    int x, y;
    if (index < 0 && ctx_.platform.takeClick(&x, &y)) {
        index = find([&](const Widget& w) { return x >= w.x0 && x <= w.x1 && y >= w.y0 && y <= w.y1; });
        if (index < 0) lastClick_ = {true, x, y};
    }
    if (index < 0) return -1;
    Widget& w = list[index];
    if ((w.flags & Widget::kSlider) && w.slider) {
        // Dragged when the click is on the knob; either way its index.
        const Slider& s = *w.slider;
        if (!byKey && x >= s.kx && x < s.kx + s.w && y >= s.ky && y < s.ky + s.h) dragSlider(w);
        return index;
    }
    if (w.flags & Widget::kToggle) w.toggle = !w.toggle;
    if (!(w.flags & Widget::kPressed)) {
        drawWidget(w, true);
        w.flags |= Widget::kPressed;
    }
    if (w.flags & Widget::kRadio)
        for (Widget& o : list)
            if (&o != &w && o.group == w.group && (o.flags & Widget::kPressed)) {
                drawWidget(o, false);
                o.flags &= ~Widget::kPressed;
            }
    // Until the button comes up. (The original tests "still inside" with the
    // position from before the press, so it never changes.)
    for (bool down = !byKey; down;) {
        ctx_.pump();
        int mx, my;
        ctx_.platform.mouse(&mx, &my, &down);
    }
    if (!(w.flags & Widget::kRadio) && !w.toggle && (w.flags & Widget::kPressed)) {
        drawWidget(w, false);
        w.flags &= ~Widget::kPressed;
    }
    return index;
}

void RockBach::placeSlider(Widget& w, bool onDisplay) {
    // f34_0d6c (drawn on the current screen) / f34_0e14 (on screen 2, then
    // onto the display).
    if (!w.slider) return;
    Slider& s = *w.slider;
    const int k = (s.value - s.min) * s.len / (s.max - s.min + 1);
    s.kx = w.x0 + (w.flags & 0x08 ? k : 0);
    s.ky = w.y0 + (w.flags & 0x08 ? 0 : k);
    drawSlider(w, onDisplay);
}

void RockBach::drawSlider(const Widget& w, bool onDisplay) {
    // f34_0372 / f34_0168: the track, then the knob.
    const Slider& s = *w.slider;
    const bool across = w.flags & 0x08;
    const int previous = current();
    if (onDisplay) select(2);
    const int tx = across ? w.x0 - s.c : w.x0 - s.a, ty = across ? w.y0 - s.a : w.y0 - s.c;
    if (w.mode == 'c') drawLogo(tx, ty, w.bitmapUp);
    else drawOpaque(tx, ty, w.bitmapUp);
    drawLogo(s.kx, s.ky, w.overlayUp);
    if (onDisplay) {
        if (across) copyArea(2, 1, w.x0 - s.c, w.y0, s.len + s.c, s.h + 2);
        else copyArea(2, 1, w.x0, w.y0 - s.c, s.w + 2, s.len + s.c);
        select(previous);
    }
}

void RockBach::dragSlider(Widget& w) {
    // f34_04dc: the knob follows the mouse along the track while the button
    // is down; then the value comes from where it is.
    Slider& s = *w.slider;
    const bool across = w.flags & 0x08;
    int mx, my;
    bool down = true;
    ctx_.platform.mouse(&mx, &my, &down);
    int last = across ? mx : my;
    while (down) {
        ctx_.pump();
        ctx_.platform.mouse(&mx, &my, &down);
        const int now = across ? mx : my;
        if (now == last) continue;
        int& knob = across ? s.kx : s.ky;
        const int start = across ? w.x0 : w.y0;
        knob = std::clamp(knob + now - last, start, start + s.len - (across ? s.w : s.h));
        last = now;
        drawSlider(w, true);
    }
    s.value = ((across ? s.kx - w.x0 : s.ky - w.y0) * (s.max - s.min + 1)) / s.len;
    placeSlider(w, true);
}

}  // namespace edison
