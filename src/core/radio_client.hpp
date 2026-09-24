#pragma once

#include <string>

#include "core/types.hpp"

namespace winampdeck {

// Control of, and events from, the Internet Radio Engine. Backed in production
// by mpv's JSON IPC socket. Station stepping, random picks, and the Station
// List are PlayerController's business; this only plays what it's told to.
class RadioClient {
public:
    class Listener {
    public:
        // The stream's own "now playing" metadata (ICY title), or empty.
        virtual void onStreamTitleChanged(const std::string& title) = 0;

    protected:
        ~Listener() = default;
    };

    virtual ~RadioClient() = default;

    virtual void setListener(Listener* listener) = 0;

    // Load the Station's stream and start playing it, unpaused.
    virtual void tune(const Station& station) = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;

    // Release / reacquire the audio device without touching pause state. mpv
    // only lets go of the device when its audio track is deselected (`aid no`
    // / `aid auto`), not on plain pause (ADR 0004).
    virtual void mute() = 0;
    virtual void unmute() = 0;
};

}  // namespace winampdeck
