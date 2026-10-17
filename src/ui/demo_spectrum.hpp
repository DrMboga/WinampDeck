#pragma once

#include <cstdint>
#include <vector>

#include "core/scheduler.hpp"
#include "core/spectrum_bands.hpp"
#include "ui/spectrum_source.hpp"

namespace winampdeck::ui {

// A stand-in for the analyzer where there's no audio to listen to (the TFT
// preview and the panel test): a few seconds of synthetic music, looped (a
// kick drum, a bass line, a melody and hi-hats), played through the real
// SpectrumBands, so the bars move the way real ones do.
class DemoSpectrum final : public SpectrumSource {
public:
    explicit DemoSpectrum(Scheduler& scheduler);

    SpectrumBands::Levels levels() override;

private:
    Scheduler& scheduler_;
    SpectrumBands bands_;
    std::vector<std::int16_t> block_;
    std::uint32_t noise_ = 0x2545F491;
};

}  // namespace winampdeck::ui
