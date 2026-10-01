#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace winampdeck::ui {

// RGB565, the ST7735's native pixel format: 5 bits red, 6 green, 5 blue.
using Color = std::uint16_t;

constexpr Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    return static_cast<Color>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Back to 8 bits per channel, the low bits filled in so white stays white.
struct Rgb888 {
    std::uint8_t r, g, b;
};
constexpr Rgb888 toRgb888(Color color) {
    const unsigned r = (color >> 11) & 0x1F;
    const unsigned g = (color >> 5) & 0x3F;
    const unsigned b = color & 0x1F;
    return {static_cast<std::uint8_t>((r << 3) | (r >> 2)),
            static_cast<std::uint8_t>((g << 2) | (g >> 4)),
            static_cast<std::uint8_t>((b << 3) | (b >> 2))};
}

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool empty() const { return width <= 0 || height <= 0; }
    int right() const { return x + width; }    // Exclusive.
    int bottom() const { return y + height; }  // Exclusive.
    Rect intersect(const Rect& other) const;

    bool operator==(const Rect&) const = default;
};

// Row-major RGB565 pixels, top row first.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<Color> pixels;

    Image() = default;
    Image(int w, int h, Color fill = 0);

    Color at(int x, int y) const { return pixels[static_cast<std::size_t>(y * width + x)]; }
    Color& at(int x, int y) { return pixels[static_cast<std::size_t>(y * width + x)]; }

    bool operator==(const Image&) const = default;
};

// Station logos are stored as raw RGB565, little-endian, row-major, no header
// (the format of the SABA Radio project's `.565` files). Returns nothing if
// the file is missing or isn't exactly width × height pixels.
std::optional<Image> loadRgb565File(const std::filesystem::path& path, int width, int height);

// Scales by area averaging: every source pixel contributes to the output pixel
// it falls in, weighted by how much of it falls there. Good for shrinking
// (cover art, logo thumbnails); acceptable for mild enlarging.
// `source` is 3 bytes per pixel (R, G, B), row-major.
Image resampleRgb888(std::span<const std::uint8_t> source, int width, int height, int outWidth,
                     int outHeight);
Image resample(const Image& image, int outWidth, int outHeight);

}  // namespace winampdeck::ui
