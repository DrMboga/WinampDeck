#include "adapters/loopback_spectrum.hpp"

#include <alsa/asoundlib.h>

#include <algorithm>
#include <iostream>
#include <utility>

namespace winampdeck {

namespace {

constexpr unsigned kChannels = 2;
// The HAT's period, which the loopback delivers in (audio/asound.conf).
constexpr snd_pcm_uframes_t kPeriod = 6000;
// Enough to hold the most results a slow event loop could leave waiting.
constexpr std::size_t kMostDue = 64;

std::chrono::microseconds framesToTime(std::size_t frames) {
    return std::chrono::microseconds(static_cast<long long>(frames) * 1'000'000 / SpectrumBands::kSampleRate);
}

std::string alsaError(const std::string& what, int error) { return what + ": " + snd_strerror(error); }

// Closes the device however recording ends.
struct Closer {
    snd_pcm_t* pcm;
    ~Closer() { snd_pcm_close(pcm); }
};

}  // namespace

LoopbackSpectrum::LoopbackSpectrum(std::string device)
    : device_(std::move(device)), worker_([this] { run(); }) {}

LoopbackSpectrum::~LoopbackSpectrum() {
    stopping_ = true;
    wake_.notify_all();
    worker_.join();
}

SpectrumBands::Levels LoopbackSpectrum::levels() {
    const std::lock_guard lock(mutex_);
    const auto now = Clock::now();
    while (!due_.empty() && due_.front().at <= now) {
        shown_ = due_.front().levels;
        shownAt_ = due_.front().at;
        due_.pop_front();
    }
    if (now - shownAt_ > kStale) {
        return {};
    }
    return shown_;
}

void LoopbackSpectrum::run() {
    while (!stopping_) {
        const std::string problem = record();
        if (stopping_) {
            break;
        }
        report("Spectrum: " + problem + "; the bars stay flat, retrying");
        sleepUnlessStopped(kRetry);
    }
}

std::string LoopbackSpectrum::record() {
    snd_pcm_t* pcm = nullptr;
    if (const int error = snd_pcm_open(&pcm, device_.c_str(), SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
        error < 0) {
        return alsaError("can't open " + device_, error);
    }
    const Closer closer{pcm};

    // The loopback dictates format, rate and period from the playing side;
    // these are what it gives (audio/asound.conf).
    snd_pcm_hw_params_t* params = nullptr;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm, params);
    snd_pcm_uframes_t period = kPeriod;
    snd_pcm_uframes_t buffer = kPeriod * 4;
    if (const int error = snd_pcm_hw_params_set_access(pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED); error < 0) {
        return alsaError("can't record interleaved from " + device_, error);
    }
    if (const int error = snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_LE); error < 0) {
        return alsaError("can't record 16-bit from " + device_, error);
    }
    if (const int error = snd_pcm_hw_params_set_channels(pcm, params, kChannels); error < 0) {
        return alsaError("can't record stereo from " + device_, error);
    }
    if (const int error = snd_pcm_hw_params_set_rate(pcm, params, SpectrumBands::kSampleRate, 0); error < 0) {
        return alsaError("can't record at 48000 Hz from " + device_, error);
    }
    snd_pcm_hw_params_set_period_size_near(pcm, params, &period, nullptr);
    snd_pcm_hw_params_set_buffer_size_near(pcm, params, &buffer);
    if (const int error = snd_pcm_hw_params(pcm, params); error < 0) {
        return alsaError("can't set up " + device_, error);
    }
    if (const int error = snd_pcm_start(pcm); error < 0) {
        return alsaError("can't start recording " + device_, error);
    }
    report("Spectrum: listening to " + device_);

    std::vector<std::int16_t> chunk(kPeriod * kChannels);
    while (!stopping_) {
        // Wakes up at least this often, to notice being stopped.
        int error = snd_pcm_wait(pcm, 200);
        if (error == 0) {
            continue;  // Nothing playing: the loopback has no timing to deliver by.
        }
        while (error >= 0) {
            const snd_pcm_sframes_t frames = snd_pcm_readi(pcm, chunk.data(), kPeriod);
            if (frames == -EAGAIN) {
                break;
            }
            if (frames < 0) {
                error = static_cast<int>(frames);
                break;
            }
            analyse(chunk.data(), static_cast<std::size_t>(frames), Clock::now());
        }
        if (error < 0) {
            // An overrun, or the playing side stopping: start again quietly.
            if (const int recovered = snd_pcm_recover(pcm, error, 1); recovered < 0) {
                return alsaError("recording " + device_ + " failed", recovered);
            }
            snd_pcm_start(pcm);
            // In case it keeps failing straight away: don't spin.
            sleepUnlessStopped(std::chrono::milliseconds{50});
        }
    }
    return {};
}

void LoopbackSpectrum::analyse(const std::int16_t* frames, std::size_t count, Clock::time_point arrived) {
    std::vector<Due> results;
    std::size_t done = 0;
    while (done < count) {
        const std::size_t take = std::min(count - done, kStep - sinceStep_);
        history_.insert(history_.end(), frames + done * kChannels, frames + (done + take) * kChannels);
        done += take;
        sinceStep_ += take;
        if (sinceStep_ < kStep) {
            break;
        }
        sinceStep_ = 0;
        const std::size_t keep = SpectrumBands::kFrames * kChannels;
        if (history_.size() > keep) {
            history_.erase(history_.begin(), history_.end() - static_cast<std::ptrdiff_t>(keep));
        }
        // The last frame analysed played (count - done) frames before the
        // arrival; it's shown kDisplayDelay after that.
        results.push_back({arrived + kDisplayDelay - framesToTime(count - done), bands_.levels(history_)});
    }

    const std::lock_guard lock(mutex_);
    due_.insert(due_.end(), results.begin(), results.end());
    while (due_.size() > kMostDue) {
        due_.pop_front();
    }
}

void LoopbackSpectrum::report(const std::string& state) {
    // Only changes: a Deck that can't listen says so once, not every retry.
    if (state == reported_) {
        return;
    }
    reported_ = state;
    std::cout << state << std::endl;
}

bool LoopbackSpectrum::sleepUnlessStopped(std::chrono::milliseconds duration) {
    std::unique_lock lock(mutex_);
    return !wake_.wait_for(lock, duration, [this] { return stopping_.load(); });
}

}  // namespace winampdeck
