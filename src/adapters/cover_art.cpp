#include "adapters/cover_art.hpp"

#include <httplib.h>
#include <stb_image.h>

#include <asio/post.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <iostream>
#include <utility>

#include "ui/artwork.hpp"

namespace winampdeck {

namespace {

// Spotify image IDs start with a size code: ab67616d0000b273 is the 640×640
// album cover, ab67616d00001e02 the same cover at 300×300.
constexpr std::string_view kCover640 = "ab67616d0000b273";
constexpr std::string_view kCover300 = "ab67616d00001e02";

constexpr std::chrono::seconds kConnectTimeout{5};
constexpr std::chrono::seconds kReadTimeout{10};
constexpr int kAttempts = 2;

// Downloads over IPv4 only. On the Deck's home network, connections to the
// image CDN over IPv6 were reset more often than not (2026-10-01: 3 of 8
// succeeded, against 12 of 12 over IPv4).
httplib::Result download(const CoverDownload& where) {
    httplib::Client client(where.host, where.port);
    client.set_address_family(AF_INET);
    client.set_connection_timeout(kConnectTimeout);
    client.set_read_timeout(kReadTimeout);
    client.set_follow_location(true);
    return client.Get(where.path);
}

}  // namespace

std::optional<CoverDownload> coverDownload(std::string_view url) {
    std::string_view rest;
    if (url.starts_with("https://")) {
        rest = url.substr(8);
    } else if (url.starts_with("http://")) {
        rest = url.substr(7);
    } else {
        return std::nullopt;
    }

    const auto slash = rest.find('/');
    std::string_view authority = rest.substr(0, slash);
    CoverDownload download;
    download.path = slash == std::string_view::npos ? "/" : std::string(rest.substr(slash));

    if (const auto colon = authority.find(':'); colon != std::string_view::npos) {
        const std::string_view port = authority.substr(colon + 1);
        const auto [end, error] = std::from_chars(port.data(), port.data() + port.size(), download.port);
        if (error != std::errc{} || end != port.data() + port.size()) {
            return std::nullopt;
        }
        authority = authority.substr(0, colon);
    }
    if (authority.empty()) {
        return std::nullopt;
    }
    download.host = authority;

    if (const auto at = download.path.find(kCover640); at != std::string::npos) {
        download.path.replace(at, kCover640.size(), kCover300);
    }
    return download;
}

std::optional<ui::Image> decodeCover(std::span<const std::uint8_t> jpeg) {
    int width = 0;
    int height = 0;
    int channels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(jpeg.data(), static_cast<int>(jpeg.size()), &width, &height, &channels, 3),
        &stbi_image_free);
    if (!pixels) {
        return std::nullopt;
    }

    // Crop to the centred square, then scale.
    const int side = std::min(width, height);
    const int left = (width - side) / 2;
    const int top = (height - side) / 2;
    std::vector<std::uint8_t> square;
    square.reserve(static_cast<std::size_t>(side * side * 3));
    for (int y = top; y < top + side; ++y) {
        const stbi_uc* row = pixels.get() + (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                             static_cast<std::size_t>(left)) * 3;
        square.insert(square.end(), row, row + side * 3);
    }
    return ui::resampleRgb888(square, side, side, ui::Artwork::kSize, ui::Artwork::kSize);
}

CoverArtFetcher::CoverArtFetcher(asio::io_context& io, Callback onFetched)
    : io_(io), onFetched_(std::make_shared<Callback>(std::move(onFetched))), worker_([this] { run(); }) {}

CoverArtFetcher::~CoverArtFetcher() {
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_one();
    worker_.join();
}

void CoverArtFetcher::fetch(const std::string& url) {
    {
        std::lock_guard lock(mutex_);
        pending_ = url;
    }
    wake_.notify_one();
}

void CoverArtFetcher::run() {
    while (true) {
        std::string url;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || pending_; });
            if (stopping_) {
                return;
            }
            url = std::move(*pending_);
            pending_.reset();
        }

        std::optional<ui::Image> cover;
        if (const auto where = coverDownload(url)) {
            auto response = download(*where);
            for (int attempt = 1; attempt < kAttempts && !response; ++attempt) {
                response = download(*where);
            }
            if (!response) {
                std::cerr << "Cover " << url << ": " << httplib::to_string(response.error()) << std::endl;
            } else if (response->status != 200) {
                std::cerr << "Cover " << url << ": HTTP " << response->status << std::endl;
            } else {
                const auto* bytes = reinterpret_cast<const std::uint8_t*>(response->body.data());
                cover = decodeCover({bytes, response->body.size()});
                if (!cover) {
                    std::cerr << "Cover " << url << ": not a JPEG we can decode" << std::endl;
                }
            }
        } else {
            std::cerr << "Cover " << url << ": not an http(s) URL" << std::endl;
        }

        asio::post(io_, [weak = std::weak_ptr<Callback>(onFetched_), url = std::move(url),
                         cover = std::move(cover)]() mutable {
            if (const auto callback = weak.lock()) {
                (*callback)(url, std::move(cover));
            }
        });
    }
}

}  // namespace winampdeck
