#include "ui/lcd_view.hpp"

#include <string_view>
#include <utility>

#include "ui/lcd_text.hpp"

namespace winampdeck::ui {

namespace {

constexpr auto kColumns = static_cast<std::size_t>(LcdDisplay::kColumns);
// Winamp's marquee: the text, a separator, then the text again.
constexpr std::string_view kSeparator = "  ***  ";

}  // namespace

LcdView::LcdView(LcdDisplay& display, Scheduler& scheduler, std::chrono::milliseconds scrollStep)
    : display_(display), scheduler_(scheduler), scrollStep_(scrollStep) {}

LcdView::~LcdView() {
    cancelTimer();
}

void LcdView::show(const std::string& text) {
    if (text_ == text) {
        return;
    }
    text_ = text;
    codes_ = encodeLcdText(text);
    offset_ = 0;
    cancelTimer();
    render();
    if (codes_.size() > kColumns) {
        timer_ = scheduler_.callAfter(kScrollDelay, [this] { onStep(); });
    }
}

void LcdView::render() {
    std::string row;
    if (codes_.size() <= kColumns) {
        row = codes_;
        row.resize(kColumns, ' ');
    } else {
        const std::string cycle = codes_ + std::string(kSeparator);
        row.reserve(kColumns);
        for (std::size_t i = 0; i < kColumns; ++i) {
            row += cycle[(offset_ + i) % cycle.size()];
        }
    }
    if (shown_ != row) {
        display_.writeRow(row);
        shown_ = std::move(row);
    }
}

void LcdView::onStep() {
    timer_.reset();
    offset_ = (offset_ + 1) % (codes_.size() + kSeparator.size());
    render();
    timer_ = scheduler_.callAfter(scrollStep_, [this] { onStep(); });
}

void LcdView::cancelTimer() {
    if (timer_) {
        scheduler_.cancel(*timer_);
        timer_.reset();
    }
}

}  // namespace winampdeck::ui
