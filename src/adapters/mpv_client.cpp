#include "adapters/mpv_client.hpp"

#include <asio/buffer.hpp>
#include <asio/buffers_iterator.hpp>
#include <asio/read_until.hpp>
#include <asio/write.hpp>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <iostream>
#include <system_error>
#include <utility>

namespace winampdeck {

namespace {

using nlohmann::json;

// observe_property's id for the stream title, echoed in its change events.
constexpr int kTitleObserver = 1;
constexpr const char* kTitleProperty = "metadata/by-key/icy-title";

std::string setProperty(const char* name, json value) {
    return json{{"command", {"set_property", name, std::move(value)}}}.dump();
}

}  // namespace

MpvClient::MpvClient(asio::io_context& io, std::filesystem::path socket)
    : path_(std::move(socket)), socket_(io), reconnect_(io) {
    connect();
}

MpvClient::~MpvClient() {
    reconnect_.cancel();
    std::error_code error;
    socket_.close(error);
}

void MpvClient::setListener(Listener* listener) {
    listener_ = listener;
}

void MpvClient::tune(const Station& station) {
    station_ = station;
    paused_ = false;
    if (!muted_) {
        load();
    }
}

void MpvClient::pause() {
    paused_ = true;
    command(setProperty("pause", true));
}

void MpvClient::resume() {
    paused_ = false;
    command(setProperty("pause", false));
}

void MpvClient::mute() {
    muted_ = true;
    command(setProperty("aid", "no"));
}

void MpvClient::unmute() {
    muted_ = false;
    if (station_) {
        load();
    } else {
        command(setProperty("aid", "auto"));
    }
}

void MpvClient::load() {
    command(setProperty("pause", paused_));
    command(setProperty("aid", "auto"));
    command(json{{"command", {"loadfile", station_->url, "replace"}}}.dump());
}

void MpvClient::connect() {
    socket_.async_connect(asio::local::stream_protocol::endpoint(path_.string()),
                          [this](const std::error_code& error) {
                              if (error) {
                                  onLost(error.message());
                              } else {
                                  onConnected();
                              }
                          });
}

void MpvClient::onConnected() {
    connected_ = true;
    reportedDown_ = false;
    std::cout << "mpv: connected to " << path_.string() << std::endl;
    readLine();

    command(json{{"command", {"observe_property", kTitleObserver, kTitleProperty}}}.dump());
    command(setProperty("pause", paused_));
    if (muted_) {
        command(setProperty("aid", "no"));
    } else if (station_) {
        load();
    }
}

void MpvClient::onLost(const std::string& why) {
    const bool wasConnected = std::exchange(connected_, false);
    if (wasConnected || !reportedDown_) {
        std::cerr << "mpv: " << (wasConnected ? "lost " : "can't connect to ") << path_.string()
                  << " (" << why << "), retrying every " << kReconnectDelay.count() << "s" << std::endl;
        reportedDown_ = true;
    }
    if (wasConnected && listener_ != nullptr) {
        listener_->onStreamTitleChanged("");
    }
    std::error_code ignored;
    socket_.close(ignored);
    input_.consume(input_.size());
    reconnect_.expires_after(kReconnectDelay);
    reconnect_.async_wait([this](const std::error_code& error) {
        if (!error) {
            connect();
        }
    });
}

void MpvClient::readLine() {
    asio::async_read_until(socket_, input_, '\n', [this](const std::error_code& error, std::size_t length) {
        if (error) {
            if (error != asio::error::operation_aborted) {
                onLost(error.message());
            }
            return;
        }
        const auto begin = asio::buffers_begin(input_.data());
        std::string line(begin, begin + static_cast<std::ptrdiff_t>(length - 1));
        input_.consume(length);
        onLine(line);
        readLine();
    });
}

void MpvClient::onLine(const std::string& line) {
    const json message = json::parse(line, nullptr, /*allow_exceptions=*/false);
    if (!message.is_object()) {
        return;
    }
    if (message.value("event", "") == "property-change" && message.value("id", 0) == kTitleObserver) {
        const auto data = message.find("data");
        const std::string title = data != message.end() && data->is_string() ? data->get<std::string>() : "";
        if (listener_ != nullptr) {
            listener_->onStreamTitleChanged(title);
        }
    } else if (const auto error = message.find("error");
               error != message.end() && error->is_string() && *error != "success") {
        std::cerr << "mpv: " << line << std::endl;
    }
}

void MpvClient::command(std::string line) {
    if (!connected_) {
        return;  // Replayed by onConnected.
    }
    output_.push_back(std::move(line) + "\n");
    if (output_.size() == 1) {
        writeNext();
    }
}

void MpvClient::writeNext() {
    asio::async_write(socket_, asio::buffer(output_.front()), [this](const std::error_code& error, std::size_t) {
        if (error) {
            output_.clear();  // The read side notices the connection is gone.
            return;
        }
        output_.pop_front();
        if (!output_.empty()) {
            writeNext();
        }
    });
}

}  // namespace winampdeck
