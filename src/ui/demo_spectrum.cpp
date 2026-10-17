#include "ui/demo_spectrum.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <numbers>

namespace winampdeck::ui {

namespace {

constexpr double kBeat = 0.5;  // Seconds: 120 beats a minute.
// A minor pentatonic, in semitones from A4, one note per half beat.
constexpr std::array<int, 16> kMelody{0, 3, 5, 7, 10, 7, 5, 3, 0, -2, 0, 3, 7, 5, 3, -2};
constexpr std::array<int, 4> kBassLine{-24, -24, -21, -19};  // Semitones from A4, one per beat.

double note(int semitones) { return 440.0 * std::pow(2.0, semitones / 12.0); }

double tone(double hz, double t) { return std::sin(2 * std::numbers::pi * hz * t); }

}  // namespace

DemoSpectrum::DemoSpectrum(Scheduler& scheduler)
    : scheduler_(scheduler), block_(SpectrumBands::kFrames * 2) {}

SpectrumBands::Levels DemoSpectrum::levels() {
    const double now = std::chrono::duration<double>(scheduler_.now()).count();
    for (std::size_t frame = 0; frame < SpectrumBands::kFrames; ++frame) {
        const double t =
            now - static_cast<double>(SpectrumBands::kFrames - frame) / SpectrumBands::kSampleRate;
        const double beats = t / kBeat;
        const double inBeat = (beats - std::floor(beats)) * kBeat;  // Seconds since the beat.
        const auto beat = static_cast<long>(std::floor(beats));
        const auto half = static_cast<long>(std::floor(beats * 2));
        const double inHalf = (beats * 2 - std::floor(beats * 2)) * kBeat / 2;

        // The kick: a falling low thump on every beat.
        double value = 0.5 * std::exp(-inBeat * 12) * tone(50 + 40 * std::exp(-inBeat * 30), inBeat);
        // The bass line, and the melody with two overtones.
        value += 0.15 * tone(note(kBassLine[static_cast<std::size_t>(beat % 4)]), t);
        const double melody = note(kMelody[static_cast<std::size_t>(half % 16)]);
        const double pluck = std::exp(-inHalf * 4);
        value += pluck * (0.12 * tone(melody, t) + 0.05 * tone(2 * melody, t) + 0.02 * tone(3 * melody, t));
        // Hi-hats on the off-beats: short bursts of noise.
        if (half % 2 == 1) {
            noise_ ^= noise_ << 13;
            noise_ ^= noise_ >> 17;
            noise_ ^= noise_ << 5;
            const double white = static_cast<double>(noise_) / 4294967295.0 * 2 - 1;
            value += 0.08 * std::exp(-inHalf * 40) * white;
        }

        const auto sample = static_cast<std::int16_t>(std::lround(std::clamp(value, -1.0, 1.0) * 32767));
        block_[2 * frame] = sample;
        block_[2 * frame + 1] = sample;
    }
    return bands_.levels(block_);
}

}  // namespace winampdeck::ui
