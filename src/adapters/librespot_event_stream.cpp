#include "adapters/librespot_event_stream.hpp"

#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

#include <system_error>
#include <utility>

namespace winampdeck {

class LibrespotEventStream::Impl {
public:
    Impl(asio::io_context& io, std::string url, Callbacks callbacks)
        : url_(std::move(url)), callbacks_(std::move(callbacks)) {
        socket_.clear_access_channels(websocketpp::log::alevel::all);
        socket_.clear_error_channels(websocketpp::log::elevel::all);
        socket_.init_asio(&io);
        socket_.set_open_handler([this](websocketpp::connection_hdl) {
            open_ = true;
            callbacks_.onOpen();
        });
        socket_.set_message_handler([this](websocketpp::connection_hdl, WebSocket::message_ptr message) {
            callbacks_.onMessage(message->get_payload());
        });
        socket_.set_fail_handler([this](websocketpp::connection_hdl connection) {
            std::error_code error;
            const auto failed = socket_.get_con_from_hdl(connection, error);
            closed(failed ? failed->get_ec().message() : "connection failed");
        });
        socket_.set_close_handler([this](websocketpp::connection_hdl) { closed("connection closed"); });
    }

    ~Impl() {
        closing_ = true;
        if (open_) {
            std::error_code error;
            socket_.close(connection_, websocketpp::close::status::going_away, "", error);
        }
    }

    void connect() {
        std::error_code error;
        const auto connection = socket_.get_connection(url_, error);
        if (error) {
            closed(error.message());
            return;
        }
        connection_ = connection->get_handle();
        socket_.connect(connection);
    }

private:
    using WebSocket = websocketpp::client<websocketpp::config::asio_client>;

    void closed(const std::string& why) {
        open_ = false;
        if (!closing_) {
            callbacks_.onClosed(why);
        }
    }

    std::string url_;
    Callbacks callbacks_;
    WebSocket socket_;
    websocketpp::connection_hdl connection_;
    bool open_ = false;
    bool closing_ = false;
};

LibrespotEventStream::LibrespotEventStream(asio::io_context& io, std::string url, Callbacks callbacks)
    : impl_(std::make_unique<Impl>(io, std::move(url), std::move(callbacks))) {}

LibrespotEventStream::~LibrespotEventStream() = default;

void LibrespotEventStream::connect() {
    impl_->connect();
}

}  // namespace winampdeck
