#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

#include "core/hardware_io.hpp"
#include "core/scheduler.hpp"
#include "ui/artwork.hpp"
#include "ui/canvas.hpp"
#include "ui/display.hpp"

namespace winampdeck::ui {

// Draws PlayerController's Screen on the TFT, Winamp style:
//
// - Now Playing: the album cover or Station logo, a play/pause/stop indicator,
//   the elapsed time in big digits, a decorative spectrum, Spotify's progress
//   bar, and two lines of text that scroll when they don't fit.
// - Station List: the Stations with logo thumbnails, the highlighted one in
//   Winamp's playlist blue and the tuned one in white.
//
// Between Screen changes it keeps animating on its own timer (spectrum,
// clock, scrolling) for as long as anything is moving. Each frame is drawn
// whole in memory, and only the parts that changed are sent to the Display.
// Must be driven from the event loop's thread, like everything on Scheduler.
class TftView {
public:
    static constexpr std::chrono::milliseconds kFrameInterval{50};
    // Scrolling text moves one pixel per step, after a pause at the start.
    static constexpr std::chrono::milliseconds kScrollStep{50};
    static constexpr std::chrono::milliseconds kScrollDelay{1500};
    static constexpr int kSpectrumBars = 15;

    TftView(Display& display, Artwork& artwork, Scheduler& scheduler);
    ~TftView();

    TftView(const TftView&) = delete;
    TftView& operator=(const TftView&) = delete;

    void show(const Screen& screen);

private:
    // Text that scrolls when it doesn't fit, and when it started.
    struct ScrollingLine {
        std::u32string text;
        std::chrono::milliseconds since{0};
    };

    struct Spectrum {
        std::array<int, kSpectrumBars> bars{};
        std::array<int, kSpectrumBars> peaks{};
        std::array<int, kSpectrumBars> peakHold{};
        std::uint32_t random = 0x2545F491;

        void step(bool playing, int height);
        bool idle() const;
    };

    void trackTime(const NowPlayingScreen& next);
    std::chrono::milliseconds elapsed() const;

    void render();
    void flush();
    void scheduleFrame();
    void onFrame();
    bool animating() const;

    void drawNowPlaying(const NowPlayingScreen& screen);
    void drawStationList(const StationListScreen& screen);
    void drawArt(const Rect& area, const Image* image, std::string_view placeholder);
    void drawStatus(int x, int y, PlaybackStatus status);
    void drawSpectrum(const Rect& area);
    void drawProgress(const Rect& area, const NowPlayingScreen& screen);
    // Draws one line of text in `area`, scrolling it if it's too long.
    // Returns whether it had to scroll.
    bool drawLine(const Rect& area, ScrollingLine& line, std::string_view text, Color color);
    void drawTruncated(int x, int y, int width, std::string_view text, Color color);

    Display& display_;
    Artwork& artwork_;
    Scheduler& scheduler_;

    Screen screen_;
    Image frame_;
    Image shown_;
    bool shownValid_ = false;
    Canvas canvas_;
    std::optional<Scheduler::TimerId> frameTimer_;

    // The clock: Spotify's last reported position, or how long Radio has
    // played the current Station, as of `clockSince_`.
    std::optional<NowPlayingScreen> lastNowPlaying_;
    std::chrono::milliseconds clockBase_{0};
    std::chrono::milliseconds clockSince_{0};
    bool clockRunning_ = false;

    Spectrum spectrum_;
    std::array<ScrollingLine, 3> lines_;  // Now Playing's two, the Station List's highlight.
    bool scrolling_ = false;              // Whether the last frame had a line scrolling.
};

}  // namespace winampdeck::ui
