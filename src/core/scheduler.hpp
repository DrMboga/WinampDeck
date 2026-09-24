#pragma once

#include <chrono>
#include <cstdint>
#include <functional>

namespace winampdeck {

// One-shot timers on the controller's single event loop. Backed in production
// by Asio steady_timers; tests drive time by hand.
class Scheduler {
public:
    using TimerId = std::uint64_t;

    virtual ~Scheduler() = default;

    virtual TimerId callAfter(std::chrono::milliseconds delay, std::function<void()> callback) = 0;
    // Cancelling a timer that already fired or was already cancelled is a no-op.
    virtual void cancel(TimerId timer) = 0;
};

}  // namespace winampdeck
