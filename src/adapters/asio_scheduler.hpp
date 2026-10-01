#pragma once

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <system_error>
#include <utility>

#include "core/scheduler.hpp"

namespace winampdeck {

// Scheduler on the controller's Asio event loop: every callback runs on the
// thread running the io_context.
class AsioScheduler final : public Scheduler {
public:
    explicit AsioScheduler(asio::io_context& io) : io_(io) {}

    std::chrono::milliseconds now() const override {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
    }

    TimerId callAfter(std::chrono::milliseconds delay, std::function<void()> callback) override {
        const TimerId id = nextId_++;
        auto timer = std::make_unique<asio::steady_timer>(io_, delay);
        timer->async_wait(
            [this, id, callback = std::move(callback)](const std::error_code& error) mutable {
                if (error) {
                    return;  // Cancelled.
                }
                auto fire = std::move(callback);
                timers_.erase(id);
                fire();
            });
        timers_.emplace(id, std::move(timer));
        return id;
    }

    // Destroying the timer cancels its pending wait.
    void cancel(TimerId timer) override { timers_.erase(timer); }

private:
    asio::io_context& io_;
    TimerId nextId_ = 1;
    std::map<TimerId, std::unique_ptr<asio::steady_timer>> timers_;
};

}  // namespace winampdeck
