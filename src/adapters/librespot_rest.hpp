#pragma once

#include <asio/io_context.hpp>

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace httplib {
class Client;
}

namespace winampdeck {

// go-librespot's REST API, called in order on a background thread so that a
// slow reply (a skip waits for the next track to load) never holds up the
// event loop. cpp-httplib stays out of this header: it's heavy to compile,
// and the Pi has little memory to compile it with.
class LibrespotRest {
public:
    // Called on the io_context's thread with the HTTP status and body, or
    // status 0 if the request got no reply at all.
    using Reply = std::function<void(int status, const std::string& body)>;

    LibrespotRest(asio::io_context& io, std::string host, int port);
    // Waits for a request in progress to finish or time out.
    ~LibrespotRest();

    LibrespotRest(const LibrespotRest&) = delete;
    LibrespotRest& operator=(const LibrespotRest&) = delete;

    // Fire-and-forget: failures are logged, not reported.
    void post(const std::string& path, const std::string& jsonBody = {});
    // `onReply` is dropped if this is destroyed before the reply arrives.
    void get(const std::string& path, Reply onReply);

private:
    using Request = std::function<void(httplib::Client&)>;

    void enqueue(Request request);
    void run();

    asio::io_context& io_;
    std::string host_;
    int port_;
    // Shared with replies posted to the event loop, so one that arrives after
    // destruction finds it gone.
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Request> requests_;
    bool stopping_ = false;
    std::thread worker_;
};

}  // namespace winampdeck
