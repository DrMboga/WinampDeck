// The 5x7 bitmap font and UTF-8 decoding.

#include <gtest/gtest.h>

#include <string>

#include "ui/font5x7.hpp"

namespace winampdeck::ui {
namespace {

TEST(FontTest, CoversAsciiGermanAndRussian) {
    for (char32_t c = U' '; c <= U'~'; ++c) {
        EXPECT_NE(findGlyph(c), nullptr) << static_cast<unsigned>(c);
    }
    for (const char32_t c : std::u32string(U"ÄÖÜäöüß")) {
        EXPECT_NE(findGlyph(c), nullptr) << static_cast<unsigned>(c);
    }
    for (char32_t c = U'А'; c <= U'я'; ++c) {
        EXPECT_NE(findGlyph(c), nullptr) << static_cast<unsigned>(c);
    }
    EXPECT_NE(findGlyph(U'Ё'), nullptr);
    EXPECT_NE(findGlyph(U'ё'), nullptr);
}

TEST(FontTest, BitZeroIsTheLeftmostPixel) {
    // 'L': a full-height left column, a full bottom row.
    const Glyph& l = glyph(U'L');
    for (int row = 0; row < 6; ++row) {
        EXPECT_EQ(l[static_cast<std::size_t>(row)], 0x01);
    }
    EXPECT_EQ(l[6], 0x1F);
}

TEST(FontTest, MissingCharactersFallBackToTheClosestOrAQuestionMark) {
    EXPECT_EQ(findGlyph(U'é'), nullptr);
    EXPECT_EQ(glyph(U'é'), glyph(U'e'));
    EXPECT_EQ(glyph(U'—'), glyph(U'-'));
    EXPECT_EQ(glyph(U'’'), glyph(U'\''));
    EXPECT_EQ(glyph(U'日'), glyph(U'?'));
}

TEST(FontTest, DecodesUtf8) {
    EXPECT_EQ(decodeUtf8("Abc"), U"Abc");
    EXPECT_EQ(decodeUtf8("Наше Радио"), U"Наше Радио");
    EXPECT_EQ(decodeUtf8("Motörhead — Ace"), U"Motörhead — Ace");
    EXPECT_EQ(decodeUtf8("\xF0\x9F\x8E\xB5"), U"\U0001F3B5");
}

TEST(FontTest, InvalidUtf8BecomesReplacementCharacters) {
    EXPECT_EQ(decodeUtf8("a\xFFz"), U"a\uFFFDz");
    EXPECT_EQ(decodeUtf8("\xD0"), U"\uFFFD");          // Truncated sequence.
    EXPECT_EQ(decodeUtf8("\xD0x"), U"\uFFFDx");        // Bad continuation byte.
    EXPECT_EQ(glyph(U'\uFFFD'), glyph(U'?'));
}

}  // namespace
}  // namespace winampdeck::ui
