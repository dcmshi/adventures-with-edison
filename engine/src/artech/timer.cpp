#include "artech/timer.h"

namespace edison {

void Timer::setPeriodic(int slot, uint32_t rate, std::function<void()> fn) {
    periodic_[slot].rate = rate;
    periodic_[slot].acc = 0;
    periodic_[slot].fn = rate ? std::move(fn) : nullptr;
}

void Timer::advance(uint64_t milliseconds) {
    const uint64_t target = milliseconds / kTickMs;
    // After a long stall (window dragged, debugger), don't replay minutes.
    if (target > ticks_ + 100) ticks_ = target - 100;
    while (ticks_ < target) {
        ++ticks_;
        for (auto& p : periodic_) {
            if (!p.rate) continue;
            p.acc += p.rate;
            if (p.acc >= kThreshold) {
                p.acc -= kThreshold;
                p.fn();
            }
        }
    }
}

}  // namespace edison
