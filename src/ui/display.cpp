#include "ui/display.hpp"

#include <algorithm>

namespace winampdeck::ui {

namespace {

constexpr int kBandHeight = 8;

}  // namespace

std::vector<Rect> changedAreas(const Image& before, const Image& after) {
    std::vector<Rect> areas;
    for (int bandTop = 0; bandTop < after.height; bandTop += kBandHeight) {
        const int bandBottom = std::min(bandTop + kBandHeight, after.height);
        int left = after.width;
        int right = -1;
        int top = bandBottom;
        int bottom = -1;
        for (int y = bandTop; y < bandBottom; ++y) {
            for (int x = 0; x < after.width; ++x) {
                if (before.at(x, y) != after.at(x, y)) {
                    left = std::min(left, x);
                    right = std::max(right, x);
                    top = std::min(top, y);
                    bottom = std::max(bottom, y);
                }
            }
        }
        if (right >= 0) {
            areas.push_back({left, top, right - left + 1, bottom - top + 1});
        }
    }
    return areas;
}

std::vector<Color> crop(const Image& image, const Rect& area) {
    std::vector<Color> pixels;
    pixels.reserve(static_cast<std::size_t>(area.width * area.height));
    for (int y = area.y; y < area.bottom(); ++y) {
        const auto row = image.pixels.begin() + y * image.width;
        pixels.insert(pixels.end(), row + area.x, row + area.right());
    }
    return pixels;
}

}  // namespace winampdeck::ui
