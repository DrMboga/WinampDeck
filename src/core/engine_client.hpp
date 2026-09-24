#pragma once

#include <chrono>
#include <string>

namespace winampdeck {

struct SpotifyTrack {
    std::string artist;
    std::string title;
    std::string album;
    std::chrono::milliseconds duration{0};

    bool operator==(const SpotifyTrack&) const = default;
};

// Control of, and events from, the Spotify Engine. Backed in production by
// go-librespot's local REST API (commands) and /events WebSocket (events).
// Commands are fire-and-forget: the Engine's resulting state comes back as
// events, and that is the only state PlayerController trusts.
class EngineClient {
public:
    class Listener {
    public:
        // Whether this Deck is the Spotify Connect device the connected
        // account is currently playing on. Spotify reports playback for the
        // whole account, so this is what keeps activity on a phone's own
        // speaker from counting as activity here. go-librespot scopes its
        // `active`/`inactive` events to its own instance, so the adapter can
        // map those directly (see docs/pi-setup.md, Phase 2).
        virtual void onSpotifyActiveChanged(bool active) = 0;
        virtual void onSpotifyPlayingChanged(bool playing) = 0;
        virtual void onSpotifyTrackChanged(const SpotifyTrack& track) = 0;
        virtual void onSpotifyPositionChanged(std::chrono::milliseconds position) = 0;
        virtual void onSpotifyShuffleChanged(bool enabled) = 0;
        virtual void onSpotifyRepeatChanged(bool enabled) = 0;

    protected:
        ~Listener() = default;
    };

    virtual ~EngineClient() = default;

    virtual void setListener(Listener* listener) = 0;

    // Pausing is also how Spotify is muted when it stops being the Source:
    // go-librespot releases the audio device whenever it's paused (ADR 0004).
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void next() = 0;
    virtual void previous() = 0;
    virtual void setShuffle(bool enabled) = 0;
    virtual void setRepeat(bool enabled) = 0;
};

}  // namespace winampdeck
