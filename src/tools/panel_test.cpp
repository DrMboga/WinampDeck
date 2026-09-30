// Phase 5 bring-up tool for the panel's buttons and LEDs, run on the Pi as
// root. Two modes:
//
//   winampdeck-panel-test
//       Prints every button press/release. Shuffle and Repeat toggle their
//       own LED. Both LEDs blink once at startup.
//
//   winampdeck-panel-test --controller
//       Runs the real PlayerController on the real buttons/LEDs, with console
//       stand-ins for the Engines, so the panel behaves as the Phase 1 tests
//       say it should. The Spotify stand-in echoes each command back as the
//       Engine's new state, the way go-librespot would. Safe Shutdown is only
//       printed, never carried out.
//
// Ctrl+C quits and switches both LEDs off.

#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/signal_set.hpp>

#include <chrono>
#include <csignal>
#include <cstddef>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "adapters/asio_scheduler.hpp"
#include "core/player_controller.hpp"
#include "hw/pigpio_hardware_io.hpp"
#include "hw/pigpio_session.hpp"

namespace {

using namespace winampdeck;
using Clock = std::chrono::steady_clock;

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

// Mode 2 stand-ins.
class ConsoleEngineClient final : public EngineClient {
public:
    explicit ConsoleEngineClient(asio::io_context& io) : io_(io) {}

    void setListener(Listener* listener) override { listener_ = listener; }
    void play() override { command("play", [](Listener& l) { l.onSpotifyPlayingChanged(true); }); }
    void pause() override { command("pause", [](Listener& l) { l.onSpotifyPlayingChanged(false); }); }
    void next() override { command("next"); }
    void previous() override { command("previous"); }
    void setShuffle(bool enabled) override {
        command(enabled ? "shuffle on" : "shuffle off",
                [enabled](Listener& l) { l.onSpotifyShuffleChanged(enabled); });
    }
    void setRepeat(bool enabled) override {
        command(enabled ? "repeat on" : "repeat off",
                [enabled](Listener& l) { l.onSpotifyRepeatChanged(enabled); });
    }

private:
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

std::vector<Station> demoStations() {
    return {
        {"Station One", "", ""},
        {"Station Two", "", ""},
        {"Station Three", "", ""},
    };
}

void blinkLeds(HardwareIO& hardware) {
    hardware.setLed(Led::Shuffle, true);
    hardware.setLed(Led::Repeat, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    hardware.setLed(Led::Shuffle, false);
    hardware.setLed(Led::Repeat, false);
}

}  // namespace

int main(int argc, char** argv) {
    const bool controllerMode = argc > 1 && std::string_view(argv[1]) == "--controller";
    if (argc > 2 || (argc == 2 && !controllerMode)) {
        std::cerr << "Usage: " << argv[0] << " [--controller]\n";
        return 2;
    }

    try {
        PigpioSession pigpio;
        asio::io_context io;
        PigpioHardwareIO hardware(io);

        asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](const std::error_code&, int) { io.stop(); });

        if (!controllerMode) {
            std::cout << "Buttons mode. Blinking both LEDs..." << std::endl;
            blinkLeds(hardware);
            ButtonEcho echo(hardware);
            std::cout << "Press any button; Shuffle and Repeat toggle their LEDs. Ctrl+C quits."
                      << std::endl;
            io.run();
        } else {
            std::cout << "Controller mode: PlayerController on the real panel, Engines simulated.\n"
                         "Starts Stopped: press Eject to switch Source. Ctrl+C quits."
                      << std::endl;
            AsioScheduler scheduler(io);
            ConsoleEngineClient engine(io);
            ConsoleRadioClient radio;
            ConsoleSystemControl system;
            LoggingHardwareIO panel(hardware);
            std::mt19937 random{std::random_device{}()};
            PlayerController controller(engine, radio, panel, scheduler, system, demoStations(),
                                        [&random](std::size_t count) {
                                            return std::uniform_int_distribution<std::size_t>(
                                                0, count - 1)(random);
                                        });
            controller.start();
            io.run();
        }
        std::cout << "\nBye." << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << std::endl;
        return 1;
    }
}
