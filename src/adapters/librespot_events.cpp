#include "adapters/librespot_events.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <string>

namespace winampdeck {

namespace {

using nlohmann::json;

std::string text(const json& object, const char* key) {
    const auto it = object.find(key);
    return it != object.end() && it->is_string() ? it->get<std::string>() : std::string();
}

std::chrono::milliseconds milliseconds(const json& object, const char* key) {
    const auto it = object.find(key);
    return std::chrono::milliseconds(it != object.end() && it->is_number() ? it->get<std::int64_t>() : 0);
}

bool flag(const json& object, const char* key) {
    const auto it = object.find(key);
    return it != object.end() && it->is_boolean() && it->get<bool>();
}

// go-librespot's track object (ApiTrack).
SpotifyTrack track(const json& data) {
    SpotifyTrack result;
    if (const auto artists = data.find("artist_names"); artists != data.end() && artists->is_array()) {
        for (const auto& artist : *artists) {
            if (artist.is_string()) {
                if (!result.artist.empty()) {
                    result.artist += ", ";
                }
                result.artist += artist.get<std::string>();
            }
        }
    }
    result.title = text(data, "name");
    result.album = text(data, "album_name");
    result.duration = milliseconds(data, "duration");
    result.coverUrl = text(data, "album_cover_url");  // May be null.
    return result;
}

// The value of a `{"value": bool}` event.
bool value(const json& data) {
    return data.is_object() && flag(data, "value");
}

}  // namespace

bool dispatchLibrespotEvent(std::string_view message, EngineClient::Listener& listener) {
    const json event = json::parse(message, nullptr, /*allow_exceptions=*/false);
    if (!event.is_object() || !event.contains("type") || !event["type"].is_string()) {
        return false;
    }
    const std::string type = event["type"].get<std::string>();
    const json data = event.value("data", json());

    if (type == "active") {
        listener.onSpotifyActiveChanged(true);
    } else if (type == "inactive") {
        listener.onSpotifyActiveChanged(false);
    } else if (type == "metadata") {
        if (!data.is_object()) {
            return false;
        }
        listener.onSpotifyTrackChanged(track(data));
        listener.onSpotifyPositionChanged(milliseconds(data, "position"));
    } else if (type == "playing") {
        listener.onSpotifyPlayingChanged(true);
    } else if (type == "paused" || type == "not_playing" || type == "stopped") {
        listener.onSpotifyPlayingChanged(false);
    } else if (type == "seek") {
        if (!data.is_object()) {
            return false;
        }
        listener.onSpotifyPositionChanged(milliseconds(data, "position"));
    } else if (type == "shuffle_context") {
        listener.onSpotifyShuffleChanged(value(data));
    } else if (type == "repeat_context") {
        listener.onSpotifyRepeatChanged(value(data));
    }
    return true;
}

bool dispatchLibrespotStatus(std::string_view body, EngineClient::Listener& listener) {
    const json status = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (!status.is_object()) {
        return false;
    }
    const bool stopped = flag(status, "stopped");
    listener.onSpotifyActiveChanged(!stopped);
    if (const auto it = status.find("track"); it != status.end() && it->is_object()) {
        listener.onSpotifyTrackChanged(track(*it));
        listener.onSpotifyPositionChanged(milliseconds(*it, "position"));
    }
    listener.onSpotifyShuffleChanged(flag(status, "shuffle_context"));
    listener.onSpotifyRepeatChanged(flag(status, "repeat_context"));
    // Last, so an auto-switch to Spotify already knows the track.
    listener.onSpotifyPlayingChanged(!stopped && !flag(status, "paused"));
    return true;
}

bool dispatchLibrespotModes(std::string_view body, EngineClient::Listener& listener) {
    const json status = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (!status.is_object()) {
        return false;
    }
    listener.onSpotifyShuffleChanged(flag(status, "shuffle_context"));
    listener.onSpotifyRepeatChanged(flag(status, "repeat_context"));
    return true;
}

bool librespotEventMayChangeModes(std::string_view message) {
    const json event = json::parse(message, nullptr, /*allow_exceptions=*/false);
    if (!event.is_object()) {
        return false;
    }
    const std::string type = text(event, "type");
    return type == "active" || type == "metadata";
}

void dispatchLibrespotNoSession(EngineClient::Listener& listener) {
    listener.onSpotifyActiveChanged(false);
    listener.onSpotifyPlayingChanged(false);
}

}  // namespace winampdeck
