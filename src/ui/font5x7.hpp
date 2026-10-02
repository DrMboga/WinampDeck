#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace winampdeck::ui {

// A 5×7 bitmap character: 7 rows, top first. In each row, bit 0 is the
// leftmost of the 5 pixels.
using Glyph = std::array<std::uint8_t, 7>;

constexpr int kGlyphWidth = 5;
constexpr int kGlyphHeight = 7;
// Glyph plus one pixel of spacing.
constexpr int kCharAdvance = kGlyphWidth + 1;

// The font's own glyph for a character, or nullptr if it has none.
const Glyph* findGlyph(char32_t codePoint);

// The plain character the font draws in place of one it lacks (é as e,
// — as -), or `codePoint` itself if there's no such substitute.
char32_t substitute(char32_t codePoint);

// What to draw for any character: its own glyph, else the closest one the font
// has (é as e, — as -), else '?'.
const Glyph& glyph(char32_t codePoint);

// Invalid UTF-8 bytes each become U+FFFD, which draws as '?'.
std::u32string decodeUtf8(std::string_view text);

}  // namespace winampdeck::ui
