#pragma once

#include <string_view>

#include "core/engine_client.hpp"

namespace winampdeck {

// go-librespot's API payloads, turned into EngineClient::Listener calls. Kept
// apart from the network code so CI can check them against recorded payloads.
// Field names follow go-librespot's daemon/api_server.go and api-spec.yml.

// One message from the /events WebSocket, `{"type": "...", "data": ...}`:
// - active / inactive: whether this Deck is the account's Connect device.
// - metadata: a new track (artists joined with ", "), and its position.
// - playing; paused, not_playing, stopped: whether it's playing.
// - seek: the position.
// - shuffle_context, repeat_context: the modes the LEDs mirror.
// Other types (will_play, volume, repeat_track, ...) change nothing here.
// Returns false, calling nothing, if the message isn't one it understands.
bool dispatchLibrespotEvent(std::string_view message, EngineClient::Listener& listener);

// The body of a 200 response to GET /status, replayed as the events that
// would have led to it, so a fresh connection starts in step. A session that
// isn't stopped counts as active. (No session at all is a 204 with no body:
// that's dispatchLibrespotNoSession.)
bool dispatchLibrespotStatus(std::string_view body, EngineClient::Listener& listener);

void dispatchLibrespotNoSession(EngineClient::Listener& listener);

}  // namespace winampdeck
