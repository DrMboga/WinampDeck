#pragma once

#include <string_view>

namespace winampdeck::ui {

// The LCD's glass: the HD44780's first row, the only one the panel's slot
// shows. Backed in production by the HD44780 behind its PCF8574 backpack.
class LcdDisplay {
public:
    static constexpr int kColumns = 16;

    virtual ~LcdDisplay() = default;

    // Replaces the row with `codes`: kColumns character codes from the
    // controller's character ROM (see encodeLcdText).
    virtual void writeRow(std::string_view codes) = 0;
};

}  // namespace winampdeck::ui
