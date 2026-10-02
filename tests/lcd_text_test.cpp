// What the LCD's character ROM is sent for each kind of text.

#include <gtest/gtest.h>

#include "ui/lcd_text.hpp"

namespace winampdeck::ui {
namespace {

TEST(LcdText, AsciiIsItself) {
    EXPECT_EQ(encodeLcdText("Daft Punk - One More Time (2000)"), "Daft Punk - One More Time (2000)");
}

TEST(LcdText, BackslashAndTildeAvoidTheRomsYenAndArrow) {
    EXPECT_EQ(encodeLcdText("AC\\DC ~ live"), "AC/DC - live");
}

TEST(LcdText, ControlCharactersBecomeSpaces) {
    EXPECT_EQ(encodeLcdText("a\tb\nc"), "a b c");
}

TEST(LcdText, GermanUsesTheRomsOwnLetters) {
    EXPECT_EQ(encodeLcdText("Grönemeyer — Männer"), "Gr\xEFnemeyer - M\xE1nner");
    EXPECT_EQ(encodeLcdText("Straße über"), "Stra\xE2" "e \xF5" "ber");
    EXPECT_EQ(encodeLcdText("Ärzte Öl Übel"), "Aerzte Oel Uebel");
}

TEST(LcdText, OtherRomCharacters) {
    EXPECT_EQ(encodeLcdText("Señor 20°C 5µs"), "Se\xEEor 20\xDF" "C 5\xE4s");
}

TEST(LcdText, CyrillicIsTransliterated) {
    EXPECT_EQ(encodeLcdText("Группа крови — Кино"), "Gruppa krovi - Kino");
    EXPECT_EQ(encodeLcdText("Щука Жёлтый Объём"), "Shchuka Zhyoltyy Obyom");
}

TEST(LcdText, AccentedLatinLosesItsAccent) {
    EXPECT_EQ(encodeLcdText("Café Noir — «Où?»"), "Cafe Noir - ?Ou??");
}

TEST(LcdText, AnythingElseIsAQuestionMark) {
    EXPECT_EQ(encodeLcdText("東京 ♪"), "?? ?");
}

TEST(LcdText, NeverUsesTheUserDefinedCharacters) {
    for (const char code : encodeLcdText(std::string("\x00\x01\x07", 3))) {
        EXPECT_GE(static_cast<unsigned char>(code), 0x08);
    }
}

}  // namespace
}  // namespace winampdeck::ui
