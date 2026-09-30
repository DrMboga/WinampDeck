#pragma once

#include <string>
#include <string_view>

namespace winampdeck {

// Which single Engine is currently audible (see CONTEXT.md).
enum class Source { Stopped, Spotify, Radio };

// The 8 functional panel buttons. Everything else on the skin is decorative.
enum class Button { Previous, Stop, Pause, Play, Next, Eject, Shuffle, Repeat };

// The 2 panel LEDs.
enum class Led { Shuffle, Repeat };

constexpr std::string_view toString(Button button) {
    switch (button) {
    case Button::Previous: return "Previous";
    case Button::Stop: return "Stop";
    case Button::Pause: return "Pause";
    case Button::Play: return "Play";
    case Button::Next: return "Next";
    case Button::Eject: return "Eject";
    case Button::Shuffle: return "Shuffle";
    case Button::Repeat: return "Repeat";
    }
    return "?";
}

constexpr std::string_view toString(Led led) {
    return led == Led::Shuffle ? "Shuffle" : "Repeat";
}

// One entry of the hand-edited stations.csv.
struct Station {
    std::string name;
    std::string url;
    std::string logo;  // Logo image filename, stored alongside stations.csv.

    bool operator==(const Station&) const = default;
};

}  // namespace winampdeck
