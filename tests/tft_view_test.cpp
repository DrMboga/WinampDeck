// The TFT view: what reaches the glass for each Screen, and how it animates.

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "fakes.hpp"
#include "ui/canvas.hpp"
#include "ui/tft_view.hpp"

namespace winampdeck::ui {
namespace {

using namespace std::chrono_literals;
using winampdeck::testing::ManualScheduler;

constexpr Color kGreen = rgb(0, 230, 0);
constexpr Color kSelected = rgb(0, 0, 198);
constexpr Color kLogoColor = rgb(200, 40, 40);
constexpr Color kCoverColor = rgb(40, 40, 200);
constexpr Rect kClockArea{98, 16, 60, 14};
constexpr Rect kSpectrumArea{98, 37, 60, 57};
constexpr Rect kLine2Area{2, 117, 156, 7};

// Keeps what's on the glass, and every write that put it there.
class RecordingDisplay final : public Display {
public:
    Image glass{kWidth, kHeight};
    std::vector<Rect> writes;

    void write(const Rect& area, std::span<const Color> pixels) override {
        writes.push_back(area);
        std::size_t i = 0;
        for (int y = area.y; y < area.bottom(); ++y) {
            for (int x = area.x; x < area.right(); ++x) {
                glass.at(x, y) = pixels[i++];
            }
        }
    }
};

class FakeArtwork final : public Artwork {
public:
    std::map<std::string, Image> logos;
    std::map<std::string, Image> covers;
    std::vector<std::string> coversAskedFor;

    const Image* logo(const std::string& filename) override { return find(logos, filename); }
    const Image* logoThumbnail(const std::string& filename) override {
        if (!logos.contains(filename)) {
            return nullptr;
        }
        thumbnail_ = Image(kThumbnailSize, kThumbnailSize, logos[filename].pixels[0]);
        return &*thumbnail_;
    }
    const Image* cover(const std::string& url) override {
        coversAskedFor.push_back(url);
        return find(covers, url);
    }
    void setOnCoverLoaded(std::function<void()> callback) override { onCoverLoaded = std::move(callback); }

    void deliverCover(const std::string& url, const Image& image) {
        covers[url] = image;
        onCoverLoaded();
    }

    std::function<void()> onCoverLoaded;

private:
    static const Image* find(std::map<std::string, Image>& images, const std::string& key) {
        const auto it = images.find(key);
        return it == images.end() ? nullptr : &it->second;
    }
    std::optional<Image> thumbnail_;
};

std::vector<Station> manyStations(int count) {
    std::vector<Station> stations;
    for (int i = 0; i < count; ++i) {
        stations.push_back({"Station " + std::to_string(i), "http://s" + std::to_string(i), ""});
    }
    return stations;
}

class TftViewTest : public ::testing::Test {
protected:
    TftViewTest() {
        artwork.logos["rock.565"] = Image(Artwork::kSize, Artwork::kSize, kLogoColor);
    }

    // The clock as TftView draws it: green, double size, right-aligned.
    Image clockReading(const std::string& text) const {
        Image expected(kClockArea.width, kClockArea.height);
        Canvas canvas(expected);
        canvas.text(kClockArea.width - (Canvas::textWidth(text.size(), 2) - 2), 0, text, kGreen, 2);
        return expected;
    }

    // Moves time on a frame at a time, so every animation frame is drawn.
    void runFor(std::chrono::milliseconds duration) {
        for (auto t = 0ms; t < duration; t += TftView::kFrameInterval) {
            scheduler.advance(TftView::kFrameInterval);
        }
    }

    Image region(const Rect& area) const {
        Image out(area.width, area.height);
        for (int y = 0; y < area.height; ++y) {
            for (int x = 0; x < area.width; ++x) {
                out.at(x, y) = display.glass.at(area.x + x, area.y + y);
            }
        }
        return out;
    }

    bool blank(const Rect& area) const {
        const Image pixels = region(area);
        return std::ranges::all_of(pixels.pixels, [](Color c) { return c == 0; });
    }

    static NowPlayingScreen radio(const std::string& station, PlaybackStatus status = PlaybackStatus::Playing) {
        NowPlayingScreen screen;
        screen.source = Source::Radio;
        screen.status = status;
        screen.station = station;
        screen.logo = "rock.565";
        return screen;
    }

    static NowPlayingScreen spotify(PlaybackStatus status, std::chrono::milliseconds position) {
        NowPlayingScreen screen;
        screen.source = Source::Spotify;
        screen.status = status;
        screen.artist = "Artist";
        screen.title = "Title";
        screen.coverUrl = "http://i.scdn.co/image/cover";
        screen.position = position;
        screen.duration = 200s;
        return screen;
    }

    RecordingDisplay display;
    FakeArtwork artwork;
    ManualScheduler scheduler;
    TftView view{display, artwork, scheduler};
};

TEST_F(TftViewTest, TheFirstScreenIsSentWhole) {
    view.show(NowPlayingScreen{});

    EXPECT_EQ(display.writes, (std::vector<Rect>{{0, 0, 160, 128}}));
}

TEST_F(TftViewTest, ShowingTheSameScreenAgainSendsNothing) {
    view.show(NowPlayingScreen{});
    display.writes.clear();

    view.show(NowPlayingScreen{});

    EXPECT_TRUE(display.writes.empty());
}

TEST_F(TftViewTest, OnlyWhatChangedIsSent) {
    view.show(radio("Rock Antenne"));
    display.writes.clear();

    NowPlayingScreen withTitle = radio("Rock Antenne");
    withTitle.title = "Metallica - One";
    view.show(withTitle);

    ASSERT_FALSE(display.writes.empty());
    for (const Rect& area : display.writes) {
        EXPECT_FALSE(area.intersect(kLine2Area).empty());
        EXPECT_GE(area.y, 112);
    }
}

TEST_F(TftViewTest, TheStationLogoIsShown) {
    view.show(radio("Rock Antenne"));

    EXPECT_EQ(display.glass.at(2, 2), kLogoColor);
    EXPECT_EQ(display.glass.at(93, 93), kLogoColor);
}

TEST_F(TftViewTest, TheAlbumCoverIsShownOnceItArrives) {
    view.show(spotify(PlaybackStatus::Playing, 0s));
    EXPECT_EQ(artwork.coversAskedFor.back(), "http://i.scdn.co/image/cover");
    EXPECT_NE(display.glass.at(40, 40), kCoverColor);

    artwork.deliverCover("http://i.scdn.co/image/cover", Image(92, 92, kCoverColor));

    EXPECT_EQ(display.glass.at(40, 40), kCoverColor);
}

TEST_F(TftViewTest, TheSpotifyClockRunsBetweenEngineReports) {
    view.show(spotify(PlaybackStatus::Playing, 30s));
    EXPECT_EQ(region(kClockArea), clockReading(" 0:30"));

    scheduler.advance(61s);
    EXPECT_EQ(region(kClockArea), clockReading(" 1:31"));

    // The Engine's own position wins as soon as it reports one.
    view.show(spotify(PlaybackStatus::Playing, 10s));
    EXPECT_EQ(region(kClockArea), clockReading(" 0:10"));
}

TEST_F(TftViewTest, ThePausedClockStopsAndBlinks) {
    view.show(spotify(PlaybackStatus::Playing, 0s));
    scheduler.advance(5s);
    view.show(spotify(PlaybackStatus::Paused, 0s));  // No new position reported.

    scheduler.advance(10s);  // On a blink-on half second.
    EXPECT_EQ(region(kClockArea), clockReading(" 0:05"));
    scheduler.advance(500ms);
    EXPECT_TRUE(blank(kClockArea));
}

TEST_F(TftViewTest, TheClockStopsAtTheEndOfTheTrack) {
    view.show(spotify(PlaybackStatus::Playing, 195s));
    scheduler.advance(30s);
    EXPECT_EQ(region(kClockArea), clockReading(" 3:20"));
}

TEST_F(TftViewTest, TheRadioClockCountsFromTuningTheStation) {
    view.show(radio("Alpha"));
    scheduler.advance(90s);
    EXPECT_EQ(region(kClockArea), clockReading(" 1:30"));

    NowPlayingScreen newTitle = radio("Alpha");
    newTitle.title = "Next song";
    view.show(newTitle);
    scheduler.advance(10s);
    EXPECT_EQ(region(kClockArea), clockReading(" 1:40"));

    view.show(radio("Beta"));
    EXPECT_EQ(region(kClockArea), clockReading(" 0:00"));
}

TEST_F(TftViewTest, LongListeningShowsHours) {
    view.show(radio("Alpha"));
    scheduler.advance(2h + 5min);
    EXPECT_EQ(region(kClockArea), clockReading("2h05"));
}

TEST_F(TftViewTest, TheSpectrumMovesWhilePlayingAndSettlesWhenStopped) {
    view.show(radio("Alpha"));
    EXPECT_TRUE(blank(kSpectrumArea));

    runFor(500ms);
    EXPECT_FALSE(blank(kSpectrumArea));

    view.show(NowPlayingScreen{});  // Back to Stopped.
    runFor(5s);
    EXPECT_TRUE(blank(kSpectrumArea));
    EXPECT_EQ(scheduler.pending(), 0u);  // Nothing left to animate.
}

TEST_F(TftViewTest, AStoppedDeckDoesNotAnimate) {
    view.show(NowPlayingScreen{});
    EXPECT_EQ(scheduler.pending(), 0u);
}

TEST_F(TftViewTest, ALongLineScrolls) {
    NowPlayingScreen screen = radio("Alpha");
    screen.title = "A stream title far too long to fit on one line of the TFT";
    view.show(screen);
    const Image start = region(kLine2Area);

    scheduler.advance(TftView::kScrollDelay - 100ms);
    EXPECT_EQ(region(kLine2Area), start);  // Holds still first...

    scheduler.advance(500ms);
    EXPECT_NE(region(kLine2Area), start);  // ...then moves.
}

TEST_F(TftViewTest, TheStationListHighlightsTheSelectionInTheMiddleRow) {
    view.show(StationListScreen{manyStations(60), 10, 3});

    // Rows start at y=13 and are 23 high; the highlight is the third of five.
    EXPECT_EQ(display.glass.at(1, 13 + 2 * 23 + 1), kSelected);
    EXPECT_NE(display.glass.at(1, 13 + 1 * 23 + 1), kSelected);
    EXPECT_NE(display.glass.at(1, 13 + 3 * 23 + 1), kSelected);
}

TEST_F(TftViewTest, TheStationListHighlightStaysOnScreenAtTheEnds) {
    view.show(StationListScreen{manyStations(60), 0, 0});
    EXPECT_EQ(display.glass.at(1, 13 + 1), kSelected);

    view.show(StationListScreen{manyStations(60), 59, 0});
    EXPECT_EQ(display.glass.at(1, 13 + 4 * 23 + 1), kSelected);

    view.show(StationListScreen{manyStations(2), 1, 0});
    EXPECT_EQ(display.glass.at(1, 13 + 1 * 23 + 1), kSelected);
}

TEST_F(TftViewTest, TheStationListOnlyAnimatesALongHighlightedName) {
    auto stations = manyStations(3);
    view.show(StationListScreen{stations, 1, 1});
    EXPECT_EQ(scheduler.pending(), 0u);

    stations[1].name = "A Station name much too long for its row";
    view.show(StationListScreen{stations, 1, 1});
    EXPECT_EQ(scheduler.pending(), 1u);
}

}  // namespace
}  // namespace winampdeck::ui
