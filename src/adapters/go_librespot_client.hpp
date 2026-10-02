#pragma once

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "core/engine_client.hpp"

namespace httplib {
class Client;
}

namespace winampdeck {

// The Spotify Engine: go-librespot's local API (docs/pi-setup.md, Phase 2).
//
// - Events come from its /events WebSocket, on the io_context's thread. On
//   every (re)connect the current state is fetched from GET /status first, so
//   a controller started mid-song is in step straight away.
// - Commands are REST calls, made in order on a background thread: a slow
//   reply (a skip waits for the next track to load) never holds up the event
//   loop. They're fire-and-forget, as EngineClient asks; failures are logged.
//
// If go-librespot isn't up yet or restarts, it reconnects every few seconds,
// and reports Spotify inactive and not playing while it's gone.
class GoLibrespotClient final : public EngineClient {
public:
    static constexpr std::chrono::seconds kReconnectDelay{2};

    GoLibrespotClient(asio::io_context& io, std::string host = "127.0.0.1", int port = 3678);
    // Waits for a command in progress to finish or time out.
    ~GoLibrespotClient() override;

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
    using WebSocket = websocketpp::client<websocketpp::config::asio_client>;
    using Request = std::function<void(httplib::Client&)>;

    void connect();
    void onOpen();
    void onMessage(const std::string& payload);
    void onLost(const std::string& why);
    void fetchStatus();

    void post(const std::string& path, const std::string& body = {});
    void enqueue(Request request);
    void run();

    asio::io_context& io_;
    std::string host_;
    int port_;
    Listener* listener_ = nullptr;

    WebSocket socket_;
    websocketpp::connection_hdl connection_;
    asio::steady_timer reconnect_;
    bool connected_ = false;
    bool reportedDown_ = false;  // So a long outage is logged once, not every retry.

    // Shared with what the worker posts back to the event loop, so a reply
    // that arrives after destruction finds it gone.
    std::shared_ptr<GoLibrespotClient*> self_;

    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Request> requests_;
    bool stopping_ = false;
    std::thread worker_;
};

}  // namespace winampdeck
