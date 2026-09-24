#pragma once

#include <gtest/gtest.h>

#include <cstddef>
#include <deque>
#include <string>
#include <vector>

#include "core/player_controller.hpp"
#include "fakes.hpp"

namespace winampdeck::testing {

using Commands = std::vector<std::string>;

inline std::vector<Station> testStations() {
    return {
        {"Alpha", "http://alpha.example/stream", "alpha.png"},
        {"Beta", "http://beta.example/stream", "beta.png"},
        {"Gamma", "http://gamma.example/stream", "gamma.png"},
        {"Delta", "http://delta.example/stream", "delta.png"},
    };
}

// A started PlayerController wired to fakes, with four Stations.
class PlayerControllerTest : public ::testing::Test {
protected:
    PlayerControllerTest() { controller.start(); }

    // Stopped -> Spotify, then forget the commands that took.
    void enterSpotify() {
        hw.tap(Button::Eject);
        clearCommands();
    }

    // Stopped -> Spotify -> Radio (tuning the first Station), then forget the
    // commands that took.
    void enterRadio() {
        hw.tap(Button::Eject);
        hw.tap(Button::Eject);
        clearCommands();
    }

    void clearCommands() {
        engine.commands.clear();
        radio.commands.clear();
    }

    FakeEngineClient engine;
    FakeRadioClient radio;
    FakeHardwareIO hw;
    ManualScheduler scheduler;
    FakeSystemControl system;
    // Queued answers for Shuffle's random pick, and the counts it was asked for.
    std::deque<std::size_t> randomPicks;
    std::vector<std::size_t> randomCounts;

    PlayerController controller{engine, radio, hw, scheduler, system, testStations(),
                                [this](std::size_t count) {
                                    randomCounts.push_back(count);
                                    const std::size_t pick = randomPicks.front();
                                    randomPicks.pop_front();
                                    return pick;
                                }};
};

}  // namespace winampdeck::testing
