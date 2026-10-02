#pragma once

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <chrono>
#include <string>

#include "adapters/librespot_event_stream.hpp"
#include "adapters/librespot_rest.hpp"
#include "core/engine_client.hpp"

namespace winampdeck {

// The Spotify Engine: go-librespot's local API (docs/pi-setup.md, Phase 2).
//
// - Events come from its /events WebSocket, on the io_context's thread. On
//   every (re)connect the current state is fetched from GET /status first, so
//   a controller started mid-song is in step straight away.
// - Commands are REST calls, made in order on a background thread (see
//   LibrespotRest). They're fire-and-forget, as EngineClient asks; failures
//   are logged.
//
// If go-librespot isn't up yet or restarts, it reconnects every few seconds,
// and reports Spotify inactive and not playing while it's gone.
class GoLibrespotClient final : public EngineClient {
public:
    static constexpr std::chrono::seconds kReconnectDelay{2};

    GoLibrespotClient(asio::io_context& io, const std::string& host = "127.0.0.1", int port = 3678);

    GoLibrespotClient(const GoLibrespotClient&) = delete;
    GoLibrespotClient& operator=(const GoLibrespotClient&) = delete;

    void setListener(Listener* listener) override;
    void play() override;
    void pause() override;
    void next() override;
    void previous() override;
    void setShuffle(bool enabled) override;
    void setRepeat(bool enabled) override;

private:
    void onOpen();
    void onMessage(const std::string& message);
    void onClosed(const std::string& why);
    void onStatus(int status, const std::string& body);

    Listener* listener_ = nullptr;
    asio::steady_timer reconnect_;
    bool connected_ = false;
    bool reportedDown_ = false;  // So a long outage is logged once, not every retry.
    // Last, so they're destroyed first: neither calls back afterwards.
    LibrespotRest rest_;
    LibrespotEventStream events_;
};

}  // namespace winampdeck
