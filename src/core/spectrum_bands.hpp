#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace winampdeck {

// The spectrum analyzer's arithmetic: a block of what's playing in, one level
// per bar out, bass on the left to treble on the right. No hardware and no
// threads, so it can be tested with synthetic tones.
//
// Left and right are mixed into one channel, windowed (Hann) and transformed.
// Each bar is a band of pitch, the bands spaced evenly in pitch (the same
// ratio from each to the next) from kLowHz to kHighHz. A bar's level is its
// band's power in decibels relative to a full-scale tone, mapped from
// kFloorDb..0 dB onto 0..1. These are starting values, to tune on the screen.
class SpectrumBands {
public:
    static constexpr std::size_t kBars = 15;
    static constexpr int kSampleRate = 48000;
    // About 43 ms of audio; a power of two, for the transform.
    static constexpr std::size_t kFrames = 2048;
    static constexpr double kLowHz = 60;
    static constexpr double kHighHz = 16000;
    // A band this far below a full-scale tone, or quieter, is an empty bar.
    static constexpr double kFloorDb = -60;

    // 0 (silent, or below kFloorDb) to 1 (a full-scale tone), per bar.
    using Levels = std::array<float, kBars>;

    SpectrumBands();
    ~SpectrumBands();

    SpectrumBands(const SpectrumBands&) = delete;
    SpectrumBands& operator=(const SpectrumBands&) = delete;

    // Where bar `bar`'s band starts, for bar in 0..kBars; bar kBars gives
    // where the last one ends.
    static double bandStartHz(std::size_t bar);

    // `interleaved` is stereo 16-bit audio at kSampleRate, left first, as
    // recorded from the loopback. Only its last kFrames frames are used; with
    // fewer, the rest counts as silence.
    Levels levels(std::span<const std::int16_t> interleaved);

private:
    struct Transform;

    std::unique_ptr<Transform> transform_;
    std::vector<float> window_;
    // The transform's bins each bar adds up: firstBin_[bar] to firstBin_[bar + 1].
    std::array<std::size_t, kBars + 1> firstBin_{};
};

}  // namespace winampdeck
