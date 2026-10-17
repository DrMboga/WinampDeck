// Measures what the spectrum analyzer's arithmetic costs on this machine
// (Phase 12: measured on the Pi 3 before the analyzer is built on it).
//
//   winampdeck-spectrum-bench [CALLS]
//
// Runs SpectrumBands::levels on a block of noise CALLS times (default 2000)
// and prints the time per call and what 20 calls a second take of one core.

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <vector>

#include "core/spectrum_bands.hpp"

int main(int argc, char** argv) {
    using winampdeck::SpectrumBands;

    const long calls = argc > 1 ? std::strtol(argv[1], nullptr, 10) : 2000;
    if (calls <= 0) {
        std::cerr << "Usage: winampdeck-spectrum-bench [CALLS]\n";
        return 2;
    }

    std::mt19937 random{1};
    std::uniform_int_distribution<int> sample(-16000, 16000);
    std::vector<std::int16_t> block(SpectrumBands::kFrames * 2);
    for (auto& value : block) {
        value = static_cast<std::int16_t>(sample(random));
    }

    SpectrumBands bands;
    float sink = 0;  // So the work can't be optimised away.
    bands.levels(block);  // Warm up.
    const auto start = std::chrono::steady_clock::now();
    for (long i = 0; i < calls; ++i) {
        sink += bands.levels(block)[static_cast<std::size_t>(i) % SpectrumBands::kBars];
    }
    const std::chrono::duration<double, std::micro> took = std::chrono::steady_clock::now() - start;

    const double perCall = took.count() / static_cast<double>(calls);
    std::cout << calls << " calls of " << SpectrumBands::kFrames << " frames: " << perCall << " us each; "
              << "at 20 a second, " << perCall * 20 / 1e6 * 100 << "% of one core"
              << (sink < 0 ? "" : "") << std::endl;
    return 0;
}
