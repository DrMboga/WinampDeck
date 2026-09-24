#include "core/player_controller.hpp"

#include <utility>

namespace winampdeck {

namespace {

// "Artist — Track" / "Station — Stream Title", dropping the separator when
// either half is missing.
std::string joinWithDash(const std::string& first, const std::string& second) {
    if (second.empty()) {
        return first;
    }
    if (first.empty()) {
        return second;
    }
    return first + " — " + second;
}

}  // namespace

PlayerController::PlayerController(EngineClient& engine, RadioClient& radio, HardwareIO& hardware,
                                   Scheduler& scheduler, SystemControl& system,
                                   std::vector<Station> stations, RandomIndex randomIndex)
    : engine_(engine),
      radio_(radio),
      hardware_(hardware),
      scheduler_(scheduler),
      system_(system),
      stations_(std::move(stations)),
      randomIndex_(std::move(randomIndex)) {
    engine_.setListener(this);
    radio_.setListener(this);
    hardware_.setListener(this);
}

PlayerController::~PlayerController() {
    if (ejectTimer_) {
        scheduler_.cancel(*ejectTimer_);
    }
    engine_.setListener(nullptr);
    radio_.setListener(nullptr);
    hardware_.setListener(nullptr);
}

void PlayerController::start() {
    // Both Engines run for the Deck's whole uptime; while Stopped, neither may
    // hold the audio device. go-librespot only holds it while playing, and if
    // it is, that's a phone starting playback here, which auto-switches anyway.
    radio_.mute();
    refreshPanel();
}

// --- Buttons ---------------------------------------------------------------------

void PlayerController::onButtonPressed(Button button) {
    if (shuttingDown_) {
        return;
    }
    if (button == Button::Eject) {
        // Short vs. long press is only known on release or when the timer
        // fires, whichever comes first.
        if (!ejectTimer_) {
            ejectTimer_ = scheduler_.callAfter(kEjectLongPress, [this] { onEjectHeld(); });
        }
        return;
    }

    switch (source_) {
    case Source::Spotify:
        handleSpotifyButton(button);
        break;
    case Source::Radio:
        if (stationListOpen_) {
            handleStationListButton(button);
        } else {
            handleRadioButton(button);
        }
        break;
    case Source::Stopped:
        break;
    }
    refreshPanel();
}

void PlayerController::onButtonReleased(Button button) {
    if (button != Button::Eject || !ejectTimer_ || shuttingDown_) {
        return;
    }
    scheduler_.cancel(*ejectTimer_);
    ejectTimer_.reset();
    switchSource(source_ == Source::Spotify ? Source::Radio : Source::Spotify);
    refreshPanel();
}

void PlayerController::onEjectHeld() {
    ejectTimer_.reset();
    shuttingDown_ = true;
    // Show it before powering off, since the poweroff may take us down with it.
    refreshPanel();
    system_.safeShutdown();
}

void PlayerController::handleSpotifyButton(Button button) {
    switch (button) {
    case Button::Play:
        engine_.play();
        break;
    case Button::Pause:
        engine_.pause();
        break;
    case Button::Next:
        engine_.next();
        break;
    case Button::Previous:
        engine_.previous();
        break;
    // The LEDs follow the Engine's reported state, not the button press.
    case Button::Shuffle:
        engine_.setShuffle(!spotifyShuffle_);
        break;
    case Button::Repeat:
        engine_.setRepeat(!spotifyRepeat_);
        break;
    case Button::Stop:
    case Button::Eject:
        break;
    }
}

void PlayerController::handleRadioButton(Button button) {
    if (!tunedStation_) {
        return;  // No Stations to play.
    }
    switch (button) {
    case Button::Play:
        radio_.resume();
        radioPlaying_ = true;
        break;
    case Button::Pause:
        radio_.pause();
        radioPlaying_ = false;
        break;
    case Button::Next:
        tune(stepStation(*tunedStation_, true));
        break;
    case Button::Previous:
        tune(stepStation(*tunedStation_, false));
        break;
    case Button::Shuffle:
        // A random Station other than the current one.
        if (stations_.size() > 1) {
            const std::size_t pick = randomIndex_(stations_.size() - 1);
            tune(pick >= *tunedStation_ ? pick + 1 : pick);
        }
        break;
    case Button::Repeat:
        stationListOpen_ = true;
        stationListSelection_ = *tunedStation_;
        break;
    case Button::Stop:
    case Button::Eject:
        break;
    }
}

void PlayerController::handleStationListButton(Button button) {
    switch (button) {
    case Button::Previous:
        stationListSelection_ = stepStation(stationListSelection_, false);
        break;
    case Button::Next:
        stationListSelection_ = stepStation(stationListSelection_, true);
        break;
    case Button::Play:
        if (stationListSelection_ != *tunedStation_) {
            tune(stationListSelection_);
        } else {
            radio_.resume();
            radioPlaying_ = true;
        }
        stationListOpen_ = false;
        break;
    case Button::Repeat:
        stationListOpen_ = false;
        break;
    case Button::Pause:
    case Button::Shuffle:
    case Button::Stop:
    case Button::Eject:
        break;
    }
}

// --- Source ----------------------------------------------------------------------

void PlayerController::switchSource(Source to) {
    if (to == source_) {
        return;
    }

    // Engines are never stopped, only muted: Spotify by pausing it, Radio by
    // deselecting its audio track (ADR 0004).
    if (source_ == Source::Spotify) {
        resumeSpotifyOnReturn_ = spotifyPlaying_;
        if (spotifyPlaying_) {
            engine_.pause();
        }
    } else if (source_ == Source::Radio) {
        radio_.mute();
        stationListOpen_ = false;
    }

    source_ = to;

    if (to == Source::Spotify) {
        if (resumeSpotifyOnReturn_) {
            engine_.play();
        }
        resumeSpotifyOnReturn_ = false;
    } else if (to == Source::Radio) {
        radio_.unmute();
        if (!tunedStation_ && !stations_.empty()) {
            tune(0);
        }
    }
}

void PlayerController::autoSwitchToSpotify() {
    // Spotify is already playing; there's nothing to resume.
    resumeSpotifyOnReturn_ = false;
    switchSource(Source::Spotify);
}

void PlayerController::updateSpotifyAudibleHere(bool wasAudibleHere) {
    // Only a transition into "playing on this Deck" counts. A late `playing`
    // event that was already in flight when Eject paused Spotify must not
    // yank the Source straight back.
    const bool audibleHere = spotifyActive_ && spotifyPlaying_;
    if (audibleHere && !wasAudibleHere && source_ != Source::Spotify && !shuttingDown_) {
        autoSwitchToSpotify();
    }
}

// --- Engine events ---------------------------------------------------------------

void PlayerController::onSpotifyActiveChanged(bool active) {
    const bool wasAudibleHere = spotifyActive_ && spotifyPlaying_;
    spotifyActive_ = active;
    if (!active) {
        // Playback moved to another device; there's no session here any more.
        spotifyPlaying_ = false;
        spotifyTrack_.reset();
        spotifyPosition_ = std::chrono::milliseconds{0};
        resumeSpotifyOnReturn_ = false;
    }
    updateSpotifyAudibleHere(wasAudibleHere);
    refreshPanel();
}

void PlayerController::onSpotifyPlayingChanged(bool playing) {
    const bool wasAudibleHere = spotifyActive_ && spotifyPlaying_;
    spotifyPlaying_ = playing;
    updateSpotifyAudibleHere(wasAudibleHere);
    refreshPanel();
}

void PlayerController::onSpotifyTrackChanged(const SpotifyTrack& track) {
    spotifyTrack_ = track;
    spotifyPosition_ = std::chrono::milliseconds{0};
    refreshPanel();
}

void PlayerController::onSpotifyPositionChanged(std::chrono::milliseconds position) {
    spotifyPosition_ = position;
    refreshPanel();
}

void PlayerController::onSpotifyShuffleChanged(bool enabled) {
    spotifyShuffle_ = enabled;
    refreshPanel();
}

void PlayerController::onSpotifyRepeatChanged(bool enabled) {
    spotifyRepeat_ = enabled;
    refreshPanel();
}

void PlayerController::onStreamTitleChanged(const std::string& title) {
    streamTitle_ = title;
    refreshPanel();
}

// --- Stations --------------------------------------------------------------------

void PlayerController::tune(std::size_t station) {
    tunedStation_ = station;
    radioPlaying_ = true;
    streamTitle_.clear();
    radio_.tune(stations_[station]);
}

std::size_t PlayerController::stepStation(std::size_t from, bool forward) const {
    const std::size_t count = stations_.size();
    return forward ? (from + 1) % count : (from + count - 1) % count;
}

// --- Panel -----------------------------------------------------------------------

void PlayerController::refreshPanel() {
    const bool shuffleLed = source_ == Source::Spotify && spotifyShuffle_;
    const bool repeatLed = source_ == Source::Spotify ? spotifyRepeat_
                                                      : source_ == Source::Radio && stationListOpen_;
    if (shownShuffleLed_ != shuffleLed) {
        hardware_.setLed(Led::Shuffle, shuffleLed);
        shownShuffleLed_ = shuffleLed;
    }
    if (shownRepeatLed_ != repeatLed) {
        hardware_.setLed(Led::Repeat, repeatLed);
        shownRepeatLed_ = repeatLed;
    }

    std::string text = lcdText();
    if (shownLcdText_ != text) {
        hardware_.showLcdText(text);
        shownLcdText_ = std::move(text);
    }

    Screen current = screen();
    if (shownScreen_ != current) {
        hardware_.showScreen(current);
        shownScreen_ = std::move(current);
    }
}

std::string PlayerController::lcdText() const {
    if (shuttingDown_) {
        return "Shutting down";
    }
    switch (source_) {
    case Source::Spotify:
        if (!spotifyTrack_) {
            return "Spotify";
        }
        return joinWithDash(spotifyTrack_->artist, spotifyTrack_->title);
    case Source::Radio:
        if (!tunedStation_) {
            return "Internet Radio";
        }
        return joinWithDash(stations_[*tunedStation_].name, streamTitle_);
    case Source::Stopped:
        break;
    }
    return {};
}

Screen PlayerController::screen() const {
    if (stationListOpen_) {
        return StationListScreen{stations_, stationListSelection_, *tunedStation_};
    }

    NowPlayingScreen nowPlaying;
    nowPlaying.source = source_;
    if (source_ == Source::Spotify && spotifyActive_) {
        nowPlaying.status = spotifyPlaying_ ? PlaybackStatus::Playing : PlaybackStatus::Paused;
        if (spotifyTrack_) {
            nowPlaying.artist = spotifyTrack_->artist;
            nowPlaying.title = spotifyTrack_->title;
            nowPlaying.duration = spotifyTrack_->duration;
            nowPlaying.position = spotifyPosition_;
        }
    } else if (source_ == Source::Radio && tunedStation_) {
        const Station& station = stations_[*tunedStation_];
        nowPlaying.status = radioPlaying_ ? PlaybackStatus::Playing : PlaybackStatus::Paused;
        nowPlaying.station = station.name;
        nowPlaying.logo = station.logo;
        nowPlaying.title = streamTitle_;
    }
    return nowPlaying;
}

}  // namespace winampdeck
