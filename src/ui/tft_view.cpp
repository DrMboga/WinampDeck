#include "ui/tft_view.hpp"

#include <algorithm>
#include <cstdio>
#include <variant>

#include "ui/font5x7.hpp"

namespace winampdeck::ui {

using std::chrono::milliseconds;

namespace {

// Winamp's classic skin colours.
constexpr Color kBackground = rgb(0, 0, 0);
constexpr Color kGreen = rgb(0, 230, 0);       // Playlist text, the clock.
constexpr Color kWhite = rgb(224, 224, 224);   // The playing entry.
constexpr Color kSelected = rgb(0, 0, 198);    // Playlist selection.
constexpr Color kDim = rgb(42, 74, 42);        // Frames and separators.
constexpr Color kTrack = rgb(24, 48, 24);      // Empty part of bars.
constexpr Color kLabel = rgb(150, 150, 150);
constexpr Color kPlayIcon = rgb(0, 230, 0);
constexpr Color kPauseIcon = rgb(240, 192, 0);
constexpr Color kStopIcon = rgb(224, 96, 0);
constexpr Color kSeekKnob = rgb(232, 200, 0);  // Yellow, like the panel's seek bar knob.
constexpr Color kPlaceholder = rgb(16, 24, 16);
constexpr Color kPeak = rgb(150, 150, 150);

// Now Playing layout. The art sits top-left; the status, clock and spectrum
// in the column to its right; progress and two text lines along the bottom.
constexpr Rect kArtArea{2, 2, Artwork::kSize, Artwork::kSize};
constexpr int kColumnX = 98;
constexpr int kColumnRight = 158;
constexpr int kStatusY = 4;
constexpr int kClockY = 16;
constexpr int kClockScale = 2;
constexpr Rect kSpectrumArea{kColumnX, 37, 60, 57};
constexpr int kBarWidth = 3;  // Plus a 1-pixel gap.
constexpr Rect kProgressArea{2, 98, 156, 4};
constexpr Rect kLine1Area{2, 106, 156, kGlyphHeight};
constexpr Rect kLine2Area{2, 117, 156, kGlyphHeight};

// Station List layout.
constexpr int kHeaderHeight = 11;
constexpr int kRowsTop = 13;
constexpr int kRowHeight = 23;
constexpr int kVisibleRows = 5;
constexpr int kRowTextX = 27;
constexpr int kScrollbarX = 157;

constexpr milliseconds kBlinkPeriod{500};  // The clock blinks while paused, as in Winamp.

// Spectrum dynamics, in pixels per frame.
constexpr int kBarFall = 4;
constexpr int kPeakFall = 1;
constexpr int kPeakHoldFrames = 8;

std::string clockText(milliseconds elapsed) {
    const long seconds = static_cast<long>(elapsed.count() / 1000);
    char text[32];
    if (seconds < 100 * 60) {
        std::snprintf(text, sizeof text, "%2ld:%02ld", seconds / 60, seconds % 60);
    } else {
        std::snprintf(text, sizeof text, "%ldh%02ld", seconds / 3600, (seconds / 60) % 60);
    }
    return text;
}

// Green at the bottom of a bar, through yellow, to red at the top.
Color spectrumColor(int fromBottom, int height) {
    const int t = fromBottom * 255 / std::max(1, height - 1);
    if (t < 150) {
        return rgb(static_cast<std::uint8_t>(t * 230 / 150), static_cast<std::uint8_t>(200 + t * 20 / 150), 0);
    }
    return rgb(230, static_cast<std::uint8_t>(220 - (t - 150) * 180 / 105), 0);
}

}  // namespace

TftView::TftView(Display& display, Artwork& artwork, Scheduler& scheduler)
    : display_(display),
      artwork_(artwork),
      scheduler_(scheduler),
      frame_(Display::kWidth, Display::kHeight),
      shown_(Display::kWidth, Display::kHeight),
      canvas_(frame_) {
    artwork_.setOnCoverLoaded([this] {
        render();
        flush();
        scheduleFrame();
    });
}

TftView::~TftView() {
    artwork_.setOnCoverLoaded({});
    if (frameTimer_) {
        scheduler_.cancel(*frameTimer_);
    }
}

void TftView::show(const Screen& screen) {
    if (const auto* nowPlaying = std::get_if<NowPlayingScreen>(&screen)) {
        trackTime(*nowPlaying);
    }
    screen_ = screen;
    render();
    flush();
    scheduleFrame();
}

// --- The clock ---------------------------------------------------------------------

void TftView::trackTime(const NowPlayingScreen& next) {
    const milliseconds now = scheduler_.now();
    const auto& last = lastNowPlaying_;
    const bool sameSource = last && last->source == next.source;

    if (next.source == Source::Spotify) {
        // The Engine's position is the truth whenever it reports one; between
        // reports, the clock runs on its own.
        const bool newTrack = !sameSource || last->artist != next.artist || last->title != next.title;
        if (newTrack || last->position != next.position) {
            clockBase_ = next.position;
        } else {
            clockBase_ = elapsed();
        }
    } else if (next.source == Source::Radio) {
        // Counts from tuning the Station, like Winamp does for a stream.
        if (!sameSource || last->station != next.station) {
            clockBase_ = milliseconds{0};
        } else {
            clockBase_ = elapsed();
        }
    } else {
        clockBase_ = milliseconds{0};
    }
    clockSince_ = now;
    clockRunning_ = next.source != Source::Stopped && next.status == PlaybackStatus::Playing;
    lastNowPlaying_ = next;
}

milliseconds TftView::elapsed() const {
    milliseconds time = clockBase_;
    if (clockRunning_) {
        time += scheduler_.now() - clockSince_;
    }
    if (lastNowPlaying_ && lastNowPlaying_->source == Source::Spotify &&
        lastNowPlaying_->duration > milliseconds{0}) {
        time = std::min(time, lastNowPlaying_->duration);
    }
    return time;
}

// --- Frames ------------------------------------------------------------------------

void TftView::render() {
    canvas_.resetClip();
    canvas_.fill(kBackground);
    scrolling_ = false;
    if (const auto* list = std::get_if<StationListScreen>(&screen_)) {
        drawStationList(*list);
    } else {
        drawNowPlaying(std::get<NowPlayingScreen>(screen_));
    }
}

void TftView::flush() {
    if (!shownValid_) {
        display_.write({0, 0, frame_.width, frame_.height}, frame_.pixels);
        shownValid_ = true;
    } else {
        for (const Rect& area : changedAreas(shown_, frame_)) {
            display_.write(area, crop(frame_, area));
        }
    }
    // Every frame is drawn from scratch, so the old one can be recycled.
    std::swap(shown_, frame_);
}

bool TftView::animating() const {
    if (scrolling_) {
        return true;
    }
    if (const auto* nowPlaying = std::get_if<NowPlayingScreen>(&screen_)) {
        // Playing: spectrum and clock. Paused: the blinking clock.
        return nowPlaying->source != Source::Stopped && nowPlaying->status != PlaybackStatus::Stopped
                   ? true
                   : !spectrum_.idle();
    }
    return false;
}

void TftView::scheduleFrame() {
    if (!frameTimer_ && animating()) {
        frameTimer_ = scheduler_.callAfter(kFrameInterval, [this] { onFrame(); });
    }
}

void TftView::onFrame() {
    frameTimer_.reset();
    if (const auto* nowPlaying = std::get_if<NowPlayingScreen>(&screen_)) {
        spectrum_.step(nowPlaying->source != Source::Stopped &&
                           nowPlaying->status == PlaybackStatus::Playing,
                       kSpectrumArea.height);
    }
    render();
    flush();
    scheduleFrame();
}

// --- Now Playing -------------------------------------------------------------------

void TftView::drawNowPlaying(const NowPlayingScreen& screen) {
    // Art and labels.
    std::string_view placeholder = "WINAMP";
    std::string_view label;
    const Image* art = nullptr;
    std::string line1 = "WinampDeck";
    std::string line2 = "Press Eject to start";
    bool clockShown = false;

    if (screen.source == Source::Spotify) {
        placeholder = "SPOTIFY";
        label = "SPOTIFY";
        if (!screen.coverUrl.empty()) {
            art = artwork_.cover(screen.coverUrl);
        }
        if (screen.status == PlaybackStatus::Stopped) {
            line1 = "Spotify";
            line2 = "Connect from Spotify app";
        } else {
            line1 = screen.artist.empty() ? "Spotify" : screen.artist;
            line2 = screen.title;
            clockShown = !screen.title.empty() || !screen.artist.empty();
        }
    } else if (screen.source == Source::Radio) {
        placeholder = "RADIO";
        label = "RADIO";
        if (!screen.logo.empty()) {
            art = artwork_.logo(screen.logo);
        }
        line1 = screen.station.empty() ? "Internet Radio" : screen.station;
        line2 = screen.title;
        clockShown = screen.status != PlaybackStatus::Stopped;
    }

    drawArt(kArtArea, art, placeholder);
    drawStatus(kColumnX, kStatusY, screen.source == Source::Stopped ? PlaybackStatus::Stopped
                                                                    : screen.status);
    canvas_.text(kColumnX + 10, kStatusY, label, kLabel);

    if (clockShown) {
        const bool blinkedOff = screen.status == PlaybackStatus::Paused &&
                                (scheduler_.now() / kBlinkPeriod) % 2 == 1;
        if (!blinkedOff) {
            const std::string text = clockText(elapsed());
            const int width = Canvas::textWidth(text.size(), kClockScale) - kClockScale;
            canvas_.text(kColumnRight - width, kClockY, text, kGreen, kClockScale);
        }
    }

    drawSpectrum(kSpectrumArea);
    if (screen.source == Source::Spotify) {
        drawProgress(kProgressArea, screen);
    }

    // Scroll checks go last: drawLine() records whether anything scrolls.
    const bool scroll1 = drawLine(kLine1Area, lines_[0], line1, kWhite);
    const bool scroll2 = drawLine(kLine2Area, lines_[1], line2, kGreen);
    scrolling_ = scroll1 || scroll2;
}

void TftView::drawArt(const Rect& area, const Image* image, std::string_view placeholder) {
    if (image != nullptr && image->width == area.width && image->height == area.height) {
        canvas_.blit(*image, area.x, area.y);
        return;
    }
    canvas_.fill(area, kPlaceholder);
    canvas_.frame(area, kDim);
    const std::u32string text = decodeUtf8(placeholder);
    const int width = Canvas::textWidth(text.size(), 2) - 2;
    canvas_.text(area.x + (area.width - width) / 2, area.y + (area.height - kGlyphHeight * 2) / 2,
                 text, kDim, 2);
}

void TftView::drawStatus(int x, int y, PlaybackStatus status) {
    switch (status) {
    case PlaybackStatus::Playing:
        // A right-pointing triangle, 4 wide and 7 tall.
        for (int row = 0; row < kGlyphHeight; ++row) {
            canvas_.horizontalLine(x, y + row, 4 - std::abs(row - 3), kPlayIcon);
        }
        break;
    case PlaybackStatus::Paused:
        canvas_.fill({x, y, 2, kGlyphHeight}, kPauseIcon);
        canvas_.fill({x + 4, y, 2, kGlyphHeight}, kPauseIcon);
        break;
    case PlaybackStatus::Stopped:
        canvas_.fill({x, y + 1, 6, 6}, kStopIcon);
        break;
    }
}

void TftView::drawSpectrum(const Rect& area) {
    for (int bar = 0; bar < kSpectrumBars; ++bar) {
        const int x = area.x + bar * (kBarWidth + 1);
        const int height = spectrum_.bars[static_cast<std::size_t>(bar)];
        for (int level = 0; level < height; ++level) {
            canvas_.horizontalLine(x, area.bottom() - 1 - level, kBarWidth,
                                   spectrumColor(level, area.height));
        }
        const int peak = spectrum_.peaks[static_cast<std::size_t>(bar)];
        if (peak > 0) {
            canvas_.horizontalLine(x, area.bottom() - peak, kBarWidth, kPeak);
        }
    }
}

void TftView::drawProgress(const Rect& area, const NowPlayingScreen& screen) {
    if (screen.status == PlaybackStatus::Stopped || screen.duration <= milliseconds{0}) {
        return;
    }
    canvas_.fill(area, kTrack);
    const int knobWidth = 4;
    const auto span = static_cast<long>(area.width - knobWidth);
    const int knobX = area.x + static_cast<int>(span * elapsed().count() / screen.duration.count());
    canvas_.fill({area.x, area.y, knobX - area.x, area.height}, kGreen);
    canvas_.fill({knobX, area.y, knobWidth, area.height}, kSeekKnob);
}

bool TftView::drawLine(const Rect& area, ScrollingLine& line, std::string_view text, Color color) {
    const milliseconds now = scheduler_.now();
    std::u32string decoded = decodeUtf8(text);
    if (decoded != line.text) {
        line.text = std::move(decoded);
        line.since = now;
    }

    canvas_.setClip(area);
    const int width = Canvas::textWidth(line.text.size()) - 1;
    if (width <= area.width) {
        canvas_.text(area.x, area.y, line.text, color);
        canvas_.resetClip();
        return false;
    }

    // Winamp's marquee: the text, a separator, then the text again.
    const std::u32string cycle = line.text + U"  ***  ";
    const int cycleWidth = Canvas::textWidth(cycle.size());
    const milliseconds moving = now - line.since - kScrollDelay;
    const int offset =
        moving > milliseconds{0} ? static_cast<int>((moving / kScrollStep) % cycleWidth) : 0;
    canvas_.text(area.x - offset, area.y, cycle, color);
    canvas_.text(area.x - offset + cycleWidth, area.y, cycle, color);
    canvas_.resetClip();
    return true;
}

void TftView::drawTruncated(int x, int y, int width, std::string_view text, Color color) {
    std::u32string decoded = decodeUtf8(text);
    const auto fits = static_cast<std::size_t>((width + 1) / kCharAdvance);
    if (decoded.size() > fits) {
        decoded.resize(fits);
    }
    canvas_.text(x, y, decoded, color);
}

// --- Station List ------------------------------------------------------------------

void TftView::drawStationList(const StationListScreen& screen) {
    const int count = static_cast<int>(screen.stations.size());
    const int selected = static_cast<int>(screen.selected);

    canvas_.text(2, 2, "STATIONS", kGreen);
    if (count == 0) {
        canvas_.text(2, kRowsTop + 8, "No Stations", kWhite);
        return;
    }
    const std::string position = std::to_string(selected + 1) + "/" + std::to_string(count);
    canvas_.text(Display::kWidth - 1 - (Canvas::textWidth(position.size()) - 1), 2, position, kWhite);
    canvas_.horizontalLine(0, kHeaderHeight, Display::kWidth, kDim);

    // Keep the highlight in the middle row where possible.
    const int top = std::clamp(selected - kVisibleRows / 2, 0, std::max(0, count - kVisibleRows));
    const bool scrollbar = count > kVisibleRows;
    const int rowRight = scrollbar ? kScrollbarX - 1 : Display::kWidth;
    const int textRight = rowRight - 2;

    for (int row = 0; row < kVisibleRows && top + row < count; ++row) {
        const int index = top + row;
        const Station& station = screen.stations[static_cast<std::size_t>(index)];
        const int y = kRowsTop + row * kRowHeight;
        if (index == selected) {
            canvas_.fill({0, y, rowRight, kRowHeight}, kSelected);
        }

        const Rect thumbArea{2, y + 1, Artwork::kThumbnailSize, Artwork::kThumbnailSize};
        const Image* thumb = station.logo.empty() ? nullptr : artwork_.logoThumbnail(station.logo);
        if (thumb != nullptr && thumb->width == thumbArea.width && thumb->height == thumbArea.height) {
            canvas_.blit(*thumb, thumbArea.x, thumbArea.y);
        } else {
            canvas_.fill(thumbArea, kPlaceholder);
            canvas_.frame(thumbArea, kDim);
        }

        const Color color = index == static_cast<int>(screen.tuned) ? kWhite : kGreen;
        const int textY = y + (kRowHeight - kGlyphHeight) / 2;
        if (index == selected) {
            scrolling_ = drawLine({kRowTextX, textY, textRight - kRowTextX, kGlyphHeight}, lines_[2],
                                  station.name, color);
        } else {
            drawTruncated(kRowTextX, textY, textRight - kRowTextX, station.name, color);
        }
    }

    if (scrollbar) {
        const int trackHeight = kVisibleRows * kRowHeight;
        const int thumbHeight = std::max(6, trackHeight * kVisibleRows / count);
        const int thumbY = kRowsTop + (trackHeight - thumbHeight) * top / (count - kVisibleRows);
        canvas_.fill({kScrollbarX, kRowsTop, 2, trackHeight}, kTrack);
        canvas_.fill({kScrollbarX, thumbY, 2, thumbHeight}, kGreen);
    }
}

// --- Spectrum ----------------------------------------------------------------------

void TftView::Spectrum::step(bool playing, int height) {
    for (std::size_t bar = 0; bar < bars.size(); ++bar) {
        int target = 0;
        if (playing) {
            // xorshift32: cheap, and deterministic for tests.
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            // Lower bands run taller, like most music.
            const int reach = height * (100 - static_cast<int>(bar) * 45 / kSpectrumBars) / 100;
            target = static_cast<int>(random % static_cast<std::uint32_t>(reach + 1));
        }
        bars[bar] = std::max(target, bars[bar] - kBarFall);
        bars[bar] = std::max(bars[bar], 0);

        if (bars[bar] >= peaks[bar]) {
            peaks[bar] = bars[bar];
            peakHold[bar] = kPeakHoldFrames;
        } else if (peakHold[bar] > 0) {
            --peakHold[bar];
        } else {
            peaks[bar] = std::max(0, peaks[bar] - kPeakFall);
        }
    }
}

bool TftView::Spectrum::idle() const {
    return std::ranges::all_of(bars, [](int bar) { return bar == 0; }) &&
           std::ranges::all_of(peaks, [](int peak) { return peak == 0; });
}

}  // namespace winampdeck::ui
