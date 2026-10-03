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

// Only the shuffle and repeat modes from that same /status body. go-librespot
// sends shuffle_context and repeat_context events when a mode changes, but
// not when a session or playlist starts with one already on, so these have to
// be asked for (see librespotEventMayChangeModes).
bool dispatchLibrespotModes(std::string_view body, EngineClient::Listener& listener);

// Whether an /events message is one after which the modes may differ from
// what was last reported, with no event to say so: `active` (a session
// arriving from a phone) and `metadata` (a new track, perhaps a new playlist).
bool librespotEventMayChangeModes(std::string_view message);

void dispatchLibrespotNoSession(EngineClient::Listener& listener);

}  // namespace winampdeck
