#pragma once

#include "core/spectrum_bands.hpp"

namespace winampdeck::ui {

// Where the Now Playing spectrum's bar levels come from: in production, the
// analyzer listening to the loopback copy of what's playing (Phase 12).
class SpectrumSource {
public:
    virtual ~SpectrumSource() = default;

    // The newest levels, 0 to 1 per bar, bass first. Asked once per frame,
    // on the event loop's thread, so it must return at once.
    virtual SpectrumBands::Levels levels() = 0;
};

// Flat bars: nothing to listen to.
class SilentSpectrum final : public SpectrumSource {
public:
    SpectrumBands::Levels levels() override { return {}; }
};

}  // namespace winampdeck::ui
