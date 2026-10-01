// Bring-up tool for the panel's buttons, LEDs and TFT, run on the Pi as root.
// Three modes:
//
//   winampdeck-panel-test
//       Prints every button press/release. Shuffle and Repeat toggle their
//       own LED. Both LEDs blink once at startup.
//
//   winampdeck-panel-test --tft [TFT OPTIONS]
//       Draws a test pattern on the TFT for checking its orientation, colour
//       order and edges, then cycles through the real Now Playing and Station
//       List screens. Doesn't touch the buttons.
//
//   winampdeck-panel-test --controller [TFT OPTIONS]
//       Runs the real PlayerController on the real buttons, LEDs and TFT,
//       with console stand-ins for the Engines and the Stations from
//       stations.csv. The Spotify stand-in echoes each command back as the
//       Engine's new state, the way go-librespot would, and plays a few demo
//       tracks with real album covers. Safe Shutdown is only printed, never
//       carried out.
//
// TFT OPTIONS:
//   --data DIR         where stations.csv and logos/ are (default: data)
//   --madctl 0xNN      ST7735 rotation and colour order (default: 0xA0)
//   --spi-hz N         SPI clock in Hz (default: 16000000)
//   --brightness N     backlight, 0-255 (default: 255)
//   --offset COL,ROW   where the visible area starts (default: 0,0)
//
// Ctrl+C quits, switching the LEDs and TFT off.

#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/signal_set.hpp>
#include <asio/steady_timer.hpp>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "adapters/asio_scheduler.hpp"
#include "adapters/deck_artwork.hpp"
#include "core/player_controller.hpp"
#include "core/stations_csv.hpp"
#include "hw/pigpio_hardware_io.hpp"
#include "hw/pigpio_session.hpp"
#include "hw/st7735.hpp"
#include "ui/canvas.hpp"
#include "ui/tft_view.hpp"

namespace {

using namespace winampdeck;
using namespace std::chrono_literals;
using Clock = std::chrono::steady_clock;

struct Options {
    enum class Mode { Buttons, Tft, Controller } mode = Mode::Buttons;
    std::filesystem::path data = "data";
    St7735::Config tft;
};

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument(std::string(arg) + " needs a value");
            }
            return argv[++i];
        };
        if (arg == "--tft") {
            options.mode = Options::Mode::Tft;
        } else if (arg == "--controller") {
            options.mode = Options::Mode::Controller;
        } else if (arg == "--data") {
            options.data = value();
        } else if (arg == "--madctl") {
            options.tft.madctl = static_cast<std::uint8_t>(std::stoul(value(), nullptr, 0));
        } else if (arg == "--spi-hz") {
            options.tft.spiBaud = static_cast<unsigned>(std::stoul(value()));
        } else if (arg == "--brightness") {
            options.tft.brightness = static_cast<unsigned>(std::stoul(value()));
        } else if (arg == "--offset") {
            const std::string offset = value();
            const auto comma = offset.find(',');
            if (comma == std::string::npos) {
                throw std::invalid_argument("--offset takes COL,ROW");
            }
            options.tft.columnOffset = std::stoi(offset.substr(0, comma));
            options.tft.rowOffset = std::stoi(offset.substr(comma + 1));
        } else {
            throw std::invalid_argument("unknown option " + std::string(arg));
        }
    }
    return options;
}

// Mode 1: report the raw, debounced buttons.
class ButtonEcho final : private HardwareIO::Listener {
public:
    explicit ButtonEcho(HardwareIO& hardware) : hardware_(hardware) { hardware_.setListener(this); }
    ~ButtonEcho() { hardware_.setListener(nullptr); }

    ButtonEcho(const ButtonEcho&) = delete;
    ButtonEcho& operator=(const ButtonEcho&) = delete;

private:
    void onButtonPressed(Button button) override {
        pressedAt_[button] = Clock::now();
        std::cout << "pressed  " << toString(button) << std::endl;
        if (button == Button::Shuffle || button == Button::Repeat) {
            const Led led = button == Button::Shuffle ? Led::Shuffle : Led::Repeat;
            const bool on = !ledOn_[led];
            ledOn_[led] = on;
            hardware_.setLed(led, on);
            std::cout << "  " << toString(led) << " LED " << (on ? "on" : "off") << std::endl;
        }
    }

    void onButtonReleased(Button button) override {
        const auto held = std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now() - pressedAt_[button]);
        std::cout << "released " << toString(button) << " (held " << held.count() << " ms)"
                  << std::endl;
    }

    HardwareIO& hardware_;
    std::map<Button, Clock::time_point> pressedAt_;
    std::map<Led, bool> ledOn_;
};

// Tracks the Spotify stand-in plays, with real album covers so the TFT
// downloads and converts them just as it will for go-librespot's.
const std::vector<SpotifyTrack>& demoTracks() {
    static const std::vector<SpotifyTrack> tracks = {
        {"Daft Punk", "One More Time", "Discovery", 320s,
         "https://i.scdn.co/image/ab67616d0000b2731e81bff9807a9e629fce5ade"},
        {"Daft Punk", "Get Lucky (feat. Pharrell Williams and Nile Rodgers)", "Random Access Memories",
         369s, "https://i.scdn.co/image/ab67616d0000b2739b9b36b0e22870b9f542d937"},
        {"Radiohead", "Paranoid Android", "OK Computer", 387s,
         "https://i.scdn.co/image/ab67616d0000b273c8b444df094279e70d0ed856"},
    };
    return tracks;
}

// Mode 3 stand-ins.
class ConsoleEngineClient final : public EngineClient {
public:
    explicit ConsoleEngineClient(asio::io_context& io) : io_(io) {}

    void setListener(Listener* listener) override { listener_ = listener; }
    void play() override {
        // The first Play stands in for a phone picking this Deck.
        const bool connect = !active_;
        active_ = true;
        command("play", [this, connect](Listener& l) {
            if (connect) {
                l.onSpotifyActiveChanged(true);
                l.onSpotifyTrackChanged(demoTracks()[track_]);
            }
            l.onSpotifyPlayingChanged(true);
        });
    }
    void pause() override { command("pause", [](Listener& l) { l.onSpotifyPlayingChanged(false); }); }
    void next() override { skip(1); }
    void previous() override { skip(demoTracks().size() - 1); }
    void setShuffle(bool enabled) override {
        command(enabled ? "shuffle on" : "shuffle off",
                [enabled](Listener& l) { l.onSpotifyShuffleChanged(enabled); });
    }
    void setRepeat(bool enabled) override {
        command(enabled ? "repeat on" : "repeat off",
                [enabled](Listener& l) { l.onSpotifyRepeatChanged(enabled); });
    }

private:
    void skip(std::size_t by) {
        command(by == 1 ? "next" : "previous", [this, by](Listener& l) {
            if (active_) {
                track_ = (track_ + by) % demoTracks().size();
                l.onSpotifyTrackChanged(demoTracks()[track_]);
            }
        });
    }

    // Reports the resulting state later, from the event loop, like a real event.
    void command(std::string_view name, std::function<void(Listener&)> report = {}) {
        std::cout << "  Spotify: " << name << std::endl;
        if (report) {
            asio::post(io_, [this, report = std::move(report)] {
                if (listener_ != nullptr) {
                    report(*listener_);
                }
            });
        }
    }

    asio::io_context& io_;
    Listener* listener_ = nullptr;
    bool active_ = false;
    std::size_t track_ = 0;
};

class ConsoleRadioClient final : public RadioClient {
public:
    void setListener(Listener* /*listener*/) override {}
    void tune(const Station& station) override { std::cout << "  Radio: tune " << station.name << std::endl; }
    void pause() override { std::cout << "  Radio: pause" << std::endl; }
    void resume() override { std::cout << "  Radio: resume" << std::endl; }
    void mute() override { std::cout << "  Radio: mute" << std::endl; }
    void unmute() override { std::cout << "  Radio: unmute" << std::endl; }
};

class ConsoleSystemControl final : public SystemControl {
public:
    void safeShutdown() override {
        std::cout << "  System: Safe Shutdown (not carried out by the panel test)" << std::endl;
    }
};

// Presses reach PlayerController through this, so the console shows what was
// pressed right above what the controller did about it.
class LoggingHardwareIO final : public HardwareIO, private HardwareIO::Listener {
public:
    explicit LoggingHardwareIO(HardwareIO& inner) : inner_(inner) { inner_.setListener(this); }
    ~LoggingHardwareIO() override { inner_.setListener(nullptr); }

    LoggingHardwareIO(const LoggingHardwareIO&) = delete;
    LoggingHardwareIO& operator=(const LoggingHardwareIO&) = delete;

    void setListener(HardwareIO::Listener* listener) override { listener_ = listener; }
    void setLed(Led led, bool on) override {
        std::cout << "  " << toString(led) << " LED " << (on ? "on" : "off") << std::endl;
        inner_.setLed(led, on);
    }
    void showLcdText(const std::string& text) override { inner_.showLcdText(text); }
    void showScreen(const Screen& screen) override { inner_.showScreen(screen); }

private:
    void onButtonPressed(Button button) override {
        std::cout << "pressed  " << toString(button) << std::endl;
        if (listener_ != nullptr) {
            listener_->onButtonPressed(button);
        }
    }
    void onButtonReleased(Button button) override {
        std::cout << "released " << toString(button) << std::endl;
        if (listener_ != nullptr) {
            listener_->onButtonReleased(button);
        }
    }

    HardwareIO& inner_;
    HardwareIO::Listener* listener_ = nullptr;
};

void blinkLeds(HardwareIO& hardware) {
    hardware.setLed(Led::Shuffle, true);
    hardware.setLed(Led::Repeat, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    hardware.setLed(Led::Shuffle, false);
    hardware.setLed(Led::Repeat, false);
}

// Mode 2: a frame on every edge, labelled corners, and red/green/blue bars.
void drawTestPattern(ui::Display& display) {
    using ui::rgb;
    ui::Image image(ui::Display::kWidth, ui::Display::kHeight);
    ui::Canvas canvas(image);
    const ui::Color white = rgb(255, 255, 255);
    canvas.frame({0, 0, image.width, image.height}, white);
    canvas.text(3, 3, "TOP LEFT", white);
    canvas.text(image.width - 3 - (ui::Canvas::textWidth(12) - 1), image.height - 10, "BOTTOM RIGHT", white);

    const struct {
        const char* label;
        ui::Color color;
    } bars[] = {{"RED", rgb(255, 0, 0)}, {"GREEN", rgb(0, 255, 0)}, {"BLUE", rgb(0, 0, 255)}};
    for (int i = 0; i < 3; ++i) {
        const int x = 8 + i * 50;
        canvas.fill({x, 30, 44, 50}, bars[i].color);
        canvas.text(x + 2, 84, bars[i].label, white);
    }

    const auto start = Clock::now();
    display.write({0, 0, image.width, image.height}, image.pixels);
    const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
    std::cout << "Full-screen write took " << took.count() << " ms." << std::endl;
}

// Mode 2, after the pattern: the real views, one Screen every few seconds.
std::vector<Screen> demoScreens(const std::vector<Station>& stations) {
    std::vector<Screen> screens;
    screens.emplace_back(NowPlayingScreen{});

    const SpotifyTrack& track = demoTracks()[0];
    NowPlayingScreen spotify;
    spotify.source = Source::Spotify;
    spotify.status = PlaybackStatus::Playing;
    spotify.artist = track.artist;
    spotify.title = track.title;
    spotify.coverUrl = track.coverUrl;
    spotify.duration = track.duration;
    spotify.position = 75s;
    screens.emplace_back(spotify);

    if (!stations.empty()) {
        NowPlayingScreen radio;
        radio.source = Source::Radio;
        radio.status = PlaybackStatus::Playing;
        radio.station = stations[0].name;
        radio.logo = stations[0].logo;
        radio.title = "Stream title — Группа крови — a long one, so it scrolls";
        screens.emplace_back(radio);
        screens.emplace_back(StationListScreen{stations, std::min<std::size_t>(3, stations.size() - 1), 0});
    }
    return screens;
}

void runTftDemo(asio::io_context& io, const Options& options) {
    const auto stations = loadStationsCsv(options.data / "stations.csv");
    St7735 tft(options.tft);
    drawTestPattern(tft);
    std::cout << "Check: a white frame on all four edges; TOP LEFT and BOTTOM RIGHT reading\n"
                 "normally in their corners; RED, GREEN, BLUE bars in those colours.\n"
                 "The demo screens start in 10 seconds. Ctrl+C quits."
              << std::endl;

    AsioScheduler scheduler(io);
    DeckArtwork artwork(io, options.data / "logos");
    ui::TftView view(tft, artwork, scheduler);
    const auto screens = demoScreens(stations);
    std::size_t next = 0;
    asio::steady_timer timer(io, 10s);
    std::function<void(const std::error_code&)> step = [&](const std::error_code& error) {
        if (error) {
            return;
        }
        view.show(screens[next]);
        next = (next + 1) % screens.size();
        timer.expires_after(6s);
        timer.async_wait(step);
    };
    timer.async_wait(step);
    io.run();
}

std::size_t randomBelow(std::mt19937& random, std::size_t count) {
    return std::uniform_int_distribution<std::size_t>(0, count - 1)(random);
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    try {
        options = parseOptions(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n"
                  << "Usage: " << argv[0] << " [--tft | --controller] [--data DIR] [--madctl 0xNN]\n"
                  << "       [--spi-hz N] [--brightness 0-255] [--offset COL,ROW]\n";
        return 2;
    }

    try {
        PigpioSession pigpio;
        asio::io_context io;
        asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](const std::error_code&, int) { io.stop(); });

        switch (options.mode) {
        case Options::Mode::Buttons: {
            PigpioHardwareIO hardware(io);
            std::cout << "Buttons mode. Blinking both LEDs..." << std::endl;
            blinkLeds(hardware);
            ButtonEcho echo(hardware);
            std::cout << "Press any button; Shuffle and Repeat toggle their LEDs. Ctrl+C quits."
                      << std::endl;
            io.run();
            break;
        }
        case Options::Mode::Tft:
            std::cout << "TFT mode." << std::endl;
            runTftDemo(io, options);
            break;
        case Options::Mode::Controller: {
            std::cout << "Controller mode: PlayerController on the real panel, Engines simulated.\n"
                         "Starts Stopped: press Eject to switch Source. Ctrl+C quits."
                      << std::endl;
            auto stations = loadStationsCsv(options.data / "stations.csv");
            AsioScheduler scheduler(io);
            St7735 tft(options.tft);
            DeckArtwork artwork(io, options.data / "logos");
            ui::TftView view(tft, artwork, scheduler);
            PigpioHardwareIO hardware(io, &view);
            ConsoleEngineClient engine(io);
            ConsoleRadioClient radio;
            ConsoleSystemControl system;
            LoggingHardwareIO panel(hardware);
            std::mt19937 random{std::random_device{}()};
            PlayerController controller(engine, radio, panel, scheduler, system, std::move(stations),
                                        [&random](std::size_t count) { return randomBelow(random, count); });
            controller.start();
            io.run();
            break;
        }
        }
        std::cout << "\nBye." << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << std::endl;
        return 1;
    }
}
