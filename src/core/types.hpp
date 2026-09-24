#pragma once

#include <string>

namespace winampdeck {

// Which single Engine is currently audible (see CONTEXT.md).
enum class Source { Stopped, Spotify, Radio };

// The 8 functional panel buttons. Everything else on the skin is decorative.
enum class Button { Previous, Stop, Pause, Play, Next, Eject, Shuffle, Repeat };

// The 2 panel LEDs.
enum class Led { Shuffle, Repeat };

// One entry of the hand-edited stations.csv.
struct Station {
    std::string name;
    std::string url;
    std::string logo;  // Logo image filename, stored alongside stations.csv.

    bool operator==(const Station&) const = default;
};

}  // namespace winampdeck
