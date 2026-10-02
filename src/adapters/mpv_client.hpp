#pragma once

#include <asio/io_context.hpp>
#include <asio/local/stream_protocol.hpp>
#include <asio/steady_timer.hpp>
#include <asio/streambuf.hpp>

#include <chrono>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>

#include "core/radio_client.hpp"

namespace winampdeck {

// The Internet Radio Engine: an mpv started with --idle and
// --input-ipc-server, driven over that socket with newline-delimited JSON
// (docs/pi-setup.md, Phases 3 and 4). mpv runs as its own process, so it
// connects and reconnects to the socket; while it can't, commands are dropped
// and the state they set is replayed on the next connect.
//
// What Phase 4 found shapes how it's driven (ADR 0004):
// - Muting deselects the audio track (`aid no`), which releases the device.
// - A muted stream isn't read, and its server soon drops it, so unmuting
//   loads the Station again rather than only reselecting the track.
// - `aid no` also silences the next `loadfile`, so a Station tuned while
//   muted is only loaded on unmute, and every load selects the track first.
//
// The stream title is mpv's ICY metadata (`metadata/by-key/icy-title`).
// Everything runs on the io_context's thread.
class MpvClient final : public RadioClient {
public:
    static constexpr std::chrono::seconds kReconnectDelay{1};

    MpvClient(asio::io_context& io, std::filesystem::path socket);
    ~MpvClient() override;

    MpvClient(const MpvClient&) = delete;
    MpvClient& operator=(const MpvClient&) = delete;

    void setListener(Listener* listener) override;
    void tune(const Station& station) override;
    void pause() override;
    void resume() override;
    void mute() override;
    void unmute() override;

private:
    void connect();
    void onConnected();
    void onLost(const std::string& why);
    void readLine();
    void onLine(const std::string& line);

    // Selects the audio track and loads the Station, keeping the pause state.
    void load();
    void command(std::string line);
    void writeNext();

    std::filesystem::path path_;
    asio::local::stream_protocol::socket socket_;
    asio::steady_timer reconnect_;
    asio::streambuf input_;
    std::deque<std::string> output_;  // Lines waiting to be written; the front is being written.
    bool connected_ = false;
    bool reportedDown_ = false;
    Listener* listener_ = nullptr;

    // What mpv should be doing, replayed on every connect.
    std::optional<Station> station_;
    bool paused_ = false;
    bool muted_ = false;
};

}  // namespace winampdeck
