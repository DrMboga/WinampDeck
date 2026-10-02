#include "adapters/librespot_rest.hpp"

#include <asio/post.hpp>
#include <httplib.h>

#include <chrono>
#include <iostream>
#include <utility>

namespace winampdeck {

namespace {

using namespace std::chrono_literals;

constexpr auto kConnectTimeout = 2s;
// A skip replies once the next track has started loading.
constexpr auto kReplyTimeout = 10s;

}  // namespace

LibrespotRest::LibrespotRest(asio::io_context& io, std::string host, int port)
    : io_(io), host_(std::move(host)), port_(port), worker_([this] { run(); }) {}

LibrespotRest::~LibrespotRest() {
    alive_.reset();
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_one();
    worker_.join();
}

void LibrespotRest::post(const std::string& path, const std::string& jsonBody) {
    enqueue([path, jsonBody](httplib::Client& client) {
        const auto response =
            jsonBody.empty() ? client.Post(path) : client.Post(path, jsonBody, "application/json");
        if (!response) {
            std::cerr << "go-librespot: POST " << path << ": " << httplib::to_string(response.error())
                      << std::endl;
        } else if (response->status != 200) {
            std::cerr << "go-librespot: POST " << path << ": HTTP " << response->status << std::endl;
        }
    });
}

void LibrespotRest::get(const std::string& path, Reply onReply) {
    std::weak_ptr<bool> alive = alive_;
    enqueue([this, path, alive, onReply = std::move(onReply)](httplib::Client& client) {
        const auto response = client.Get(path);
        if (!response) {
            std::cerr << "go-librespot: GET " << path << ": " << httplib::to_string(response.error())
                      << std::endl;
        }
        const int status = response ? response->status : 0;
        std::string body = response ? response->body : std::string();
        asio::post(io_, [alive, onReply, status, body = std::move(body)] {
            if (alive.lock()) {
                onReply(status, body);
            }
        });
    });
}

void LibrespotRest::enqueue(Request request) {
    {
        std::lock_guard lock(mutex_);
        requests_.push_back(std::move(request));
    }
    wake_.notify_one();
}

void LibrespotRest::run() {
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
