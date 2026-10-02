#pragma once

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string_view>

namespace winampdeck {

// The Deck's settings, from the hand-edited config.json (ADR 0006). Every
// setting is optional; a missing one keeps its default.
struct DeckConfig {
    static constexpr unsigned kMaxBrightness = 100;
    static constexpr std::chrono::milliseconds kMinScrollStep{50};
    static constexpr std::chrono::milliseconds kMaxScrollStep{5000};

    // "tft_brightness": the TFT backlight, in percent.
    unsigned tftBrightness = kMaxBrightness;
    // "lcd_scroll_ms": how long the LCD shows each scroll step.
    std::chrono::milliseconds lcdScrollStep{300};

    // The backlight as St7735 takes it, 0-255.
    unsigned tftBacklightLevel() const;

    bool operator==(const DeckConfig&) const = default;
};

// A config.json that can't be used, with the reason in the message.
class DeckConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Parses config.json: one JSON object, // and /* */ comments allowed. Unknown
// keys, wrong types and out-of-range values are errors rather than ignored,
// so a typo can't silently leave a setting at its default.
// Throws DeckConfigError.
DeckConfig parseDeckConfig(std::string_view text);

// Reads and parses the file. A missing file gives the defaults: the Deck works
// without one. Throws DeckConfigError if it exists but can't be read or used.
DeckConfig loadDeckConfig(const std::filesystem::path& path);

}  // namespace winampdeck
