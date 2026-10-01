#pragma once

#include <span>
#include <vector>

#include "ui/image.hpp"

namespace winampdeck::ui {

// The TFT's glass: a 160×128 RGB565 surface (the panel's cutout holds the
// ST7735 in landscape). Backed in production by the ST7735 over SPI.
class Display {
public:
    static constexpr int kWidth = 160;
    static constexpr int kHeight = 128;

    virtual ~Display() = default;

    // Replaces the pixels of `area` (within the display) with `pixels`,
    // row-major, area.width × area.height of them.
    virtual void write(const Rect& area, std::span<const Color> pixels) = 0;
};

// The parts of `after` that differ from `before` (same size), as a few
// rectangles: one per 8-row band with changes, spanning just the changed
// columns and rows within it. Sending only these over SPI keeps an animated
// spectrum or a scrolling line from costing a full-screen write every frame.
std::vector<Rect> changedAreas(const Image& before, const Image& after);

// Copies `area` out of `image`, row-major.
std::vector<Color> crop(const Image& image, const Rect& area);

}  // namespace winampdeck::ui
