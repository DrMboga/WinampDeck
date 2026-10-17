#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/spectrum_bands.hpp"
#include "ui/spectrum_source.hpp"

namespace winampdeck {

// The spectrum analyzer (Phase 12): records the loopback card's copy of
// what's playing on a background thread, turns it into bar levels with
// SpectrumBands, and hands the TFT view the levels due now.
//
// The loopback is timed by the HAT, so audio arrives a HAT period at a time:
// 6000 frames every 125 ms. Each arrival is analysed every kStep frames and
// the results are released over the following period, offset by
// kDisplayDelay, so the bars move smoothly at the view's frame rate instead
// of jumping 8 times a second.
//
// When nothing plays, the loopback delivers nothing and the bars go flat. If
// the device can't be opened or fails, the bars stay flat, the reason is
// logged once, and it's reopened every kRetry until it works again.
class LoopbackSpectrum final : public ui::SpectrumSource {
public:
    static constexpr std::size_t kStep = 1500;  // Frames: 4 results per HAT period.
    static constexpr std::chrono::milliseconds kDisplayDelay{125};
    // Levels older than this mean nothing is playing.
    static constexpr std::chrono::milliseconds kStale{250};
    static constexpr std::chrono::seconds kRetry{2};

    // `device` is the capture end of the loopback, by name: "hw:Loopback,1,0".
    explicit LoopbackSpectrum(std::string device);
    // Stops recording; takes at most a fraction of a second.
    ~LoopbackSpectrum() override;

    LoopbackSpectrum(const LoopbackSpectrum&) = delete;
    LoopbackSpectrum& operator=(const LoopbackSpectrum&) = delete;

    // On the event loop's thread.
    SpectrumBands::Levels levels() override;

private:
    using Clock = std::chrono::steady_clock;

    struct Due {
        Clock::time_point at;
        SpectrumBands::Levels levels;
    };

    void run();
    // Records until it fails or is stopped. Returns why it stopped, or
    // nothing if asked to.
    std::string record();
    // Analyses newly recorded frames that arrived at `arrived`.
    void analyse(const std::int16_t* frames, std::size_t count, Clock::time_point arrived);
    void report(const std::string& state);
    bool sleepUnlessStopped(std::chrono::milliseconds duration);

    const std::string device_;
    SpectrumBands bands_;
    // The most recent recorded frames, interleaved, enough for one analysis.
    std::vector<std::int16_t> history_;
    std::size_t sinceStep_ = 0;
    std::string reported_;

    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Due> due_;
    SpectrumBands::Levels shown_{};
    Clock::time_point shownAt_{};
    std::atomic<bool> stopping_{false};
    std::thread worker_;
};

}  // namespace winampdeck
