#include "adapters/go_librespot_client.hpp"

#include <asio/post.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <system_error>
#include <utility>

#include "adapters/librespot_events.hpp"

namespace winampdeck {

namespace {

using namespace std::chrono_literals;

constexpr auto kConnectTimeout = 2s;
// A skip replies once the next track has started loading.
constexpr auto kReplyTimeout = 10s;

std::string modeBody(const char* key, bool enabled) {
    return nlohmann::json{{key, enabled}}.dump();
}

}  // namespace

GoLibrespotClient::GoLibrespotClient(asio::io_context& io, std::string host, int port)
    : io_(io),
      host_(std::move(host)),
      port_(port),
      reconnect_(io),
      self_(std::make_shared<GoLibrespotClient*>(this)),
      worker_([this] { run(); }) {
    socket_.clear_access_channels(websocketpp::log::alevel::all);
    socket_.clear_error_channels(websocketpp::log::elevel::all);
    socket_.init_asio(&io_);
    socket_.set_open_handler([this](websocketpp::connection_hdl) { onOpen(); });
    socket_.set_message_handler([this](websocketpp::connection_hdl, WebSocket::message_ptr message) {
        onMessage(message->get_payload());
    });
    socket_.set_fail_handler([this](websocketpp::connection_hdl connection) {
        std::error_code error;
        const auto failed = socket_.get_con_from_hdl(connection, error);
        onLost(failed ? failed->get_ec().message() : "connection failed");
    });
    socket_.set_close_handler([this](websocketpp::connection_hdl) { onLost("connection closed"); });
    connect();
}

GoLibrespotClient::~GoLibrespotClient() {
    self_.reset();
    reconnect_.cancel();
    if (connected_) {
        std::error_code error;
        socket_.close(connection_, websocketpp::close::status::going_away, "", error);
    }
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_one();
    worker_.join();
}

void GoLibrespotClient::setListener(Listener* listener) {
    listener_ = listener;
}

void GoLibrespotClient::play() {
    post("/player/resume");
}

void GoLibrespotClient::pause() {
    post("/player/pause");
}

void GoLibrespotClient::next() {
    post("/player/next");
}

void GoLibrespotClient::previous() {
    post("/player/prev");
}

void GoLibrespotClient::setShuffle(bool enabled) {
    post("/player/shuffle_context", modeBody("shuffle_context", enabled));
}

void GoLibrespotClient::setRepeat(bool enabled) {
    post("/player/repeat_context", modeBody("repeat_context", enabled));
}

void GoLibrespotClient::connect() {
    std::error_code error;
    const auto connection =
        socket_.get_connection("ws://" + host_ + ":" + std::to_string(port_) + "/events", error);
    if (error) {
        onLost(error.message());
        return;
    }
    connection_ = connection->get_handle();
    socket_.connect(connection);
}

void GoLibrespotClient::onOpen() {
    connected_ = true;
    reportedDown_ = false;
    std::cout << "go-librespot: connected" << std::endl;
    fetchStatus();
}

void GoLibrespotClient::onMessage(const std::string& payload) {
    if (listener_ != nullptr && !dispatchLibrespotEvent(payload, *listener_)) {
        std::cerr << "go-librespot: ignored an event it couldn't read: " << payload << std::endl;
    }
}

void GoLibrespotClient::onLost(const std::string& why) {
    const bool wasConnected = std::exchange(connected_, false);
    if (wasConnected || !reportedDown_) {
        std::cerr << "go-librespot: " << (wasConnected ? "lost the event stream" : "can't connect")
                  << " (" << why << "), retrying every " << kReconnectDelay.count() << "s" << std::endl;
        reportedDown_ = true;
    }
    if (wasConnected && listener_ != nullptr) {
        dispatchLibrespotNoSession(*listener_);
    }
    reconnect_.expires_after(kReconnectDelay);
    reconnect_.async_wait([this](const std::error_code& error) {
        if (!error) {
            connect();
        }
    });
}

void GoLibrespotClient::fetchStatus() {
    std::weak_ptr<GoLibrespotClient*> self = self_;
    enqueue([this, self](httplib::Client& client) {
        const auto response = client.Get("/status");
        if (!response) {
            std::cerr << "go-librespot: GET /status: " << httplib::to_string(response.error()) << std::endl;
            return;
        }
        asio::post(io_, [self, status = response->status, body = response->body] {
            const auto alive = self.lock();
            if (!alive) {
                return;
            }
            GoLibrespotClient& client = **alive;
            if (client.listener_ == nullptr) {
                return;
            }
            if (status == 204) {
                dispatchLibrespotNoSession(*client.listener_);
            } else if (status != 200 || !dispatchLibrespotStatus(body, *client.listener_)) {
                std::cerr << "go-librespot: GET /status: unexpected reply (HTTP " << status << ")"
                          << std::endl;
            }
        });
    });
}

void GoLibrespotClient::post(const std::string& path, const std::string& body) {
    enqueue([path, body](httplib::Client& client) {
        const auto response =
            body.empty() ? client.Post(path) : client.Post(path, body, "application/json");
        if (!response) {
            std::cerr << "go-librespot: POST " << path << ": " << httplib::to_string(response.error())
                      << std::endl;
        } else if (response->status != 200) {
            std::cerr << "go-librespot: POST " << path << ": HTTP " << response->status << std::endl;
        }
    });
}

void GoLibrespotClient::enqueue(Request request) {
    {
        std::lock_guard lock(mutex_);
        requests_.push_back(std::move(request));
    }
    wake_.notify_one();
}

void GoLibrespotClient::run() {
    httplib::Client client(host_, port_);
    client.set_connection_timeout(kConnectTimeout);
    client.set_read_timeout(kReplyTimeout);
    client.set_write_timeout(kConnectTimeout);
    while (true) {
        Request request;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || !requests_.empty(); });
            if (stopping_) {
                return;
            }
            request = std::move(requests_.front());
            requests_.pop_front();
        }
        request(client);
    }
}

}  // namespace winampdeck
