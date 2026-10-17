// The spectrum analyzer's arithmetic, on synthetic tones.

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

#include "core/spectrum_bands.hpp"

namespace winampdeck {
namespace {

constexpr std::size_t kBars = SpectrumBands::kBars;

struct Tone {
    double hz;
    double amplitude = 1.0;  // Of full scale.
    bool left = true;
    bool right = true;
};

// A block of interleaved stereo 16-bit audio holding the given tones.
std::vector<std::int16_t> play(std::initializer_list<Tone> tones,
                               std::size_t frames = SpectrumBands::kFrames) {
    std::vector<std::int16_t> samples(frames * 2);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        double left = 0;
        double right = 0;
        for (const Tone& tone : tones) {
            const double value = tone.amplitude * std::sin(2 * std::numbers::pi * tone.hz * static_cast<double>(frame) /
                                                           SpectrumBands::kSampleRate);
            left += tone.left ? value : 0;
            right += tone.right ? value : 0;
        }
        samples[2 * frame] = static_cast<std::int16_t>(std::lround(left * 32767));
        samples[2 * frame + 1] = static_cast<std::int16_t>(std::lround(right * 32767));
    }
    return samples;
}

// The bar whose band holds `hz`.
std::size_t barFor(double hz) {
    std::size_t bar = 0;
    while (bar + 1 < kBars && SpectrumBands::bandStartHz(bar + 1) <= hz) {
        ++bar;
    }
    return bar;
}

// The middle of a bar's band, in pitch.
double middleOf(std::size_t bar) {
    return std::sqrt(SpectrumBands::bandStartHz(bar) * SpectrumBands::bandStartHz(bar + 1));
}

std::size_t loudest(const SpectrumBands::Levels& levels) {
    std::size_t best = 0;
    for (std::size_t bar = 1; bar < kBars; ++bar) {
        if (levels[bar] > levels[best]) {
            best = bar;
        }
    }
    return best;
}

std::size_t distance(std::size_t a, std::size_t b) { return a > b ? a - b : b - a; }

TEST(SpectrumBands, BandsRunFromTheLowestToTheHighestPitch) {
    EXPECT_DOUBLE_EQ(SpectrumBands::bandStartHz(0), 60.0);
    EXPECT_NEAR(SpectrumBands::bandStartHz(kBars), 16000.0, 1e-6);
    // The same ratio from each band to the next: a little over half an octave.
    const double ratio = SpectrumBands::bandStartHz(1) / SpectrumBands::bandStartHz(0);
    EXPECT_NEAR(ratio, 1.45, 0.01);
    EXPECT_NEAR(SpectrumBands::bandStartHz(8) / SpectrumBands::bandStartHz(7), ratio, 1e-9);
}

TEST(SpectrumBands, SilenceIsAllZero) {
    SpectrumBands bands;
    for (const float level : bands.levels(std::vector<std::int16_t>(SpectrumBands::kFrames * 2))) {
        EXPECT_EQ(level, 0.0f);
    }
}

TEST(SpectrumBands, NoAudioAtAllIsAllZero) {
    SpectrumBands bands;
    for (const float level : bands.levels({})) {
        EXPECT_EQ(level, 0.0f);
    }
}

TEST(SpectrumBands, AToneLightsItsBarAndNoBarFarFromIt) {
    SpectrumBands bands;
    for (std::size_t bar = 0; bar < kBars; ++bar) {
        const auto levels = bands.levels(play({{middleOf(bar)}}));

        EXPECT_EQ(loudest(levels), bar);
        EXPECT_GT(levels[bar], 0.9f) << "bar " << bar;
        // The window's side lobes can leave a trace just above the floor in
        // the narrow lowest bands (bar 0 is one bin wide), but never enough
        // to see: under 0.02 is about a pixel on the 57-pixel spectrum.
        for (std::size_t other = 0; other < kBars; ++other) {
            if (distance(other, bar) >= 3) {
                EXPECT_LT(levels[other], 0.02f) << "tone in bar " << bar << ", bar " << other;
            }
        }
    }
}

TEST(SpectrumBands, TonesAtTheEdgesOfTheRangeLandInTheFirstAndLastBars) {
    SpectrumBands bands;
    EXPECT_EQ(loudest(bands.levels(play({{70}}))), 0u);
    EXPECT_EQ(loudest(bands.levels(play({{14000}}))), kBars - 1);
}

TEST(SpectrumBands, ToneOfAGivenPitchLandsInTheBarHoldingIt) {
    SpectrumBands bands;
    for (const double hz : {100.0, 440.0, 1000.0, 3000.0, 8000.0}) {
        EXPECT_EQ(loudest(bands.levels(play({{hz}}))), barFor(hz)) << hz << " Hz";
    }
}

TEST(SpectrumBands, LouderIsHigherAndFullScaleReachesTheTop) {
    SpectrumBands bands;
    const std::size_t bar = barFor(1000);
    const float quiet = bands.levels(play({{1000, 0.01}}))[bar];
    const float medium = bands.levels(play({{1000, 0.1}}))[bar];
    const float full = bands.levels(play({{1000, 1.0}}))[bar];

    EXPECT_GT(quiet, 0.0f);
    EXPECT_GT(medium, quiet);
    EXPECT_GT(full, medium);
    EXPECT_GT(full, 0.95f);
    EXPECT_LE(full, 1.0f);
    // 20 dB apart is a third of the 60 dB range.
    EXPECT_NEAR(medium - quiet, 1.0f / 3, 0.02f);
}

TEST(SpectrumBands, ClippedAudioDoesNotOverflow) {
    SpectrumBands bands;
    // Two full-scale tones in one band, beyond what 16 bits can hold.
    std::vector<std::int16_t> samples(SpectrumBands::kFrames * 2);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = (i / 2) % 20 < 10 ? 32767 : -32768;  // A 2.4 kHz square wave, at the limits.
    }
    for (const float level : bands.levels(samples)) {
        EXPECT_GE(level, 0.0f);
        EXPECT_LE(level, 1.0f);
    }
}

TEST(SpectrumBands, AToneInOneChannelOnlyStillRegisters) {
    SpectrumBands bands;
    const std::size_t bar = barFor(1000);
    const float both = bands.levels(play({{1000}}))[bar];
    const float left = bands.levels(play({{1000, 1.0, true, false}}))[bar];
    const float right = bands.levels(play({{1000, 1.0, false, true}}))[bar];

    EXPECT_GT(left, 0.8f);
    EXPECT_FLOAT_EQ(left, right);
    // Half the amplitude in the mix: 6 dB down.
    EXPECT_NEAR(both - left, 6.0f / 60, 0.01f);
}

TEST(SpectrumBands, ALowAndAHighToneLightTwoSeparateBars) {
    SpectrumBands bands;
    const auto levels = bands.levels(play({{100, 0.5}, {5000, 0.5}}));
    const std::size_t low = barFor(100);
    const std::size_t high = barFor(5000);

    EXPECT_GT(levels[low], 0.8f);
    EXPECT_GT(levels[high], 0.8f);
    for (std::size_t bar = low + 3; bar + 3 <= high; ++bar) {
        EXPECT_EQ(levels[bar], 0.0f) << "bar " << bar;
    }
}

TEST(SpectrumBands, UsesOnlyTheMostRecentAudio) {
    SpectrumBands bands;
    // A long block: an old low tone, then a recent high one filling the window.
    auto samples = play({{100}}, SpectrumBands::kFrames);
    const auto recent = play({{5000}}, SpectrumBands::kFrames);
    samples.insert(samples.end(), recent.begin(), recent.end());

    const auto levels = bands.levels(samples);

    EXPECT_EQ(loudest(levels), barFor(5000));
    EXPECT_EQ(levels[barFor(100)], 0.0f);
}

TEST(SpectrumBands, AShortBlockIsPaddedWithSilence) {
    SpectrumBands bands;
    const auto levels = bands.levels(play({{1000}}, SpectrumBands::kFrames / 2));

    EXPECT_EQ(loudest(levels), barFor(1000));
}

}  // namespace
}  // namespace winampdeck
