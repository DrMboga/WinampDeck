// Renders the TFT's screens to .bmp files, through the real TftView, so the
// look can be checked (and tweaked) on any machine, without a Pi or a panel.
//
//   winampdeck-tft-preview DATA_DIR OUT_DIR [--cover URL] [--scale N]
//
// DATA_DIR holds stations.csv and logos/ (the repo's data/ directory). With
// --cover, the Spotify screens show that album cover, downloaded and converted
// exactly as on the Deck. --scale enlarges each pixel (default 3).

#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>

#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "adapters/deck_artwork.hpp"
#include "core/stations_csv.hpp"
#include "ui/tft_view.hpp"

namespace {

using namespace winampdeck;
using namespace std::chrono_literals;

// Timers that fire only when the preview moves time on.
class StepScheduler final : public Scheduler {
public:
    std::chrono::milliseconds now() const override { return now_; }
    TimerId callAfter(std::chrono::milliseconds delay, std::function<void()> callback) override {
        timers_.emplace(nextId_, Timer{now_ + delay, std::move(callback)});
        return nextId_++;
    }
    void cancel(TimerId timer) override { timers_.erase(timer); }

    void advance(std::chrono::milliseconds by) {
        const auto until = now_ + by;
        while (true) {
            auto next = timers_.end();
            for (auto it = timers_.begin(); it != timers_.end(); ++it) {
                if (it->second.due <= until && (next == timers_.end() || it->second.due < next->second.due)) {
                    next = it;
                }
            }
            if (next == timers_.end()) {
                break;
            }
            now_ = next->second.due;
            auto callback = std::move(next->second.callback);
            timers_.erase(next);
            callback();
        }
        now_ = until;
    }

private:
    struct Timer {
        std::chrono::milliseconds due;
        std::function<void()> callback;
    };
    std::chrono::milliseconds now_{0};
    TimerId nextId_ = 1;
    std::map<TimerId, Timer> timers_;
};

// Keeps a copy of what's on the glass.
class MemoryDisplay final : public ui::Display {
public:
    ui::Image glass{kWidth, kHeight};

    void write(const ui::Rect& area, std::span<const ui::Color> pixels) override {
        std::size_t i = 0;
        for (int y = area.y; y < area.bottom(); ++y) {
            for (int x = area.x; x < area.right(); ++x) {
                glass.at(x, y) = pixels[i++];
            }
        }
    }
};

void writeBmp(const std::filesystem::path& path, const ui::Image& image, int scale) {
    const int width = image.width * scale;
    const int height = image.height * scale;
    const int rowBytes = (width * 3 + 3) & ~3;
    const auto pixelBytes = static_cast<std::uint32_t>(rowBytes * height);

    std::vector<std::uint8_t> file;
    auto put16 = [&file](std::uint32_t v) {
        file.push_back(static_cast<std::uint8_t>(v));
        file.push_back(static_cast<std::uint8_t>(v >> 8));
    };
    auto put32 = [&put16](std::uint32_t v) {
        put16(v & 0xFFFF);
        put16(v >> 16);
    };
    file.push_back('B');
    file.push_back('M');
    put32(54 + pixelBytes);
    put32(0);
    put32(54);
    put32(40);
    put32(static_cast<std::uint32_t>(width));
    put32(static_cast<std::uint32_t>(height));
    put16(1);
    put16(24);
    put32(0);
    put32(pixelBytes);
    put32(2835);
    put32(2835);
    put32(0);
    put32(0);
    for (int y = height - 1; y >= 0; --y) {  // Bottom row first.
        for (int x = 0; x < width; ++x) {
            const ui::Rgb888 c = ui::toRgb888(image.at(x / scale, y / scale));
            file.insert(file.end(), {c.b, c.g, c.r});
        }
        file.resize(file.size() + static_cast<std::size_t>(rowBytes - width * 3), 0);
    }
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(file.data()),
                                                static_cast<std::streamsize>(file.size()));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " DATA_DIR OUT_DIR [--cover URL] [--scale N]\n";
        return 2;
    }
    const std::filesystem::path dataDir = argv[1];
    const std::filesystem::path outDir = argv[2];
    std::string coverUrl;
    int scale = 3;
    for (int i = 3; i + 1 < argc; i += 2) {
        const std::string_view option = argv[i];
        if (option == "--cover") {
            coverUrl = argv[i + 1];
        } else if (option == "--scale") {
            scale = std::stoi(argv[i + 1]);
        }
    }

    try {
        const auto stations = loadStationsCsv(dataDir / "stations.csv");
        std::filesystem::create_directories(outDir);

        asio::io_context io;
        StepScheduler scheduler;
        MemoryDisplay display;
        DeckArtwork artwork(io, dataDir / "logos");

        if (!coverUrl.empty()) {
            // Fetch it up front, waiting for the event loop to deliver it.
            bool loaded = false;
            artwork.setOnCoverLoaded([&] { loaded = true; });
            artwork.cover(coverUrl);
            auto work = asio::make_work_guard(io);
            while (!loaded && io.run_one_for(15s) > 0) {
            }
            if (artwork.cover(coverUrl) == nullptr) {
                std::cerr << "warning: the cover didn't load; the placeholder is shown instead\n";
            }
        }
        // Constructed after the fetch: it takes over the cover callback.
        ui::TftView view(display, artwork, scheduler);

        auto snapshot = [&](const std::string& name) {
            writeBmp(outDir / (name + ".bmp"), display.glass, scale);
            std::cout << "wrote " << (outDir / (name + ".bmp")).string() << '\n';
        };

        NowPlayingScreen stopped;
        view.show(stopped);
        snapshot("1-stopped");

        NowPlayingScreen waiting;
        waiting.source = Source::Spotify;
        view.show(waiting);
        snapshot("2-spotify-waiting");

        NowPlayingScreen spotify;
        spotify.source = Source::Spotify;
        spotify.status = PlaybackStatus::Playing;
        spotify.artist = "Daft Punk";
        spotify.title = "Harder, Better, Faster, Stronger";
        spotify.coverUrl = coverUrl;
        spotify.position = 83s;
        spotify.duration = 224s;
        view.show(spotify);
        scheduler.advance(1200ms);
        snapshot("3-spotify-playing");
        scheduler.advance(2000ms);
        snapshot("4-spotify-playing-scrolled");

        spotify.status = PlaybackStatus::Paused;
        view.show(spotify);
        scheduler.advance(2000ms);
        snapshot("5-spotify-paused");

        NowPlayingScreen radio;
        radio.source = Source::Radio;
        radio.status = PlaybackStatus::Playing;
        const Station& tuned = stations.at(std::min<std::size_t>(23, stations.size() - 1));
        radio.station = tuned.name;
        radio.logo = tuned.logo;
        radio.title = "Кино — Группа крови";
        view.show(radio);
        scheduler.advance(754'000ms);
        snapshot("6-radio-playing");

        StationListScreen list{stations, 23, 23};
        view.show(list);
        snapshot("7-station-list");
        list.selected = 25;
        view.show(list);
        snapshot("8-station-list-moved");
        list.selected = 0;
        view.show(list);
        snapshot("9-station-list-top");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
