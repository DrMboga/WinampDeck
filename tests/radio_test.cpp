// The Radio (Station List closed) column of the button map, and what the panel
// shows for Internet Radio.

#include "player_controller_fixture.hpp"

namespace winampdeck::testing {
namespace {

using RadioTest = PlayerControllerTest;

TEST_F(RadioTest, PlayAndPauseControlTheTunedStation) {
    enterRadio();

    hw.tap(Button::Pause);
    EXPECT_EQ(hw.nowPlaying().status, PlaybackStatus::Paused);

    hw.tap(Button::Play);
    EXPECT_EQ(hw.nowPlaying().status, PlaybackStatus::Playing);

    EXPECT_EQ(radio.commands, (Commands{"pause", "resume"}));
    EXPECT_TRUE(engine.commands.empty());
}

TEST_F(RadioTest, NextAndPreviousStepThroughStationsWrappingAround) {
    enterRadio();

    hw.tap(Button::Next);
    hw.tap(Button::Next);
    hw.tap(Button::Previous);
    hw.tap(Button::Previous);
    hw.tap(Button::Previous);

    EXPECT_EQ(radio.commands,
              (Commands{"tune Beta", "tune Gamma", "tune Beta", "tune Alpha", "tune Delta"}));
    EXPECT_EQ(hw.nowPlaying().station, "Delta");
}

TEST_F(RadioTest, SteppingWhilePausedStartsTheNewStationPlaying) {
    enterRadio();
    hw.tap(Button::Pause);

    hw.tap(Button::Next);

    EXPECT_EQ(hw.nowPlaying().status, PlaybackStatus::Playing);
}

TEST_F(RadioTest, ShuffleTunesARandomStationOtherThanTheCurrentOne) {
    enterRadio();  // On Alpha (index 0).
    randomPicks = {0, 2};

    hw.tap(Button::Shuffle);  // Pick 0 of the 3 others: Beta.
    hw.tap(Button::Shuffle);  // Pick 2 of the 3 others (Alpha, Gamma, Delta): Delta.

    EXPECT_EQ(randomCounts, (std::vector<std::size_t>{3, 3}));
    EXPECT_EQ(radio.commands, (Commands{"tune Beta", "tune Delta"}));
}

TEST_F(RadioTest, ShuffleLedStaysOff) {
    enterRadio();
    randomPicks = {1};

    hw.tap(Button::Shuffle);

    EXPECT_FALSE(hw.led(Led::Shuffle));
}

TEST_F(RadioTest, ShuffleWithASingleStationDoesNothing) {
    FakeEngineClient localEngine;
    FakeRadioClient localRadio;
    FakeHardwareIO localHw;
    ManualScheduler localScheduler;
    FakeSystemControl localSystem;
    bool asked = false;
    PlayerController localController{localEngine, localRadio, localHw, localScheduler, localSystem,
                                     {{"Solo", "http://solo.example/stream", "solo.png"}},
                                     [&](std::size_t) {
                                         asked = true;
                                         return 0u;
                                     }};
    localController.start();
    localHw.tap(Button::Eject);
    localHw.tap(Button::Eject);
    localRadio.commands.clear();

    localHw.tap(Button::Shuffle);

    EXPECT_FALSE(asked);
    EXPECT_TRUE(localRadio.commands.empty());
}

TEST_F(RadioTest, StopDoesNothing) {
    enterRadio();

    hw.tap(Button::Stop);

    EXPECT_TRUE(radio.commands.empty());
    EXPECT_TRUE(engine.commands.empty());
}

TEST_F(RadioTest, LcdShowsStationAndStreamTitle) {
    enterRadio();
    EXPECT_EQ(hw.lcd, "Alpha");

    radio.emitStreamTitle("Boards of Canada - Roygbiv");

    EXPECT_EQ(hw.lcd, "Alpha — Boards of Canada - Roygbiv");
}

TEST_F(RadioTest, TuningClearsTheOldStreamTitle) {
    enterRadio();
    radio.emitStreamTitle("Old song");

    hw.tap(Button::Next);

    EXPECT_EQ(hw.lcd, "Beta");
    EXPECT_EQ(hw.nowPlaying().title, "");
}

TEST_F(RadioTest, ScreenShowsTheStationWithItsLogo) {
    enterRadio();
    radio.emitStreamTitle("Some song");

    NowPlayingScreen expected;
    expected.source = Source::Radio;
    expected.status = PlaybackStatus::Playing;
    expected.station = "Alpha";
    expected.logo = "alpha.png";
    expected.title = "Some song";
    EXPECT_EQ(hw.nowPlaying(), expected);
}

}  // namespace
}  // namespace winampdeck::testing
