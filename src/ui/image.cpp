#include "ui/image.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>

namespace winampdeck::ui {

Rect Rect::intersect(const Rect& other) const {
    const int left = std::max(x, other.x);
    const int top = std::max(y, other.y);
    const int r = std::min(right(), other.right());
    const int b = std::min(bottom(), other.bottom());
    if (r <= left || b <= top) {
        return {};
    }
    return {left, top, r - left, b - top};
}

Image::Image(int w, int h, Color fill)
    : width(w), height(h), pixels(static_cast<std::size_t>(w * h), fill) {}

std::optional<Image> loadRgb565File(const std::filesystem::path& path, int width, int height) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(in),
                                           std::istreambuf_iterator<char>()};
    if (bytes.size() != static_cast<std::size_t>(width * height * 2)) {
        return std::nullopt;
    }
    Image image(width, height);
    for (std::size_t i = 0; i < image.pixels.size(); ++i) {
        image.pixels[i] = static_cast<Color>(bytes[2 * i] | (bytes[2 * i + 1] << 8));
    }
    return image;
}

namespace {

// For one axis: which source pixels each output pixel covers, and how much of
// each. Weights are in units of 1/outSize of a source pixel, so each output
// pixel's weights sum to `size`.
struct Span {
    int first = 0;
    std::vector<int> weights;
};

std::vector<Span> spans(int size, int outSize) {
    std::vector<Span> result(static_cast<std::size_t>(outSize));
    for (int o = 0; o < outSize; ++o) {
        // Output pixel o covers [o*size, (o+1)*size) in units of 1/outSize.
        const int begin = o * size;
        const int end = begin + size;
        Span& span = result[static_cast<std::size_t>(o)];
        span.first = begin / outSize;
        for (int s = span.first; s * outSize < end; ++s) {
            const int overlap = std::min(end, (s + 1) * outSize) - std::max(begin, s * outSize);
            span.weights.push_back(overlap);
        }
    }
    return result;
}

}  // namespace

Image resampleRgb888(std::span<const std::uint8_t> source, int width, int height, int outWidth,
                     int outHeight) {
    Image out(outWidth, outHeight);
    const auto columns = spans(width, outWidth);
    const auto rows = spans(height, outHeight);
    const long total = static_cast<long>(width) * height;

    for (int oy = 0; oy < outHeight; ++oy) {
        const Span& row = rows[static_cast<std::size_t>(oy)];
        for (int ox = 0; ox < outWidth; ++ox) {
            const Span& column = columns[static_cast<std::size_t>(ox)];
            long sum[3] = {0, 0, 0};
            for (std::size_t j = 0; j < row.weights.size(); ++j) {
                const int sy = row.first + static_cast<int>(j);
                for (std::size_t i = 0; i < column.weights.size(); ++i) {
                    const int sx = column.first + static_cast<int>(i);
                    const long weight = static_cast<long>(row.weights[j]) * column.weights[i];
                    const std::size_t at = static_cast<std::size_t>((sy * width + sx) * 3);
                    for (std::size_t c = 0; c < 3; ++c) {
                        sum[c] += weight * source[at + c];
                    }
                }
            }
            // Round to nearest.
            out.at(ox, oy) = rgb(static_cast<std::uint8_t>((sum[0] + total / 2) / total),
                                 static_cast<std::uint8_t>((sum[1] + total / 2) / total),
                                 static_cast<std::uint8_t>((sum[2] + total / 2) / total));
        }
    }
    return out;
}

Image resample(const Image& image, int outWidth, int outHeight) {
    std::vector<std::uint8_t> source;
    source.reserve(image.pixels.size() * 3);
    for (const Color pixel : image.pixels) {
        const Rgb888 c = toRgb888(pixel);
        source.insert(source.end(), {c.r, c.g, c.b});
    }
    return resampleRgb888(source, image.width, image.height, outWidth, outHeight);
}

}  // namespace winampdeck::ui
