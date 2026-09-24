#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/engine_client.hpp"
#include "core/hardware_io.hpp"
#include "core/radio_client.hpp"
#include "core/scheduler.hpp"
#include "core/system_control.hpp"
#include "core/types.hpp"

namespace winampdeck {

// The Deck's decision-making: which Source is audible, what each button does
// for that Source, the Station List, auto-switching to Spotify, and what the
// LEDs/TFT/LCD show as a result. Talks to the outside world only through the
// interfaces it's given, and must be driven from a single thread (the event
// loop that also delivers every listener callback and timer).
class PlayerController final : private EngineClient::Listener,
                               private RadioClient::Listener,
                               private HardwareIO::Listener {
public:
    // How long Eject must be held for a Safe Shutdown instead of a Source switch.
    static constexpr std::chrono::milliseconds kEjectLongPress{2000};

    // Returns a uniformly random index in [0, count). Injected so Shuffle's
    // random Station pick is deterministic under test.
    using RandomIndex = std::function<std::size_t(std::size_t count)>;

    PlayerController(EngineClient& engine, RadioClient& radio, HardwareIO& hardware,
                     Scheduler& scheduler, SystemControl& system, std::vector<Station> stations,
                     RandomIndex randomIndex);
    ~PlayerController();

    PlayerController(const PlayerController&) = delete;
    PlayerController& operator=(const PlayerController&) = delete;

    // Puts the Deck in its initial Stopped state: Radio muted, displays drawn.
    void start();

private:
    // EngineClient::Listener
    void onSpotifyActiveChanged(bool active) override;
    void onSpotifyPlayingChanged(bool playing) override;
    void onSpotifyTrackChanged(const SpotifyTrack& track) override;
    void onSpotifyPositionChanged(std::chrono::milliseconds position) override;
    void onSpotifyShuffleChanged(bool enabled) override;
    void onSpotifyRepeatChanged(bool enabled) override;

    // RadioClient::Listener
    void onStreamTitleChanged(const std::string& title) override;

    // HardwareIO::Listener
    void onButtonPressed(Button button) override;
    void onButtonReleased(Button button) override;

    void handleSpotifyButton(Button button);
    void handleRadioButton(Button button);
    void handleStationListButton(Button button);
    void onEjectHeld();

    void switchSource(Source to);
    void autoSwitchToSpotify();
    void updateSpotifyAudibleHere(bool wasAudibleHere);
    void tune(std::size_t station);
    std::size_t stepStation(std::size_t from, bool forward) const;

    // Recomputes what the panel should show and pushes whatever changed.
    void refreshPanel();
    std::string lcdText() const;
    Screen screen() const;

    EngineClient& engine_;
    RadioClient& radio_;
    HardwareIO& hardware_;
    Scheduler& scheduler_;
    SystemControl& system_;
    const std::vector<Station> stations_;
    RandomIndex randomIndex_;

    Source source_ = Source::Stopped;
    bool shuttingDown_ = false;
    std::optional<Scheduler::TimerId> ejectTimer_;

    // Spotify, as last reported by the Engine.
    bool spotifyActive_ = false;
    bool spotifyPlaying_ = false;
    bool spotifyShuffle_ = false;
    bool spotifyRepeat_ = false;
    std::optional<SpotifyTrack> spotifyTrack_;
    std::chrono::milliseconds spotifyPosition_{0};
    // Whether switching back to Spotify should resume it: it was playing when
    // Eject switched away from it, and nothing has happened to it since.
    bool resumeSpotifyOnReturn_ = false;

    // Internet Radio. Tuned lazily, the first time Radio becomes the Source.
    std::optional<std::size_t> tunedStation_;
    bool radioPlaying_ = false;
    std::string streamTitle_;
    bool stationListOpen_ = false;
    std::size_t stationListSelection_ = 0;

    // What the panel was last told to show, so only changes get pushed.
    std::optional<bool> shownShuffleLed_;
    std::optional<bool> shownRepeatLed_;
    std::optional<std::string> shownLcdText_;
    std::optional<Screen> shownScreen_;
};

}  // namespace winampdeck
