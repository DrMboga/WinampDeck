#include "core/deck_config.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

namespace winampdeck {

namespace {

using nlohmann::json;

std::int64_t integer(const json& value, const std::string& key, std::int64_t min, std::int64_t max) {
    if (!value.is_number_integer()) {
        throw DeckConfigError("\"" + key + "\" must be a whole number");
    }
    const auto number = value.get<std::int64_t>();
    if (number < min || number > max) {
        throw DeckConfigError("\"" + key + "\" must be from " + std::to_string(min) + " to " +
                              std::to_string(max) + ", not " + std::to_string(number));
    }
    return number;
}

}  // namespace

unsigned DeckConfig::tftBacklightLevel() const {
    return (tftBrightness * 255 + kMaxBrightness / 2) / kMaxBrightness;
}

DeckConfig parseDeckConfig(std::string_view text) {
    json document;
    try {
        document = json::parse(text, nullptr, true, /*ignore_comments=*/true);
    } catch (const json::parse_error& error) {
        throw DeckConfigError(std::string("not valid JSON: ") + error.what());
    }
    if (!document.is_object()) {
        throw DeckConfigError("must be a JSON object, { ... }");
    }

    DeckConfig config;
    for (const auto& [key, value] : document.items()) {
        if (key == "tft_brightness") {
            config.tftBrightness = static_cast<unsigned>(integer(value, key, 0, DeckConfig::kMaxBrightness));
        } else if (key == "lcd_scroll_ms") {
            config.lcdScrollStep = std::chrono::milliseconds(integer(
                value, key, DeckConfig::kMinScrollStep.count(), DeckConfig::kMaxScrollStep.count()));
        } else {
            throw DeckConfigError("unknown setting \"" + key +
                                  "\" (known: \"tft_brightness\", \"lcd_scroll_ms\")");
        }
    }
    return config;
}

DeckConfig loadDeckConfig(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return {};
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw DeckConfigError(path.string() + ": can't be opened");
    }
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    try {
        return parseDeckConfig(text);
    } catch (const DeckConfigError& problem) {
        throw DeckConfigError(path.string() + ": " + problem.what());
    }
}

}  // namespace winampdeck
