// The LCD view: what reaches the row for each text, and how it scrolls.

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include "fakes.hpp"
#include "ui/lcd_view.hpp"

namespace winampdeck::ui {
namespace {

using namespace std::chrono_literals;
using winampdeck::testing::ManualScheduler;

class RecordingLcd final : public LcdDisplay {
public:
    std::vector<std::string> rows;

    void writeRow(std::string_view codes) override { rows.emplace_back(codes); }
};

class LcdViewTest : public ::testing::Test {
protected:
    static constexpr std::chrono::milliseconds kStep{300};

    RecordingLcd lcd;
    ManualScheduler scheduler;
    LcdView view{lcd, scheduler, kStep};
};

TEST_F(LcdViewTest, ShortTextIsPaddedAndDoesntScroll) {
    view.show("Spotify");
    ASSERT_EQ(lcd.rows.size(), 1u);
    EXPECT_EQ(lcd.rows[0], "Spotify         ");
    EXPECT_EQ(scheduler.pending(), 0u);
}

TEST_F(LcdViewTest, TextOfExactlyOneRowDoesntScroll) {
    view.show("0123456789abcdef");
    EXPECT_EQ(lcd.rows.back(), "0123456789abcdef");
    EXPECT_EQ(scheduler.pending(), 0u);
}

TEST_F(LcdViewTest, EmptyTextBlanksTheRow) {
    view.show("");
    ASSERT_EQ(lcd.rows.size(), 1u);
    EXPECT_EQ(lcd.rows[0], std::string(16, ' '));
}

TEST_F(LcdViewTest, ShowsTheRomCodes) {
    view.show("Кино — Звезда");
    EXPECT_EQ(lcd.rows.back(), "Kino - Zvezda   ");
}

TEST_F(LcdViewTest, LongTextWaitsThenScrollsOneCharacterPerStep) {
    view.show("Daft Punk — One More Time");
    ASSERT_EQ(lcd.rows.size(), 1u);
    EXPECT_EQ(lcd.rows[0], "Daft Punk - One ");

    scheduler.advance(LcdView::kScrollDelay - 1ms);
    EXPECT_EQ(lcd.rows.size(), 1u);
    scheduler.advance(1ms);
    ASSERT_EQ(lcd.rows.size(), 2u);
    EXPECT_EQ(lcd.rows[1], "aft Punk - One M");

    scheduler.advance(kStep);
    EXPECT_EQ(lcd.rows.back(), "ft Punk - One Mo");
}

TEST_F(LcdViewTest, ScrollingComesRoundWithTheSeparator) {
    view.show("Daft Punk - One More Time");  // 25 characters.
    scheduler.advance(LcdView::kScrollDelay);
    // Offset 1 now; 15 more steps reach offset 16.
    for (int i = 0; i < 15; ++i) {
        scheduler.advance(kStep);
    }
    EXPECT_EQ(lcd.rows.back(), "More Time  ***  ");

    // The cycle is 25 + 7 characters long: after it, the start again.
    for (int i = 0; i < 16; ++i) {
        scheduler.advance(kStep);
    }
    EXPECT_EQ(lcd.rows.back(), "Daft Punk - One ");
}

TEST_F(LcdViewTest, TheSameTextAgainDoesntRestartTheScroll) {
    view.show("Daft Punk — One More Time");
    scheduler.advance(LcdView::kScrollDelay);
    scheduler.advance(kStep);
    const auto writes = lcd.rows.size();
    view.show("Daft Punk — One More Time");
    EXPECT_EQ(lcd.rows.size(), writes);
    scheduler.advance(kStep);
    EXPECT_EQ(lcd.rows.back(), "t Punk - One Mor");
}

TEST_F(LcdViewTest, NewTextStartsFromTheBeginning) {
    view.show("Daft Punk — One More Time");
    scheduler.advance(LcdView::kScrollDelay);
    scheduler.advance(kStep);
    view.show("Radiohead — Paranoid Android");
    EXPECT_EQ(lcd.rows.back(), "Radiohead - Para");

    // And waits again before scrolling.
    scheduler.advance(LcdView::kScrollDelay - 1ms);
    EXPECT_EQ(lcd.rows.back(), "Radiohead - Para");
    scheduler.advance(1ms);
    EXPECT_EQ(lcd.rows.back(), "adiohead - Paran");
}

TEST_F(LcdViewTest, ShortTextStopsTheScroll) {
    view.show("Daft Punk — One More Time");
    scheduler.advance(LcdView::kScrollDelay);
    view.show("Shutting down");
    EXPECT_EQ(lcd.rows.back(), "Shutting down   ");
    EXPECT_EQ(scheduler.pending(), 0u);
}

TEST_F(LcdViewTest, UnchangedRowsArentWrittenAgain) {
    view.show("Spotify");
    view.show("Spotify ");  // Different text, same row.
    EXPECT_EQ(lcd.rows.size(), 1u);
}

TEST(LcdView, UsesTheScrollStepItsGiven) {
    RecordingLcd lcd;
    ManualScheduler scheduler;
    LcdView view(lcd, scheduler, 100ms);
    view.show("Daft Punk — One More Time");
    scheduler.advance(LcdView::kScrollDelay);
    scheduler.advance(100ms);
    EXPECT_EQ(lcd.rows.back(), "ft Punk - One Mo");
}

TEST(LcdView, CancelsItsTimerWhenDestroyed) {
    RecordingLcd lcd;
    ManualScheduler scheduler;
    {
        LcdView view(lcd, scheduler);
        view.show("Daft Punk — One More Time");
        EXPECT_EQ(scheduler.pending(), 1u);
    }
    EXPECT_EQ(scheduler.pending(), 0u);
}

}  // namespace
}  // namespace winampdeck::ui
