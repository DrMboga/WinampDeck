// The Last Station: the first switch to Radio tunes it, and Safe Shutdown
// saves the tuned Station (ADR 0007).

#include <vector>

#include "player_controller_fixture.hpp"

namespace winampdeck::testing {
namespace {

using LastStationTest = PlayerControllerTest;

using Saves = std::vector<std::size_t>;

void holdEject(FakeHardwareIO& hw, ManualScheduler& scheduler) {
    hw.press(Button::Eject);
    scheduler.advance(PlayerController::kEjectLongPress);
}

TEST_F(LastStationTest, NothingRememberedTunesTheFirstStation) {
    hw.tap(Button::Eject);
    hw.tap(Button::Eject);

    EXPECT_EQ(radio.commands, (Commands{"mute", "unmute", "tune Alpha"}));
}

TEST_F(LastStationTest, FirstSwitchToRadioTunesTheRememberedStation) {
    memory.remembered = 2;

    hw.tap(Button::Eject);
    hw.tap(Button::Eject);

    EXPECT_EQ(radio.commands, (Commands{"mute", "unmute", "tune Gamma"}));
    EXPECT_EQ(hw.nowPlaying().station, "Gamma");
}

TEST_F(LastStationTest, TheLastPositionInTheListIsValid) {
    memory.remembered = 3;
    enterRadio();

    EXPECT_EQ(hw.nowPlaying().station, "Delta");
}

TEST_F(LastStationTest, APositionPastTheEndTunesTheFirstStation) {
    for (const std::size_t position : {std::size_t{4}, std::size_t{1000}}) {
        FakeEngineClient localEngine;
        FakeRadioClient localRadio;
        FakeHardwareIO localHw;
        ManualScheduler localScheduler;
        FakeSystemControl localSystem;
        FakeStationMemory localMemory;
        localMemory.remembered = position;
        PlayerController localController{localEngine, localRadio, localHw, localScheduler, localSystem,
                                         localMemory, testStations(), [](std::size_t) { return 0u; }};
        localController.start();

        localHw.tap(Button::Eject);
        localHw.tap(Button::Eject);

        EXPECT_EQ(localRadio.commands, (Commands{"mute", "unmute", "tune Alpha"})) << "position " << position;
    }
}

TEST_F(LastStationTest, StillStartsStoppedAndTheFirstEjectStillGoesToSpotify) {
    memory.remembered = 2;

    EXPECT_EQ(hw.nowPlaying().source, Source::Stopped);
    EXPECT_EQ(radio.commands, Commands{"mute"});

    hw.tap(Button::Eject);

    EXPECT_EQ(hw.nowPlaying().source, Source::Spotify);
    EXPECT_EQ(radio.commands, Commands{"mute"});
}

TEST_F(LastStationTest, TheRememberedStationIsUsedOnlyOnce) {
    memory.remembered = 2;
    enterRadio();
    hw.tap(Button::Next);  // Delta
    hw.tap(Button::Eject);
    clearCommands();

    hw.tap(Button::Eject);

    EXPECT_EQ(radio.commands, Commands{"unmute"});
    EXPECT_EQ(hw.nowPlaying().station, "Delta");
}

TEST_F(LastStationTest, StepsAndTheStationListStartFromTheRememberedStation) {
    memory.remembered = 2;
    enterRadio();

    hw.tap(Button::Repeat);
    EXPECT_EQ(hw.stationList().selected, 2u);
    EXPECT_EQ(hw.stationList().tuned, 2u);
    hw.tap(Button::Repeat);

    hw.tap(Button::Next);
    EXPECT_EQ(radio.commands, Commands{"tune Delta"});
}

TEST_F(LastStationTest, SafeShutdownSavesTheTunedStation) {
    enterRadio();
    hw.tap(Button::Next);
    hw.tap(Button::Next);  // Gamma

    holdEject(hw, scheduler);

    EXPECT_EQ(memory.saves, Saves{2});
    EXPECT_EQ(system.shutdowns, 1);
}

TEST_F(LastStationTest, SafeShutdownSavesTheRememberedStationOnceTuned) {
    memory.remembered = 3;
    enterRadio();

    holdEject(hw, scheduler);

    EXPECT_EQ(memory.saves, Saves{3});
}

TEST_F(LastStationTest, SafeShutdownFromSpotifySavesTheStationTunedEarlier) {
    enterRadio();
    hw.tap(Button::Previous);  // Delta
    hw.tap(Button::Eject);     // Spotify

    holdEject(hw, scheduler);

    EXPECT_EQ(memory.saves, Saves{3});
}

TEST_F(LastStationTest, SafeShutdownWithNoStationTunedThisRunSavesNothing) {
    memory.remembered = 2;
    enterSpotify();

    holdEject(hw, scheduler);

    EXPECT_TRUE(memory.saves.empty());
    EXPECT_EQ(system.shutdowns, 1);
}

TEST_F(LastStationTest, SafeShutdownFromStoppedSavesNothing) {
    holdEject(hw, scheduler);

    EXPECT_TRUE(memory.saves.empty());
    EXPECT_EQ(system.shutdowns, 1);
}

TEST_F(LastStationTest, NothingIsSavedExceptAtSafeShutdown) {
    enterRadio();
    hw.tap(Button::Next);
    hw.tap(Button::Repeat);
    hw.tap(Button::Next);
    hw.tap(Button::Play);
    randomPicks.push_back(0);
    hw.tap(Button::Shuffle);
    hw.tap(Button::Eject);
    hw.tap(Button::Eject);
    engine.emitStartedPlayingHere();

    EXPECT_TRUE(memory.saves.empty());
}

// Records, at the moment of the save, what the panel and the OS had been told.
class ObservedStationMemory final : public StationMemory {
public:
    ObservedStationMemory(const FakeHardwareIO& hw, const FakeSystemControl& system) : hw_(hw), system_(system) {}

    std::optional<std::size_t> rememberedStation() override { return std::nullopt; }
    void rememberStation(std::size_t /*station*/) override {
        lcdAtSave = hw_.lcd;
        shutdownsAtSave = system_.shutdowns;
    }

    std::string lcdAtSave;
    int shutdownsAtSave = -1;

private:
    const FakeHardwareIO& hw_;
    const FakeSystemControl& system_;
};

TEST_F(LastStationTest, TheSaveComesAfterShuttingDownIsShownAndBeforeThePoweroff) {
    FakeEngineClient localEngine;
    FakeRadioClient localRadio;
    FakeHardwareIO localHw;
    ManualScheduler localScheduler;
    FakeSystemControl localSystem;
    ObservedStationMemory localMemory{localHw, localSystem};
    PlayerController localController{localEngine, localRadio, localHw, localScheduler, localSystem,
                                     localMemory, testStations(), [](std::size_t) { return 0u; }};
    localController.start();
    localHw.tap(Button::Eject);
    localHw.tap(Button::Eject);

    holdEject(localHw, localScheduler);

    EXPECT_EQ(localMemory.lcdAtSave, "Shutting down");
    EXPECT_EQ(localMemory.shutdownsAtSave, 0);
    EXPECT_EQ(localSystem.shutdowns, 1);
}

TEST_F(LastStationTest, WithNoStationsNothingIsTunedOrSaved) {
    FakeEngineClient localEngine;
    FakeRadioClient localRadio;
    FakeHardwareIO localHw;
    ManualScheduler localScheduler;
    FakeSystemControl localSystem;
    FakeStationMemory localMemory;
    localMemory.remembered = 0;
    PlayerController localController{localEngine, localRadio, localHw, localScheduler, localSystem,
                                     localMemory, {}, [](std::size_t) { return 0u; }};
    localController.start();
    localHw.tap(Button::Eject);
    localHw.tap(Button::Eject);

    holdEject(localHw, localScheduler);

    EXPECT_EQ(localRadio.commands, (Commands{"mute", "unmute"}));
    EXPECT_TRUE(localMemory.saves.empty());
    EXPECT_EQ(localSystem.shutdowns, 1);
}

}  // namespace
}  // namespace winampdeck::testing
