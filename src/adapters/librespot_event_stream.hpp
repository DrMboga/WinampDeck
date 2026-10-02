#pragma once

#include <asio/io_context.hpp>

#include <functional>
#include <memory>
#include <string>

namespace winampdeck {

// go-librespot's /events WebSocket, on the io_context's thread. websocketpp
// stays out of this header: it's heavy to compile, and the Pi has little
// memory to compile it with.
//
// connect() opens it; then either onOpen and later onClosed, or onClosed
// alone if it couldn't connect. Reconnecting is up to the owner.
class LibrespotEventStream {
public:
    struct Callbacks {
        std::function<void()> onOpen;
        std::function<void(const std::string& message)> onMessage;
        std::function<void(const std::string& why)> onClosed;
    };

    LibrespotEventStream(asio::io_context& io, std::string url, Callbacks callbacks);
    // Closes the connection if it's open, without calling onClosed.
    ~LibrespotEventStream();

    LibrespotEventStream(const LibrespotEventStream&) = delete;
    LibrespotEventStream& operator=(const LibrespotEventStream&) = delete;

    void connect();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace winampdeck
