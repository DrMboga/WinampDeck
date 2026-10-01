#pragma once

#include <asio/io_context.hpp>

#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>

#include "adapters/cover_art.hpp"
#include "ui/artwork.hpp"

namespace winampdeck {

// The TFT's pictures: Station logos read from the logos directory (each
// 92×92 raw RGB565, see ui::loadRgb565File), and Spotify covers downloaded and
// converted on demand. Logos are read once, on first use; the last few covers
// are kept so skipping back and forth doesn't download them again.
class DeckArtwork final : public ui::Artwork {
public:
    static constexpr std::size_t kCachedCovers = 8;

    DeckArtwork(asio::io_context& io, std::filesystem::path logoDirectory);

    const ui::Image* logo(const std::string& filename) override;
    const ui::Image* logoThumbnail(const std::string& filename) override;
    const ui::Image* cover(const std::string& url) override;
    void setOnCoverLoaded(std::function<void()> callback) override;

private:
    void onCoverFetched(const std::string& url, std::optional<ui::Image> cover);

    std::filesystem::path logoDirectory_;
    // Failed loads are remembered too (as nothing), so they're tried once.
    std::map<std::string, std::optional<ui::Image>> logos_;
    std::map<std::string, std::optional<ui::Image>> thumbnails_;
    std::map<std::string, std::optional<ui::Image>> covers_;
    std::deque<std::string> coverOrder_;  // Oldest first.
    std::optional<std::string> fetching_;
    std::function<void()> onCoverLoaded_;
    // Last, so it's destroyed (and its thread stopped) before the rest.
    CoverArtFetcher fetcher_;
};

}  // namespace winampdeck
