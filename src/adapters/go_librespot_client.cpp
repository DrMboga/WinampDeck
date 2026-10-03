#include "adapters/go_librespot_client.hpp"

#include <iostream>
#include <system_error>
#include <utility>

#include "adapters/librespot_events.hpp"

namespace winampdeck {

namespace {

// The body of a shuffle_context / repeat_context request.
std::string modeBody(const std::string& key, bool enabled) {
    return "{\"" + key + "\":" + (enabled ? "true" : "false") + "}";
}

}  // namespace

GoLibrespotClient::GoLibrespotClient(asio::io_context& io, const std::string& host, int port)
    : reconnect_(io),
      rest_(io, host, port),
      events_(io, "ws://" + host + ":" + std::to_string(port) + "/events",
              {[this] { onOpen(); }, [this](const std::string& message) { onMessage(message); },
               [this](const std::string& why) { onClosed(why); }}) {
    events_.connect();
}

void GoLibrespotClient::setListener(Listener* listener) {
    listener_ = listener;
}

void GoLibrespotClient::play() {
    rest_.post("/player/resume");
}

void GoLibrespotClient::pause() {
    rest_.post("/player/pause");
}

void GoLibrespotClient::next() {
    rest_.post("/player/next");
}

void GoLibrespotClient::previous() {
    rest_.post("/player/prev");
}

void GoLibrespotClient::setShuffle(bool enabled) {
    rest_.post("/player/shuffle_context", modeBody("shuffle_context", enabled));
}

void GoLibrespotClient::setRepeat(bool enabled) {
    rest_.post("/player/repeat_context", modeBody("repeat_context", enabled));
}

void GoLibrespotClient::onOpen() {
    connected_ = true;
    reportedDown_ = false;
    std::cout << "go-librespot: connected" << std::endl;
    rest_.get("/status", [this](int status, const std::string& body) { onStatus(status, body); });
}

void GoLibrespotClient::onMessage(const std::string& message) {
    if (listener_ != nullptr && !dispatchLibrespotEvent(message, *listener_)) {
        std::cerr << "go-librespot: ignored an event it couldn't read: " << message << std::endl;
    }
    // A session or playlist that starts with shuffle or repeat already on
    // brings no event for it, and the buttons toggle from the state last
    // reported, so ask.
    if (librespotEventMayChangeModes(message)) {
        rest_.get("/status", [this](int status, const std::string& body) {
            if (listener_ != nullptr && status == 200) {
                dispatchLibrespotModes(body, *listener_);
            }
        });
    }
}

void GoLibrespotClient::onClosed(const std::string& why) {
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
            events_.connect();
        }
    });
}

void GoLibrespotClient::onStatus(int status, const std::string& body) {
    if (listener_ == nullptr || status == 0) {
        return;  // No reply at all is logged already.
    }
    if (status == 204) {
        dispatchLibrespotNoSession(*listener_);
    } else if (status != 200 || !dispatchLibrespotStatus(body, *listener_)) {
        std::cerr << "go-librespot: GET /status: unexpected reply (HTTP " << status << ")" << std::endl;
    }
}

}  // namespace winampdeck
