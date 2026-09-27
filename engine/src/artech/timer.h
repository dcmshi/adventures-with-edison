#pragma once

#include <array>
#include <cstdint>
#include <functional>

namespace edison {

// The Artech library's timer (the *ARTDLL.DLL TIMERCALLBACK and
// set_periodic): a 13 ms tick runs up to 10 periodic callbacks. Each adds
// its rate in Hz to an accumulator and fires when the sum reaches
// 1000 / 13 = 76, then subtracts 76.
class Timer {
public:
    static constexpr uint32_t kTickMs = 13;
    static constexpr uint32_t kThreshold = 1000 / kTickMs;  // 76
    static constexpr int kSlots = 10;

    // Registers (or with rate 0 removes) the callback in `slot`.
    void setPeriodic(int slot, uint32_t rate, std::function<void()> fn);
    // Starts counting from `milliseconds` (the platform clock).
    void reset(uint64_t milliseconds) { ticks_ = milliseconds / kTickMs; }
    // Runs the timer ticks due by `milliseconds`.
    void advance(uint64_t milliseconds);

private:
    struct Periodic {
        uint32_t rate = 0;
        uint32_t acc = 0;
        std::function<void()> fn;
    };
    std::array<Periodic, kSlots> periodic_{};
    uint64_t ticks_ = 0;
};

}  // namespace edison
