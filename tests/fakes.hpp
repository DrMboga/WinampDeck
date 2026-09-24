#pragma once

// In-memory stand-ins for go-librespot, mpv, pigpio, the event loop's timers,
// and the OS, so PlayerController can be exercised with no real hardware,
// sockets, or subprocesses. Commands are recorded as plain strings so tests
// read like the button map.

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/engine_client.hpp"
#include "core/hardware_io.hpp"
#include "core/radio_client.hpp"
#include "core/scheduler.hpp"
#include "core/system_control.hpp"

namespace winampdeck::testing {

class FakeEngineClient final : public EngineClient {
public:
    std::vector<std::string> commands;

    void setListener(Listener* newListener) override { listener = newListener; }
    void play() override { commands.emplace_back("play"); }
    void pause() override { commands.emplace_back("pause"); }
    void next() override { commands.emplace_back("next"); }
    void previous() override { commands.emplace_back("previous"); }
    void setShuffle(bool enabled) override { commands.emplace_back(enabled ? "shuffle on" : "shuffle off"); }
    void setRepeat(bool enabled) override { commands.emplace_back(enabled ? "repeat on" : "repeat off"); }

    void emitActive(bool active) { listener->onSpotifyActiveChanged(active); }
    void emitPlaying(bool playing) { listener->onSpotifyPlayingChanged(playing); }
    void emitTrack(const SpotifyTrack& track) { listener->onSpotifyTrackChanged(track); }
    void emitPosition(std::chrono::milliseconds position) { listener->onSpotifyPositionChanged(position); }
    void emitShuffle(bool enabled) { listener->onSpotifyShuffleChanged(enabled); }
    void emitRepeat(bool enabled) { listener->onSpotifyRepeatChanged(enabled); }

    // What go-librespot reports when a phone picks this Deck and presses Play.
    void emitStartedPlayingHere() {
        emitActive(true);
        emitPlaying(true);
    }

    Listener* listener = nullptr;
};

class FakeRadioClient final : public RadioClient {
public:
    std::vector<std::string> commands;

    void setListener(Listener* newListener) override { listener = newListener; }
    void tune(const Station& station) override { commands.push_back("tune " + station.name); }
    void pause() override { commands.emplace_back("pause"); }
    void resume() override { commands.emplace_back("resume"); }
    void mute() override { commands.emplace_back("mute"); }
    void unmute() override { commands.emplace_back("unmute"); }

    void emitStreamTitle(const std::string& title) { listener->onStreamTitleChanged(title); }

    Listener* listener = nullptr;
};

class FakeHardwareIO final : public HardwareIO {
public:
    std::map<Led, bool> leds;
    std::string lcd;
    std::optional<Screen> screen;

    void setListener(Listener* newListener) override { listener = newListener; }
    void setLed(Led led, bool on) override { leds[led] = on; }
    void showLcdText(const std::string& text) override { lcd = text; }
    void showScreen(const Screen& newScreen) override { screen = newScreen; }

    void press(Button button) { listener->onButtonPressed(button); }
    void release(Button button) { listener->onButtonReleased(button); }
    // A normal, short press.
    void tap(Button button) {
        press(button);
        release(button);
    }

    bool led(Led which) const {
        const auto it = leds.find(which);
        return it != leds.end() && it->second;
    }
    const NowPlayingScreen& nowPlaying() const { return std::get<NowPlayingScreen>(*screen); }
    const StationListScreen& stationList() const { return std::get<StationListScreen>(*screen); }
    bool showsStationList() const { return screen && std::holds_alternative<StationListScreen>(*screen); }

    Listener* listener = nullptr;
};

// Timers that only fire when the test advances time.
class ManualScheduler final : public Scheduler {
public:
    TimerId callAfter(std::chrono::milliseconds delay, std::function<void()> callback) override {
        const TimerId id = nextId_++;
        timers_.emplace(id, Timer{now_ + delay, std::move(callback)});
        return id;
    }

    void cancel(TimerId timer) override { timers_.erase(timer); }

    void advance(std::chrono::milliseconds by) {
        now_ += by;
        // Fire in id order; a callback may schedule or cancel other timers.
        for (auto it = timers_.begin(); it != timers_.end();) {
            if (it->second.due <= now_) {
                auto callback = std::move(it->second.callback);
                timers_.erase(it);
                callback();
                it = timers_.begin();
            } else {
                ++it;
            }
        }
    }

    std::size_t pending() const { return timers_.size(); }

private:
    struct Timer {
        std::chrono::milliseconds due;
        std::function<void()> callback;
    };

    std::chrono::milliseconds now_{0};
    TimerId nextId_ = 1;
    std::map<TimerId, Timer> timers_;
};

class FakeSystemControl final : public SystemControl {
public:
    int shutdowns = 0;

    void safeShutdown() override { ++shutdowns; }
};

}  // namespace winampdeck::testing
