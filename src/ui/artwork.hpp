#pragma once

#include <functional>
#include <string>

#include "ui/image.hpp"

namespace winampdeck::ui {

// The pictures the TFT shows: Station logos (files next to stations.csv) and
// Spotify album covers (downloaded and converted at runtime). Every returned
// pointer is only valid until the next call.
class Artwork {
public:
    // Logos and covers are both drawn 92×92.
    static constexpr int kSize = 92;
    // Logos in the Station List.
    static constexpr int kThumbnailSize = 21;

    virtual ~Artwork() = default;

    // nullptr if there's no such logo, or it can't be read.
    virtual const Image* logo(const std::string& filename) = 0;
    virtual const Image* logoThumbnail(const std::string& filename) = 0;

    // The cover if it's already been fetched. Otherwise nullptr, and it's
    // fetched in the background: `onCoverLoaded` is called, on the event loop,
    // once it's ready.
    virtual const Image* cover(const std::string& url) = 0;
    virtual void setOnCoverLoaded(std::function<void()> callback) = 0;
};

}  // namespace winampdeck::ui
