#pragma once

#include <string_view>
#include <vector>

#include "ui/image.hpp"

namespace winampdeck::ui {

// Draws into an Image. Everything is clipped to the image and to the current
// clip rectangle.
class Canvas {
public:
    explicit Canvas(Image& target);

    int width() const { return target_.width; }
    int height() const { return target_.height; }

    // Restricts drawing to `area` (within the image) until reset.
    void setClip(const Rect& area);
    void resetClip();

    void fill(const Rect& area, Color color);
    void fill(Color color) { fill({0, 0, width(), height()}, color); }
    void pixel(int x, int y, Color color);
    void horizontalLine(int x, int y, int length, Color color) { fill({x, y, length, 1}, color); }
    void verticalLine(int x, int y, int length, Color color) { fill({x, y, 1, length}, color); }
    // A 1-pixel outline just inside `area`.
    void frame(const Rect& area, Color color);
    void blit(const Image& image, int x, int y);

    // One line of 5×7 text with its top-left at (x, y), each glyph pixel drawn
    // as a scale×scale block. Only glyph pixels are drawn; the background is
    // left alone. Returns the x just past the last character.
    int text(int x, int y, std::u32string_view text, Color color, int scale = 1);
    int text(int x, int y, std::string_view utf8, Color color, int scale = 1);

    // Width of `characters` characters of text, including the trailing gap.
    static int textWidth(std::size_t characters, int scale = 1);

private:
    Image& target_;
    Rect clip_;
};

}  // namespace winampdeck::ui
