// Switching Source mutes the previously-audible Engine and unmutes the new one;
// neither Engine is ever stopped.

#include "player_controller_fixture.hpp"

namespace winampdeck::testing {
namespace {

using SourceSwitchingTest = PlayerControllerTest;

TEST_F(SourceSwitchingTest, StartsStoppedWithRadioReleasingTheAudioDevice) {
    EXPECT_EQ(radio.commands, Commands{"mute"});
    EXPECT_TRUE(engine.commands.empty());
    EXPECT_EQ(hw.nowPlaying(), NowPlayingScreen{});
    EXPECT_EQ(hw.lcd, "");
    EXPECT_FALSE(hw.led(Led::Shuffle));
    EXPECT_FALSE(hw.led(Led::Repeat));
}

TEST_F(SourceSwitchingTest, EveryButtonButEjectIsIgnoredWhileStopped) {
    clearCommands();

    for (const Button button : {Button::Play, Button::Pause, Button::Next, Button::Previous,
                                Button::Shuffle, Button::Repeat, Button::Stop}) {
        hw.tap(button);
    }

    EXPECT_TRUE(engine.commands.empty());
    EXPECT_TRUE(radio.commands.empty());
    EXPECT_EQ(hw.nowPlaying().source, Source::Stopped);
}

TEST_F(SourceSwitchingTest, StoppedToSpotifyDoesNotStartPlayback) {
    clearCommands();

    hw.tap(Button::Eject);

    EXPECT_TRUE(engine.commands.empty());
    EXPECT_TRUE(radio.commands.empty());
    EXPECT_EQ(hw.lcd, "Spotify");
}

TEST_F(SourceSwitchingTest, FirstSwitchToRadioUnmutesAndTunesTheFirstStation) {
    enterSpotify();

    hw.tap(Button::Eject);

    EXPECT_EQ(radio.commands, (Commands{"unmute", "tune Alpha"}));
    EXPECT_EQ(hw.nowPlaying().station, "Alpha");
    EXPECT_EQ(hw.nowPlaying().status, PlaybackStatus::Playing);
}

TEST_F(SourceSwitchingTest, SwitchingAwayFromPlayingSpotifyPausesIt) {
    enterSpotify();
    engine.emitStartedPlayingHere();

    hw.tap(Button::Eject);

    EXPECT_EQ(engine.commands, Commands{"pause"});
}

TEST_F(SourceSwitchingTest, SwitchingAwayFromPausedSpotifySendsItNothing) {
    enterSpotify();
    engine.emitActive(true);

    hw.tap(Button::Eject);

    EXPECT_TRUE(engine.commands.empty());
}

TEST_F(SourceSwitchingTest, SwitchingBackToSpotifyResumesItIfItWasPlaying) {
    enterSpotify();
    engine.emitStartedPlayingHere();
    hw.tap(Button::Eject);
    engine.emitPlaying(false);  // go-librespot confirming the pause.
    clearCommands();

    hw.tap(Button::Eject);

    EXPECT_EQ(radio.commands, Commands{"mute"});
    EXPECT_EQ(engine.commands, Commands{"play"});
    EXPECT_EQ(hw.nowPlaying().source, Source::Spotify);
}

TEST_F(SourceSwitchingTest, SwitchingBackToSpotifyLeavesItPausedIfItWasPaused) {
    enterSpotify();
    engine.emitActive(true);
    hw.tap(Button::Eject);
    clearCommands();

    hw.tap(Button::Eject);

    EXPECT_TRUE(engine.commands.empty());
}

TEST_F(SourceSwitchingTest, SwitchingBackToSpotifyDoesNotResumeASessionThatMovedAway) {
    enterSpotify();
    engine.emitStartedPlayingHere();
    hw.tap(Button::Eject);
    engine.emitPlaying(false);
    engine.emitActive(false);  // Playback moved to another device meanwhile.
    clearCommands();

    hw.tap(Button::Eject);

    EXPECT_TRUE(engine.commands.empty());
}

TEST_F(SourceSwitchingTest, ReturningToRadioKeepsTheTunedStationAndPauseState) {
    enterRadio();
    hw.tap(Button::Next);
    hw.tap(Button::Pause);
    hw.tap(Button::Eject);
    clearCommands();

    hw.tap(Button::Eject);

    EXPECT_EQ(radio.commands, Commands{"unmute"});
    EXPECT_EQ(hw.nowPlaying().station, "Beta");
    EXPECT_EQ(hw.nowPlaying().status, PlaybackStatus::Paused);
}

TEST_F(SourceSwitchingTest, RadioIsMutedNotPausedWhenSwitchingAway) {
    enterRadio();

    hw.tap(Button::Eject);

    EXPECT_EQ(radio.commands, Commands{"mute"});
}

TEST_F(SourceSwitchingTest, RadioWithNoStationsIsAnInertSource) {
    FakeEngineClient localEngine;
    FakeRadioClient localRadio;
    FakeHardwareIO localHw;
    ManualScheduler localScheduler;
    FakeSystemControl localSystem;
    PlayerController localController{localEngine, localRadio, localHw, localScheduler, localSystem,
                                     {}, [](std::size_t) { return 0u; }};
    localController.start();
    localHw.tap(Button::Eject);
    localHw.tap(Button::Eject);
    localRadio.commands.clear();

    for (const Button button : {Button::Play, Button::Pause, Button::Next, Button::Previous,
                                Button::Shuffle, Button::Repeat, Button::Stop}) {
        localHw.tap(button);
    }

    EXPECT_TRUE(localRadio.commands.empty());
    EXPECT_FALSE(localHw.showsStationList());
    EXPECT_EQ(localHw.nowPlaying().source, Source::Radio);
    EXPECT_EQ(localHw.lcd, "Internet Radio");
}

}  // namespace
}  // namespace winampdeck::testing
