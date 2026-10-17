#include "core/spectrum_bands.hpp"

#include <kiss_fftr.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace winampdeck {

namespace {

constexpr double kBinHz = static_cast<double>(SpectrumBands::kSampleRate) / SpectrumBands::kFrames;

// The power a full-scale tone puts into the positive-frequency bins, through
// the Hann window: (3/32)·N². A band holding all of it is at 0 dB.
constexpr double kFullScalePower =
    3.0 / 32.0 * static_cast<double>(SpectrumBands::kFrames) * static_cast<double>(SpectrumBands::kFrames);

}  // namespace

struct SpectrumBands::Transform {
    Transform() : config(kiss_fftr_alloc(static_cast<int>(kFrames), 0, nullptr, nullptr)) {}
    ~Transform() { kiss_fftr_free(config); }

    Transform(const Transform&) = delete;
    Transform& operator=(const Transform&) = delete;

    kiss_fftr_cfg config;
    std::vector<kiss_fft_scalar> input = std::vector<kiss_fft_scalar>(kFrames);
    std::vector<kiss_fft_cpx> output = std::vector<kiss_fft_cpx>(kFrames / 2 + 1);
};

SpectrumBands::SpectrumBands() : transform_(std::make_unique<Transform>()), window_(kFrames) {
    for (std::size_t i = 0; i < kFrames; ++i) {
        window_[i] = static_cast<float>(
            0.5 - 0.5 * std::cos(2 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(kFrames)));
    }
    // The lowest bands are only a bin or two wide; each gets at least one.
    for (std::size_t bar = 0; bar <= kBars; ++bar) {
        firstBin_[bar] = static_cast<std::size_t>(std::ceil(bandStartHz(bar) / kBinHz));
        if (bar > 0 && firstBin_[bar] <= firstBin_[bar - 1]) {
            firstBin_[bar] = firstBin_[bar - 1] + 1;
        }
    }
}

SpectrumBands::~SpectrumBands() = default;

double SpectrumBands::bandStartHz(std::size_t bar) {
    return kLowHz * std::pow(kHighHz / kLowHz, static_cast<double>(bar) / kBars);
}

SpectrumBands::Levels SpectrumBands::levels(std::span<const std::int16_t> interleaved) {
    const std::size_t frames = std::min(interleaved.size() / 2, kFrames);
    const std::span<const std::int16_t> recent = interleaved.last(frames * 2);
    const std::size_t padding = kFrames - frames;

    std::fill_n(transform_->input.begin(), padding, kiss_fft_scalar{0});
    for (std::size_t i = 0; i < frames; ++i) {
        const float mono = (static_cast<float>(recent[2 * i]) + static_cast<float>(recent[2 * i + 1])) / 65536.0f;
        transform_->input[padding + i] = mono * window_[padding + i];
    }
    kiss_fftr(transform_->config, transform_->input.data(), transform_->output.data());

    Levels levels{};
    for (std::size_t bar = 0; bar < kBars; ++bar) {
        double power = 0;
        for (std::size_t bin = firstBin_[bar]; bin < firstBin_[bar + 1]; ++bin) {
            const kiss_fft_cpx& value = transform_->output[bin];
            power += static_cast<double>(value.r) * value.r + static_cast<double>(value.i) * value.i;
        }
        if (power <= 0) {
            continue;
        }
        const double decibels = 10 * std::log10(power / kFullScalePower);
        levels[bar] = static_cast<float>(std::clamp((decibels - kFloorDb) / -kFloorDb, 0.0, 1.0));
    }
    return levels;
}

}  // namespace winampdeck
