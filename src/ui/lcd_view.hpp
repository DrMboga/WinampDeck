#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>

#include "core/scheduler.hpp"
#include "ui/lcd_display.hpp"

namespace winampdeck::ui {

// Shows PlayerController's LCD text on the LCD's one visible row. Text that
// fits is shown as it is. Longer text scrolls Winamp style, like the TFT's:
// after a pause it moves one character per step, with `  ***  ` before it
// comes round again.
//
// Only rows that differ from what's already on the glass are written.
// Must be driven from the event loop's thread, like everything on Scheduler.
class LcdView {
public:
    static constexpr std::chrono::milliseconds kScrollDelay{1500};
    static constexpr std::chrono::milliseconds kDefaultScrollStep{300};

    LcdView(LcdDisplay& display, Scheduler& scheduler,
            std::chrono::milliseconds scrollStep = kDefaultScrollStep);
    ~LcdView();

    LcdView(const LcdView&) = delete;
    LcdView& operator=(const LcdView&) = delete;

    // Showing the text already shown changes nothing, so a scroll in
    // progress carries on.
    void show(const std::string& text);

private:
    void render();
    void onStep();
    void cancelTimer();

    LcdDisplay& display_;
    Scheduler& scheduler_;
    std::chrono::milliseconds scrollStep_;

    std::optional<std::string> text_;  // As given to show().
    std::string codes_;                // The text in ROM codes.
    std::size_t offset_ = 0;           // Into the scroll cycle.
    std::optional<std::string> shown_;
    std::optional<Scheduler::TimerId> timer_;
};

}  // namespace winampdeck::ui
