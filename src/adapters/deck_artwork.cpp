#include "adapters/deck_artwork.hpp"

#include <iostream>
#include <utility>

namespace winampdeck {

DeckArtwork::DeckArtwork(asio::io_context& io, std::filesystem::path logoDirectory)
    : logoDirectory_(std::move(logoDirectory)),
      fetcher_(io, [this](const std::string& url, std::optional<ui::Image> cover) {
          onCoverFetched(url, std::move(cover));
      }) {}

const ui::Image* DeckArtwork::logo(const std::string& filename) {
    auto it = logos_.find(filename);
    if (it == logos_.end()) {
        const auto path = logoDirectory_ / filename;
        auto image = ui::loadRgb565File(path, kSize, kSize);
        if (!image) {
            std::cerr << "Logo " << path.string() << ": missing, or not " << kSize << "x" << kSize
                      << " raw RGB565" << std::endl;
        }
        it = logos_.emplace(filename, std::move(image)).first;
    }
    return it->second ? &*it->second : nullptr;
}

const ui::Image* DeckArtwork::logoThumbnail(const std::string& filename) {
    auto it = thumbnails_.find(filename);
    if (it == thumbnails_.end()) {
        std::optional<ui::Image> thumbnail;
        if (const ui::Image* full = logo(filename)) {
            thumbnail = ui::resample(*full, kThumbnailSize, kThumbnailSize);
        }
        it = thumbnails_.emplace(filename, std::move(thumbnail)).first;
    }
    return it->second ? &*it->second : nullptr;
}

const ui::Image* DeckArtwork::cover(const std::string& url) {
    if (const auto it = covers_.find(url); it != covers_.end()) {
        return it->second ? &*it->second : nullptr;
    }
    if (fetching_ != url) {
        fetching_ = url;
        fetcher_.fetch(url);
    }
    return nullptr;
}

void DeckArtwork::setOnCoverLoaded(std::function<void()> callback) {
    onCoverLoaded_ = std::move(callback);
}

void DeckArtwork::onCoverFetched(const std::string& url, std::optional<ui::Image> cover) {
    if (fetching_ == url) {
        fetching_.reset();
    }
    if (!covers_.contains(url)) {
        coverOrder_.push_back(url);
        if (coverOrder_.size() > kCachedCovers) {
            covers_.erase(coverOrder_.front());
            coverOrder_.pop_front();
        }
    }
    covers_[url] = std::move(cover);
    if (onCoverLoaded_) {
        onCoverLoaded_();
    }
}

}  // namespace winampdeck
