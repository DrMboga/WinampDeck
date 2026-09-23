// Proves the header-only dependencies from ADR 0003 compile together under our
// C++20 settings and do the one basic thing each is here for. These pins have
// real compatibility constraints (see cmake/Dependencies.cmake), so a bump that
// breaks them should fail here, not halfway through writing an adapter.

#include <gtest/gtest.h>

#include <asio.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

TEST(Dependencies, AsioRunsPostedHandler) {
    asio::io_context io;
    bool ran = false;
    asio::post(io, [&] { ran = true; });
    io.run();
    EXPECT_TRUE(ran);
}

TEST(Dependencies, WebsocketppClientSharesOurIoContext) {
    asio::io_context io;
    websocketpp::client<websocketpp::config::asio_client> client;
    client.clear_access_channels(websocketpp::log::alevel::all);
    client.clear_error_channels(websocketpp::log::elevel::all);
    client.init_asio(&io);
    EXPECT_EQ(&client.get_io_service(), &io);
}

TEST(Dependencies, HttplibClientConstructs) {
    httplib::Client client("localhost", 3678);
    EXPECT_EQ(client.port(), 3678);
}

TEST(Dependencies, JsonParses) {
    const auto doc = nlohmann::json::parse(R"({"type":"active","data":{}})");
    EXPECT_EQ(doc.at("type"), "active");
}
