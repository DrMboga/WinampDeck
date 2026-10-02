// MpvClient against a fake mpv on a real Unix socket: the commands it sends
// for each RadioClient call, the stream title, and reconnecting.

#include <gtest/gtest.h>

#include <asio/buffers_iterator.hpp>
#include <asio/io_context.hpp>
#include <asio/local/stream_protocol.hpp>
#include <asio/read_until.hpp>
#include <asio/streambuf.hpp>
#include <asio/write.hpp>
#include <nlohmann/json.hpp>

#include <unistd.h>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <vector>

#include "adapters/mpv_client.hpp"

namespace winampdeck {
namespace {

using namespace std::chrono_literals;
using nlohmann::json;
using asio::local::stream_protocol;

// Accepts one connection at a time and records the "command" of each line.
class FakeMpv {
public:
    FakeMpv(asio::io_context& io, const std::filesystem::path& path)
        : acceptor_(io, stream_protocol::endpoint(path.string())), socket_(io) {
        accept();
    }

    std::vector<json> commands;
    int connections = 0;

    void send(const json& message) {
        const std::string line = message.dump() + "\n";
        asio::write(socket_, asio::buffer(line));
    }

    void hangUp() {
        std::error_code ignored;
        socket_.close(ignored);
        accept();
    }

private:
    void accept() {
        acceptor_.async_accept(socket_, [this](const std::error_code& error) {
            if (!error) {
                ++connections;
                read();
            }
        });
    }

    void read() {
        asio::async_read_until(socket_, input_, '\n', [this](const std::error_code& error, std::size_t length) {
            if (error) {
                return;
            }
            const auto begin = asio::buffers_begin(input_.data());
            const std::string line(begin, begin + static_cast<std::ptrdiff_t>(length));
            input_.consume(length);
            commands.push_back(json::parse(line).at("command"));
            read();
        });
    }

    stream_protocol::acceptor acceptor_;
    stream_protocol::socket socket_;
    asio::streambuf input_;
};

class RecordingListener final : public RadioClient::Listener {
public:
    std::vector<std::string> titles;
    void onStreamTitleChanged(const std::string& title) override { titles.push_back(title); }
};

json setProperty(const char* name, json value) {
    return json::array({"set_property", name, std::move(value)});
}

const json kObserveTitle = json::array({"observe_property", 1, "metadata/by-key/icy-title"});
const Station kStation{"Radio Bob", "https://streams.radiobob.de/bob-live/mp3-192", "bob.565"};

json loadfile(const Station& station) {
    return json::array({"loadfile", station.url, "replace"});
}

// A socket path of this test process's own, with nothing left at it.
std::filesystem::path freshSocketPath() {
    auto path = std::filesystem::temp_directory_path() /
                ("winampdeck_mpv_test_" + std::to_string(::getpid()) + ".sock");
    std::filesystem::remove(path);
    return path;
}

class MpvClientTest : public ::testing::Test {
protected:
    MpvClientTest() : path_(freshSocketPath()), mpv_(io_, path_) {}

    ~MpvClientTest() override { std::filesystem::remove(path_); }

    // Runs the event loop until `done`, or gives up after a few seconds.
    bool runUntil(const std::function<bool()>& done) {
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        while (!done() && std::chrono::steady_clock::now() < deadline) {
            io_.restart();
            io_.run_for(10ms);
        }
        return done();
    }

    // Lets anything already sent arrive.
    void settle() {
        io_.restart();
        io_.run_for(50ms);
    }

    std::vector<json> commandsAfter(std::size_t count) const {
        return {mpv_.commands.begin() + static_cast<std::ptrdiff_t>(count), mpv_.commands.end()};
    }

    asio::io_context io_;
    std::filesystem::path path_;
    FakeMpv mpv_;
};

TEST_F(MpvClientTest, OnConnectObservesTheTitleAndReplaysTheState) {
    MpvClient client(io_, path_);
    client.mute();  // As PlayerController::start does, before the socket is up.
    ASSERT_TRUE(runUntil([&] { return mpv_.commands.size() >= 3; }));
    settle();
    EXPECT_EQ(mpv_.commands, (std::vector<json>{kObserveTitle, setProperty("pause", false), setProperty("aid", "no")}));
}

TEST_F(MpvClientTest, TuneLoadsWithTheAudioTrackSelected) {
    MpvClient client(io_, path_);
    ASSERT_TRUE(runUntil([&] { return mpv_.commands.size() >= 2; }));
    settle();
    const auto before = mpv_.commands.size();
    client.tune(kStation);
    ASSERT_TRUE(runUntil([&] { return mpv_.commands.size() >= before + 3; }));
    EXPECT_EQ(commandsAfter(before),
              (std::vector<json>{setProperty("pause", false), setProperty("aid", "auto"), loadfile(kStation)}));
}

TEST_F(MpvClientTest, TuneWhileMutedWaitsForUnmute) {
    MpvClient client(io_, path_);
    client.mute();
    ASSERT_TRUE(runUntil([&] { return mpv_.commands.size() >= 3; }));
    settle();
    const auto before = mpv_.commands.size();
    client.tune(kStation);
    settle();
    EXPECT_EQ(mpv_.commands.size(), before);

    client.unmute();
    ASSERT_TRUE(runUntil([&] { return mpv_.commands.size() >= before + 3; }));
    EXPECT_EQ(commandsAfter(before),
              (std::vector<json>{setProperty("pause", false), setProperty("aid", "auto"), loadfile(kStation)}));
}

TEST_F(MpvClientTest, MuteDeselectsTheTrackAndUnmuteReloadsKeepingPause) {
    MpvClient client(io_, path_);
    client.tune(kStation);
    ASSERT_TRUE(runUntil([&] { return mpv_.commands.size() >= 5; }));
    settle();
    const auto before = mpv_.commands.size();
    client.pause();
    client.mute();
    client.unmute();
    ASSERT_TRUE(runUntil([&] { return mpv_.commands.size() >= before + 5; }));
    EXPECT_EQ(commandsAfter(before),
              (std::vector<json>{setProperty("pause", true), setProperty("aid", "no"), setProperty("pause", true),
                                 setProperty("aid", "auto"), loadfile(kStation)}));
}

TEST_F(MpvClientTest, ResumeUnpauses) {
    MpvClient client(io_, path_);
    ASSERT_TRUE(runUntil([&] { return mpv_.commands.size() >= 2; }));
    settle();
    const auto before = mpv_.commands.size();
    client.resume();
    ASSERT_TRUE(runUntil([&] { return mpv_.commands.size() >= before + 1; }));
    EXPECT_EQ(commandsAfter(before), (std::vector<json>{setProperty("pause", false)}));
}

TEST_F(MpvClientTest, ReportsTheStreamTitle) {
    MpvClient client(io_, path_);
    RecordingListener listener;
    client.setListener(&listener);
    ASSERT_TRUE(runUntil([&] { return mpv_.connections == 1; }));
    mpv_.send({{"event", "property-change"}, {"id", 1}, {"name", "metadata/by-key/icy-title"},
               {"data", "Motörhead - Ace of Spades"}});
    mpv_.send({{"request_id", 0}, {"error", "success"}});  // A command's reply: not a title.
    mpv_.send({{"event", "property-change"}, {"id", 1}, {"name", "metadata/by-key/icy-title"}});
    ASSERT_TRUE(runUntil([&] { return listener.titles.size() >= 2; }));
    EXPECT_EQ(listener.titles, (std::vector<std::string>{"Motörhead - Ace of Spades", ""}));
}

TEST_F(MpvClientTest, ReconnectsAndReplaysTheStation) {
    MpvClient client(io_, path_);
    RecordingListener listener;
    client.setListener(&listener);
    client.tune(kStation);
    ASSERT_TRUE(runUntil([&] { return mpv_.commands.size() >= 5; }));
    settle();

    mpv_.hangUp();
    const auto before = mpv_.commands.size();
    ASSERT_TRUE(runUntil([&] { return mpv_.connections == 2 && mpv_.commands.size() >= before + 5; }));
    EXPECT_EQ(commandsAfter(before), (std::vector<json>{kObserveTitle, setProperty("pause", false),
                                                        setProperty("pause", false), setProperty("aid", "auto"),
                                                        loadfile(kStation)}));
    EXPECT_EQ(listener.titles, (std::vector<std::string>{""}));  // Cleared while it was gone.
}

TEST_F(MpvClientTest, WaitsQuietlyWhileMpvIsMissing) {
    const auto elsewhere = path_.string() + ".missing";
    MpvClient client(io_, elsewhere);
    client.tune(kStation);
    client.pause();
    settle();
    EXPECT_EQ(mpv_.connections, 0);
}

}  // namespace
}  // namespace winampdeck
