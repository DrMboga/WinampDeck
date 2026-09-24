#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include "core/types.hpp"

namespace winampdeck {

enum class PlaybackStatus { Stopped, Playing, Paused };

// What the TFT shows during normal playback. Rendering it (skin, progress bar,
// decorative spectrum) is the TFT view's job, not PlayerController's.
struct NowPlayingScreen {
    Source source = Source::Stopped;
    PlaybackStatus status = PlaybackStatus::Stopped;
    std::string artist;   // Spotify only.
    std::string title;    // Spotify: track title. Radio: stream title.
    std::string station;  // Radio only.
    std::string logo;     // Radio only: the Station's logo filename.
    std::chrono::milliseconds position{0};  // Spotify only.
    std::chrono::milliseconds duration{0};  // Spotify only.

    bool operator==(const NowPlayingScreen&) const = default;
};

// The Station List, open while Internet Radio is the Source.
struct StationListScreen {
    std::vector<Station> stations;
    std::size_t selected = 0;  // Highlighted entry.
    std::size_t tuned = 0;     // Currently tuned Station.

    bool operator==(const StationListScreen&) const = default;
};

using Screen = std::variant<NowPlayingScreen, StationListScreen>;

// The panel: buttons in, LEDs/TFT/LCD out. Backed in production by pigpio
// (MCP23017 for buttons/LEDs, ST7735 for the TFT, PCF8574 backpack for the LCD).
class HardwareIO {
public:
    class Listener {
    public:
        // Debounced edges. Timing (e.g. Eject's long press) is decided by
        // PlayerController, not the hardware layer.
        virtual void onButtonPressed(Button button) = 0;
        virtual void onButtonReleased(Button button) = 0;

    protected:
        ~Listener() = default;
    };

    virtual ~HardwareIO() = default;

    virtual void setListener(Listener* listener) = 0;

    virtual void setLed(Led led, bool on) = 0;
    // The LCD's single visible row. Scrolling text too long to fit is the LCD
    // view's job.
    virtual void showLcdText(const std::string& text) = 0;
    virtual void showScreen(const Screen& screen) = 0;
};

}  // namespace winampdeck
