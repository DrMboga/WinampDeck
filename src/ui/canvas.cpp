#include "ui/canvas.hpp"

#include "ui/font5x7.hpp"

namespace winampdeck::ui {

Canvas::Canvas(Image& target) : target_(target), clip_{0, 0, target.width, target.height} {}

void Canvas::setClip(const Rect& area) {
    clip_ = area.intersect({0, 0, width(), height()});
}

void Canvas::resetClip() {
    clip_ = {0, 0, width(), height()};
}

void Canvas::fill(const Rect& area, Color color) {
    const Rect visible = area.intersect(clip_);
    for (int y = visible.y; y < visible.bottom(); ++y) {
        for (int x = visible.x; x < visible.right(); ++x) {
            target_.at(x, y) = color;
        }
    }
}

void Canvas::pixel(int x, int y, Color color) {
    if (x >= clip_.x && x < clip_.right() && y >= clip_.y && y < clip_.bottom()) {
        target_.at(x, y) = color;
    }
}

void Canvas::frame(const Rect& area, Color color) {
    horizontalLine(area.x, area.y, area.width, color);
    horizontalLine(area.x, area.bottom() - 1, area.width, color);
    verticalLine(area.x, area.y, area.height, color);
    verticalLine(area.right() - 1, area.y, area.height, color);
}

void Canvas::blit(const Image& image, int x, int y) {
    const Rect visible = Rect{x, y, image.width, image.height}.intersect(clip_);
    for (int ty = visible.y; ty < visible.bottom(); ++ty) {
        for (int tx = visible.x; tx < visible.right(); ++tx) {
            target_.at(tx, ty) = image.at(tx - x, ty - y);
        }
    }
}

int Canvas::text(int x, int y, std::u32string_view text, Color color, int scale) {
    for (const char32_t character : text) {
        // Skip glyphs entirely outside the clip; long marquee lines are mostly
        // off-screen.
        if (x < clip_.right() && x + kGlyphWidth * scale > clip_.x) {
            const Glyph& bitmap = glyph(character);
            for (int row = 0; row < kGlyphHeight; ++row) {
                for (int column = 0; column < kGlyphWidth; ++column) {
                    if ((bitmap[static_cast<std::size_t>(row)] >> column) & 1) {
                        fill({x + column * scale, y + row * scale, scale, scale}, color);
                    }
                }
            }
        }
        x += kCharAdvance * scale;
    }
    return x;
}

int Canvas::text(int x, int y, std::string_view utf8, Color color, int scale) {
    return text(x, y, decodeUtf8(utf8), color, scale);
}

int Canvas::textWidth(std::size_t characters, int scale) {
    return static_cast<int>(characters) * kCharAdvance * scale;
}

}  // namespace winampdeck::ui
