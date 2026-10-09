#include "mystery/panels.h"

namespace edison {

void Panels::clear() {
    panels_.clear();
    active_ = activeButton_ = -1;
}

int Panels::add(Panel panel) {
    if (panels_.size() >= 10) return -1;
    panels_.push_back(std::move(panel));
    return static_cast<int>(panels_.size()) - 1;
}

int Panels::hitPanel(int x, int y) const {
    for (size_t i = 0; i < panels_.size(); ++i) {
        const Panel& p = panels_[i];
        if (p.x <= x && p.y <= y && y < p.y + p.h && x < p.x + p.w) return static_cast<int>(i);
    }
    return -1;
}

int Panels::hitButton(int panel, int x, int y) const {
    if (panel < 0) return -1;
    const Panel& p = panels_[panel];
    const int rx = x - p.x, ry = y - p.y;
    for (size_t i = 0; i < p.buttons.size(); ++i) {
        const Button& b = p.buttons[i];
        if (b.x <= rx && b.y <= ry && ry < b.y + (b.h & 0x7FFF) && rx < b.x + b.w) return static_cast<int>(i);
    }
    return -1;
}

bool Panels::poll(Platform& platform, int* outX, int* outY) {
    if (onPause && (platform.takeKeyIf('p') || platform.takeKeyIf('P'))) onPause();
    int x, y;
    bool missed = false;
    if (platform.takeClick(&x, &y)) {
        if (active_ >= 0 && panels_[active_].onRelease) panels_[active_].onRelease(activeButton_);
        active_ = hitPanel(x, y);
        activeButton_ = hitButton(active_, x, y);
        if (active_ >= 0 && panels_[active_].onPress) panels_[active_].onPress(activeButton_);
        if (active_ < 0) {
            missed = true;
            if (outX) *outX = x;
            if (outY) *outY = y;
        }
    }
    bool down;
    platform.mouse(&x, &y, &down);
    if (active_ < 0 || active_ >= static_cast<int>(panels_.size())) return missed;
    if (!down) {
        if (panels_[active_].onRelease) panels_[active_].onRelease(activeButton_);
        active_ = activeButton_ = -1;
    } else if (panels_[active_].whileHeld) {
        panels_[active_].whileHeld(activeButton_);
    }
    return missed;
}

}  // namespace edison
