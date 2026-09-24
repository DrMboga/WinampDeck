// Auto-switch: Spotify starting to play on this Deck (e.g. Play pressed in a
// phone's Spotify app with the Deck picked) makes Spotify the Source; Spotify
// activity anywhere else on the account never does.

#include "player_controller_fixture.hpp"

namespace winampdeck::testing {
namespace {

using AutoSwitchTest = PlayerControllerTest;

TEST_F(AutoSwitchTest, PlaybackStartingHereTakesOverFromRadio) {
    enterRadio();

    engine.emitStartedPlayingHere();

    EXPECT_EQ(hw.nowPlaying().source, Source::Spotify);
    EXPECT_EQ(radio.commands, Commands{"mute"});
    // Spotify is already playing; the controller must not poke it.
    EXPECT_TRUE(engine.commands.empty());
}

TEST_F(AutoSwitchTest, PlaybackStartingHereTakesOverFromStopped) {
    clearCommands();

    engine.emitStartedPlayingHere();

    EXPECT_EQ(hw.nowPlaying().source, Source::Spotify);
    EXPECT_TRUE(engine.commands.empty());
    EXPECT_TRUE(radio.commands.empty());
}

TEST_F(AutoSwitchTest, WorksWhicheverOrderActiveAndPlayingArriveIn) {
    enterRadio();

    engine.emitPlaying(true);
    EXPECT_EQ(hw.nowPlaying().source, Source::Radio);

    engine.emitActive(true);
    EXPECT_EQ(hw.nowPlaying().source, Source::Spotify);
}

TEST_F(AutoSwitchTest, PlaybackOnAnotherDeviceNeverSwitches) {
    enterRadio();

    // The account is playing somewhere, but this Deck isn't the active device.
    engine.emitActive(false);
    engine.emitPlaying(true);

    EXPECT_EQ(hw.nowPlaying().source, Source::Radio);
    EXPECT_TRUE(radio.commands.empty());
}

TEST_F(AutoSwitchTest, BeingPickedWithoutPlayingDoesNotSwitch) {
    enterRadio();

    engine.emitActive(true);

    EXPECT_EQ(hw.nowPlaying().source, Source::Radio);
}

TEST_F(AutoSwitchTest, PlaybackMovingAwayFromTheDeckDoesNotChangeSource) {
    enterSpotify();
    engine.emitStartedPlayingHere();

    engine.emitActive(false);

    EXPECT_EQ(hw.nowPlaying().source, Source::Spotify);
    EXPECT_EQ(hw.nowPlaying().status, PlaybackStatus::Stopped);
}

TEST_F(AutoSwitchTest, ResumingAfterSwitchingAwayTakesOverAgain) {
    enterSpotify();
    engine.emitStartedPlayingHere();
    hw.tap(Button::Eject);
    engine.emitPlaying(false);  // go-librespot confirming Eject's pause.
    clearCommands();

    engine.emitPlaying(true);  // Play pressed on the phone.

    EXPECT_EQ(hw.nowPlaying().source, Source::Spotify);
    EXPECT_EQ(radio.commands, Commands{"mute"});
    EXPECT_TRUE(engine.commands.empty());
}

TEST_F(AutoSwitchTest, ALatePlayingEventAfterEjectDoesNotBounceBack) {
    enterSpotify();
    engine.emitStartedPlayingHere();
    hw.tap(Button::Eject);

    // Emitted before go-librespot processed Eject's pause, delivered after.
    engine.emitPlaying(true);

    EXPECT_EQ(hw.nowPlaying().source, Source::Radio);
}

TEST_F(AutoSwitchTest, TrackChangesWhileSpotifyIsTheSourceDoNothingExtra) {
    enterSpotify();
    engine.emitStartedPlayingHere();

    engine.emitPlaying(false);
    engine.emitTrack({"A", "Next one", "", {}});
    engine.emitPlaying(true);

    EXPECT_TRUE(engine.commands.empty());
    EXPECT_TRUE(radio.commands.empty());
}

}  // namespace
}  // namespace winampdeck::testing
