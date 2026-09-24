// Eject: a short press switches Source, a ~2s hold is a Safe Shutdown.

#include "player_controller_fixture.hpp"

namespace winampdeck::testing {
namespace {

using namespace std::chrono_literals;

using EjectTest = PlayerControllerTest;

TEST_F(EjectTest, ShortPressFromStoppedSwitchesToSpotify) {
    hw.tap(Button::Eject);

    EXPECT_EQ(hw.nowPlaying().source, Source::Spotify);
    EXPECT_EQ(system.shutdowns, 0);
}

TEST_F(EjectTest, ShortPressesToggleBetweenSpotifyAndRadio) {
    hw.tap(Button::Eject);
    hw.tap(Button::Eject);
    EXPECT_EQ(hw.nowPlaying().source, Source::Radio);

    hw.tap(Button::Eject);
    EXPECT_EQ(hw.nowPlaying().source, Source::Spotify);
}

TEST_F(EjectTest, ReleasingJustBeforeTheLongPressThresholdIsAShortPress) {
    hw.press(Button::Eject);
    scheduler.advance(PlayerController::kEjectLongPress - 1ms);
    hw.release(Button::Eject);

    EXPECT_EQ(hw.nowPlaying().source, Source::Spotify);
    EXPECT_EQ(system.shutdowns, 0);
    EXPECT_EQ(scheduler.pending(), 0u);
}

TEST_F(EjectTest, NothingHappensUntilReleaseOrTheThreshold) {
    hw.press(Button::Eject);
    scheduler.advance(1s);

    EXPECT_EQ(hw.nowPlaying().source, Source::Stopped);
    EXPECT_EQ(system.shutdowns, 0);
}

TEST_F(EjectTest, HoldingForTheThresholdTriggersSafeShutdownWhileStillHeld) {
    hw.press(Button::Eject);
    scheduler.advance(PlayerController::kEjectLongPress);

    EXPECT_EQ(system.shutdowns, 1);
    EXPECT_EQ(hw.lcd, "Shutting down");
}

TEST_F(EjectTest, ReleasingAfterALongPressDoesNotAlsoSwitchSource) {
    enterSpotify();

    hw.press(Button::Eject);
    scheduler.advance(PlayerController::kEjectLongPress);
    hw.release(Button::Eject);

    EXPECT_EQ(hw.nowPlaying().source, Source::Spotify);
    EXPECT_EQ(system.shutdowns, 1);
    EXPECT_TRUE(engine.commands.empty());
    EXPECT_TRUE(radio.commands.empty());
}

TEST_F(EjectTest, LongPressShutsDownFromEverySource) {
    for (const int shortPresses : {0, 1, 2}) {
        FakeEngineClient localEngine;
        FakeRadioClient localRadio;
        FakeHardwareIO localHw;
        ManualScheduler localScheduler;
        FakeSystemControl localSystem;
        PlayerController localController{localEngine, localRadio, localHw, localScheduler,
                                         localSystem, testStations(), [](std::size_t) { return 0u; }};
        localController.start();
        for (int i = 0; i < shortPresses; ++i) {
            localHw.tap(Button::Eject);
        }

        localHw.press(Button::Eject);
        localScheduler.advance(PlayerController::kEjectLongPress);

        EXPECT_EQ(localSystem.shutdowns, 1) << "after " << shortPresses << " short presses";
    }
}

TEST_F(EjectTest, LongPressShutsDownWithTheStationListOpen) {
    enterRadio();
    hw.tap(Button::Repeat);

    hw.press(Button::Eject);
    scheduler.advance(PlayerController::kEjectLongPress);

    EXPECT_EQ(system.shutdowns, 1);
}

TEST_F(EjectTest, EverythingIsIgnoredOnceShuttingDown) {
    enterRadio();
    hw.press(Button::Eject);
    scheduler.advance(PlayerController::kEjectLongPress);
    hw.release(Button::Eject);

    for (const Button button : {Button::Play, Button::Pause, Button::Next, Button::Previous,
                                Button::Shuffle, Button::Repeat, Button::Stop}) {
        hw.tap(button);
    }
    hw.tap(Button::Eject);
    hw.press(Button::Eject);
    scheduler.advance(PlayerController::kEjectLongPress);
    engine.emitStartedPlayingHere();

    EXPECT_TRUE(engine.commands.empty());
    EXPECT_TRUE(radio.commands.empty());
    EXPECT_EQ(system.shutdowns, 1);
    EXPECT_EQ(hw.nowPlaying().source, Source::Radio);
    EXPECT_EQ(hw.lcd, "Shutting down");
}

TEST_F(EjectTest, OtherButtonsStillWorkWhileEjectIsHeld) {
    enterSpotify();

    hw.press(Button::Eject);
    hw.tap(Button::Next);
    hw.release(Button::Eject);

    EXPECT_EQ(engine.commands, Commands{"next"});
    EXPECT_EQ(hw.nowPlaying().source, Source::Radio);
}

}  // namespace
}  // namespace winampdeck::testing
