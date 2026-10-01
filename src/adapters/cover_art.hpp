#pragma once

#include <asio/io_context.hpp>

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>

#include "ui/image.hpp"

namespace winampdeck {

// Where to actually download a cover from. cpp-httplib is built without TLS
// (ADR 0003), and Spotify's image CDN (i.scdn.co) serves the same images over
// plain HTTP, so an https:// URL is fetched as http://.
struct CoverDownload {
    std::string host;
    int port = 80;
    std::string path;

    bool operator==(const CoverDownload&) const = default;
};

// Also swaps Spotify's 640×640 cover for its 300×300 variant: plenty for
// 92×92, and much less for the Pi to download and decode. Returns nothing for
// anything that isn't an http(s) URL.
std::optional<CoverDownload> coverDownload(std::string_view url);

// Decodes a JPEG cover, crops it to a centred square, and scales it to the
// TFT's 92×92 artwork size. Returns nothing if it can't be decoded.
std::optional<ui::Image> decodeCover(std::span<const std::uint8_t> jpeg);

// Downloads and decodes covers on a background thread, so the event loop
// never waits on the network. Only the most recently requested cover matters:
// a request still waiting to start is replaced by a newer one.
class CoverArtFetcher {
public:
    // Called on the io_context's thread, with nothing if the cover couldn't be
    // downloaded or decoded.
    using Callback = std::function<void(const std::string& url, std::optional<ui::Image> cover)>;

    CoverArtFetcher(asio::io_context& io, Callback onFetched);
    // Waits for a download in progress to finish or time out.
    ~CoverArtFetcher();

    CoverArtFetcher(const CoverArtFetcher&) = delete;
    CoverArtFetcher& operator=(const CoverArtFetcher&) = delete;

    void fetch(const std::string& url);

private:
    void run();

    asio::io_context& io_;
    // Shared with the results posted to the event loop, so a result that
    // arrives after destruction finds it gone.
    std::shared_ptr<Callback> onFetched_;

    std::mutex mutex_;
    std::condition_variable wake_;
    std::optional<std::string> pending_;
    bool stopping_ = false;
    std::thread worker_;
};

}  // namespace winampdeck
