// AsioScheduler, the production Scheduler, on a real io_context.

#include <gtest/gtest.h>

#include <chrono>
#include <vector>

#include "adapters/asio_scheduler.hpp"

namespace winampdeck {
namespace {

using namespace std::chrono_literals;

TEST(AsioSchedulerTest, FiresTimersInDeadlineOrder) {
    asio::io_context io;
    AsioScheduler scheduler(io);
    std::vector<int> fired;

    scheduler.callAfter(20ms, [&] { fired.push_back(2); });
    scheduler.callAfter(5ms, [&] { fired.push_back(1); });
    io.run();

    EXPECT_EQ(fired, (std::vector<int>{1, 2}));
}

TEST(AsioSchedulerTest, CancelledTimerNeverFires) {
    asio::io_context io;
    AsioScheduler scheduler(io);
    bool fired = false;

    const auto timer = scheduler.callAfter(5ms, [&] { fired = true; });
    scheduler.cancel(timer);
    io.run();

    EXPECT_FALSE(fired);
}

TEST(AsioSchedulerTest, CancellingAFiredTimerIsANoOp) {
    asio::io_context io;
    AsioScheduler scheduler(io);
    int fired = 0;

    const auto timer = scheduler.callAfter(1ms, [&] { ++fired; });
    io.run();
    scheduler.cancel(timer);

    EXPECT_EQ(fired, 1);
}

TEST(AsioSchedulerTest, CallbackCanScheduleAnotherTimer) {
    asio::io_context io;
    AsioScheduler scheduler(io);
    int fired = 0;

    scheduler.callAfter(1ms, [&] {
        ++fired;
        scheduler.callAfter(1ms, [&] { ++fired; });
    });
    io.run();

    EXPECT_EQ(fired, 2);
}

}  // namespace
}  // namespace winampdeck
