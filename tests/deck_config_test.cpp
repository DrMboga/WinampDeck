// config.json: what each setting does, and how mistakes are reported.

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "core/deck_config.hpp"

namespace winampdeck {
namespace {

using namespace std::chrono_literals;

std::string errorFrom(const std::string& text) {
    try {
        parseDeckConfig(text);
    } catch (const DeckConfigError& error) {
        return error.what();
    }
    return "(no error)";
}

TEST(DeckConfig, EmptyObjectGivesTheDefaults) {
    EXPECT_EQ(parseDeckConfig("{}"), DeckConfig{});
    EXPECT_EQ(DeckConfig{}.tftBrightness, 100u);
    EXPECT_EQ(DeckConfig{}.lcdScrollStep, 300ms);
}

TEST(DeckConfig, ReadsBothSettings) {
    const auto config = parseDeckConfig(R"({"tft_brightness": 75, "lcd_scroll_ms": 400})");
    EXPECT_EQ(config.tftBrightness, 75u);
    EXPECT_EQ(config.lcdScrollStep, 400ms);
}

TEST(DeckConfig, AllowsComments) {
    const auto config = parseDeckConfig(R"(
        // Dimmer at night.
        { "tft_brightness": /* percent */ 40 }
    )");
    EXPECT_EQ(config.tftBrightness, 40u);
}

TEST(DeckConfig, BrightnessBecomesTheBacklightLevel) {
    DeckConfig config;
    config.tftBrightness = 100;
    EXPECT_EQ(config.tftBacklightLevel(), 255u);
    config.tftBrightness = 75;
    EXPECT_EQ(config.tftBacklightLevel(), 191u);
    config.tftBrightness = 0;
    EXPECT_EQ(config.tftBacklightLevel(), 0u);
}

TEST(DeckConfig, UnknownSettingIsAnError) {
    EXPECT_EQ(errorFrom(R"({"tft_brightnes": 75})"),
              "unknown setting \"tft_brightnes\" (known: \"tft_brightness\", \"lcd_scroll_ms\")");
}

TEST(DeckConfig, OutOfRangeIsAnError) {
    EXPECT_EQ(errorFrom(R"({"tft_brightness": 101})"), "\"tft_brightness\" must be from 0 to 100, not 101");
    EXPECT_EQ(errorFrom(R"({"lcd_scroll_ms": 10})"), "\"lcd_scroll_ms\" must be from 50 to 5000, not 10");
    EXPECT_EQ(errorFrom(R"({"tft_brightness": -1})"), "\"tft_brightness\" must be from 0 to 100, not -1");
}

TEST(DeckConfig, WrongTypeIsAnError) {
    EXPECT_EQ(errorFrom(R"({"tft_brightness": "75%"})"), "\"tft_brightness\" must be a whole number");
    EXPECT_EQ(errorFrom(R"({"lcd_scroll_ms": 300.5})"), "\"lcd_scroll_ms\" must be a whole number");
}

TEST(DeckConfig, BadJsonIsAnError) {
    EXPECT_EQ(errorFrom(R"({"tft_brightness": 75,})").rfind("not valid JSON: ", 0), 0u);
    EXPECT_EQ(errorFrom("[1, 2]"), "must be a JSON object, { ... }");
}

TEST(DeckConfig, MissingFileGivesTheDefaults) {
    EXPECT_EQ(loadDeckConfig("/nonexistent/winampdeck/config.json"), DeckConfig{});
}

TEST(DeckConfig, FileErrorsNameTheFile) {
    const auto path = std::filesystem::temp_directory_path() / "winampdeck_config_test.json";
    std::ofstream(path) << R"({"lcd_scroll_ms": 0})";
    try {
        loadDeckConfig(path);
        ADD_FAILURE() << "no error";
    } catch (const DeckConfigError& error) {
        EXPECT_EQ(std::string(error.what()), path.string() + ": \"lcd_scroll_ms\" must be from 50 to 5000, not 0");
    }
    std::filesystem::remove(path);
}

TEST(DeckConfig, TheShippedFileIsTheDefaults) {
    EXPECT_EQ(loadDeckConfig(std::filesystem::path(WINAMPDECK_DATA_DIR) / "config.json"), DeckConfig{});
}

}  // namespace
}  // namespace winampdeck
