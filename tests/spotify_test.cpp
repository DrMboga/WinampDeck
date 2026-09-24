// The Spotify column of the button map, and what the panel shows for Spotify.

#include "player_controller_fixture.hpp"

namespace winampdeck::testing {
namespace {

using namespace std::chrono_literals;

using SpotifyTest = PlayerControllerTest;

TEST_F(SpotifyTest, TransportButtonsDriveTheEngine) {
    enterSpotify();

    hw.tap(Button::Play);
    hw.tap(Button::Pause);
    hw.tap(Button::Next);
    hw.tap(Button::Previous);

    EXPECT_EQ(engine.commands, (Commands{"play", "pause", "next", "previous"}));
    EXPECT_TRUE(radio.commands.empty());
}

TEST_F(SpotifyTest, StopDoesNothing) {
    enterSpotify();

    hw.tap(Button::Stop);

    EXPECT_TRUE(engine.commands.empty());
    EXPECT_TRUE(radio.commands.empty());
}

TEST_F(SpotifyTest, ShuffleTogglesTheEnginesReportedState) {
    enterSpotify();

    hw.tap(Button::Shuffle);
    engine.emitShuffle(true);
    hw.tap(Button::Shuffle);

    EXPECT_EQ(engine.commands, (Commands{"shuffle on", "shuffle off"}));
}

TEST_F(SpotifyTest, RepeatTogglesTheEnginesReportedState) {
    enterSpotify();

    hw.tap(Button::Repeat);
    engine.emitRepeat(true);
    hw.tap(Button::Repeat);

    EXPECT_EQ(engine.commands, (Commands{"repeat on", "repeat off"}));
}

TEST_F(SpotifyTest, LedsMirrorTheEngineNotTheButton) {
    enterSpotify();

    hw.tap(Button::Shuffle);
    hw.tap(Button::Repeat);
    EXPECT_FALSE(hw.led(Led::Shuffle));
    EXPECT_FALSE(hw.led(Led::Repeat));

    engine.emitShuffle(true);
    engine.emitRepeat(true);
    EXPECT_TRUE(hw.led(Led::Shuffle));
    EXPECT_TRUE(hw.led(Led::Repeat));

    // Changed from a phone, not the panel.
    engine.emitShuffle(false);
    EXPECT_FALSE(hw.led(Led::Shuffle));
    EXPECT_TRUE(hw.led(Led::Repeat));
}

TEST_F(SpotifyTest, LedsGoDarkOnRadioAndComeBackWithSpotify) {
    enterSpotify();
    engine.emitShuffle(true);
    engine.emitRepeat(true);

    hw.tap(Button::Eject);
    EXPECT_FALSE(hw.led(Led::Shuffle));
    EXPECT_FALSE(hw.led(Led::Repeat));

    hw.tap(Button::Eject);
    EXPECT_TRUE(hw.led(Led::Shuffle));
    EXPECT_TRUE(hw.led(Led::Repeat));
}

TEST_F(SpotifyTest, LedsTrackTheEngineEvenWhileRadioIsTheSource) {
    enterRadio();
    engine.emitShuffle(true);
    EXPECT_FALSE(hw.led(Led::Shuffle));

    hw.tap(Button::Eject);

    EXPECT_TRUE(hw.led(Led::Shuffle));
}

TEST_F(SpotifyTest, LcdShowsArtistAndTrack) {
    enterSpotify();

    engine.emitStartedPlayingHere();
    engine.emitTrack({"Nullsleep", "Silicon Lust", "Electric Heart", 180s});

    EXPECT_EQ(hw.lcd, "Nullsleep — Silicon Lust");
}

TEST_F(SpotifyTest, LcdShowsJustTheTitleWhenThereIsNoArtist) {
    enterSpotify();

    engine.emitTrack({"", "Episode 12", "", 3600s});

    EXPECT_EQ(hw.lcd, "Episode 12");
}

TEST_F(SpotifyTest, LcdFallsBackToSpotifyWhenNothingIsLoaded) {
    enterSpotify();
    engine.emitStartedPlayingHere();
    engine.emitTrack({"Nullsleep", "Silicon Lust", "Electric Heart", 180s});

    engine.emitActive(false);

    EXPECT_EQ(hw.lcd, "Spotify");
}

TEST_F(SpotifyTest, ScreenShowsTrackProgressAndStatus) {
    enterSpotify();
    engine.emitStartedPlayingHere();
    engine.emitTrack({"Nullsleep", "Silicon Lust", "Electric Heart", 180s});
    engine.emitPosition(42s);

    NowPlayingScreen expected;
    expected.source = Source::Spotify;
    expected.status = PlaybackStatus::Playing;
    expected.artist = "Nullsleep";
    expected.title = "Silicon Lust";
    expected.position = 42s;
    expected.duration = 180s;
    EXPECT_EQ(hw.nowPlaying(), expected);

    engine.emitPlaying(false);
    EXPECT_EQ(hw.nowPlaying().status, PlaybackStatus::Paused);
}

TEST_F(SpotifyTest, ANewTrackStartsFromZero) {
    enterSpotify();
    engine.emitStartedPlayingHere();
    engine.emitTrack({"A", "One", "", 100s});
    engine.emitPosition(50s);

    engine.emitTrack({"B", "Two", "", 200s});

    EXPECT_EQ(hw.nowPlaying().position, 0s);
    EXPECT_EQ(hw.nowPlaying().duration, 200s);
}

TEST_F(SpotifyTest, ScreenShowsStoppedWhenNoSessionIsActive) {
    enterSpotify();

    EXPECT_EQ(hw.nowPlaying().status, PlaybackStatus::Stopped);
}

}  // namespace
}  // namespace winampdeck::testing
