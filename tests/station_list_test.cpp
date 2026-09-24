// The Station List: opened and closed with Repeat while Internet Radio is the
// Source (the "Radio (Station List open)" column of the button map).

#include "player_controller_fixture.hpp"

namespace winampdeck::testing {
namespace {

using StationListTest = PlayerControllerTest;

TEST_F(StationListTest, RepeatOpensTheListAtTheTunedStationWithTheLedOn) {
    enterRadio();
    hw.tap(Button::Next);  // Beta.
    clearCommands();

    hw.tap(Button::Repeat);

    ASSERT_TRUE(hw.showsStationList());
    EXPECT_EQ(hw.stationList().stations, testStations());
    EXPECT_EQ(hw.stationList().selected, 1u);
    EXPECT_EQ(hw.stationList().tuned, 1u);
    EXPECT_TRUE(hw.led(Led::Repeat));
    EXPECT_TRUE(radio.commands.empty());
}

TEST_F(StationListTest, NextAndPreviousMoveTheSelectionWithoutTuning) {
    enterRadio();
    hw.tap(Button::Repeat);

    hw.tap(Button::Next);
    hw.tap(Button::Next);
    EXPECT_EQ(hw.stationList().selected, 2u);

    hw.tap(Button::Previous);
    EXPECT_EQ(hw.stationList().selected, 1u);

    EXPECT_EQ(hw.stationList().tuned, 0u);
    EXPECT_TRUE(radio.commands.empty());
}

TEST_F(StationListTest, SelectionWrapsAround) {
    enterRadio();
    hw.tap(Button::Repeat);

    hw.tap(Button::Previous);
    EXPECT_EQ(hw.stationList().selected, 3u);

    hw.tap(Button::Next);
    EXPECT_EQ(hw.stationList().selected, 0u);
}

TEST_F(StationListTest, PlayTunesTheSelectionAndClosesTheList) {
    enterRadio();
    hw.tap(Button::Repeat);
    hw.tap(Button::Next);
    hw.tap(Button::Next);

    hw.tap(Button::Play);

    EXPECT_EQ(radio.commands, Commands{"tune Gamma"});
    EXPECT_FALSE(hw.showsStationList());
    EXPECT_EQ(hw.nowPlaying().station, "Gamma");
    EXPECT_FALSE(hw.led(Led::Repeat));
}

TEST_F(StationListTest, PlayOnTheAlreadyTunedStationResumesWithoutRetuning) {
    enterRadio();
    hw.tap(Button::Pause);
    hw.tap(Button::Repeat);
    clearCommands();

    hw.tap(Button::Play);

    EXPECT_EQ(radio.commands, Commands{"resume"});
    EXPECT_FALSE(hw.showsStationList());
    EXPECT_EQ(hw.nowPlaying().status, PlaybackStatus::Playing);
}

TEST_F(StationListTest, RepeatCancelsWithoutChangingStation) {
    enterRadio();
    hw.tap(Button::Repeat);
    hw.tap(Button::Next);

    hw.tap(Button::Repeat);

    EXPECT_TRUE(radio.commands.empty());
    EXPECT_FALSE(hw.showsStationList());
    EXPECT_EQ(hw.nowPlaying().station, "Alpha");
    EXPECT_FALSE(hw.led(Led::Repeat));
}

TEST_F(StationListTest, PauseShuffleAndStopAreIgnored) {
    enterRadio();
    hw.tap(Button::Repeat);
    hw.tap(Button::Next);

    hw.tap(Button::Pause);
    hw.tap(Button::Shuffle);
    hw.tap(Button::Stop);

    EXPECT_TRUE(radio.commands.empty());
    EXPECT_TRUE(randomCounts.empty());
    ASSERT_TRUE(hw.showsStationList());
    EXPECT_EQ(hw.stationList().selected, 1u);
}

TEST_F(StationListTest, EjectClosesTheListAndSwitchesToSpotify) {
    enterRadio();
    hw.tap(Button::Repeat);
    hw.tap(Button::Next);

    hw.tap(Button::Eject);

    EXPECT_FALSE(hw.showsStationList());
    EXPECT_EQ(hw.nowPlaying().source, Source::Spotify);
    EXPECT_EQ(radio.commands, Commands{"mute"});
    EXPECT_FALSE(hw.led(Led::Repeat));
}

TEST_F(StationListTest, ReturningToRadioAfterEjectShowsNowPlayingNotTheList) {
    enterRadio();
    hw.tap(Button::Repeat);
    hw.tap(Button::Eject);

    hw.tap(Button::Eject);

    EXPECT_FALSE(hw.showsStationList());
    EXPECT_EQ(hw.nowPlaying().station, "Alpha");
}

TEST_F(StationListTest, ReopeningStartsFromTheTunedStationAgain) {
    enterRadio();
    hw.tap(Button::Repeat);
    hw.tap(Button::Next);
    hw.tap(Button::Next);
    hw.tap(Button::Repeat);  // Cancel.

    hw.tap(Button::Repeat);

    EXPECT_EQ(hw.stationList().selected, 0u);
}

TEST_F(StationListTest, AutoSwitchToSpotifyClosesTheList) {
    enterRadio();
    hw.tap(Button::Repeat);

    engine.emitStartedPlayingHere();

    EXPECT_FALSE(hw.showsStationList());
    EXPECT_FALSE(hw.led(Led::Repeat));
    EXPECT_EQ(hw.nowPlaying().source, Source::Spotify);
}

}  // namespace
}  // namespace winampdeck::testing
