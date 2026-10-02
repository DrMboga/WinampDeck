// The WinampDeck controller: PlayerController on the real panel, driving the
// real Engines. Runs on the Pi as root (pigpio needs it).
//
//   winampdeck [--config-dir DIR] [--mpv-socket PATH]
//
// Reads, once at startup (ADR 0006):
//   DIR/config.json    settings; optional, missing ones keep their defaults
//   DIR/stations.csv   the Stations
//   DIR/logos/         their logos
// DIR is /etc/winampdeck unless given. go-librespot's API is expected on
// localhost:3678 and mpv's IPC socket at PATH (default /tmp/mpv-socket);
// both are separate processes that the controller connects to, and waits for.
//
// SIGINT/SIGTERM switch the panel off and exit.

#include <asio/io_context.hpp>
#include <asio/signal_set.hpp>

#include <csignal>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "adapters/asio_scheduler.hpp"
#include "adapters/deck_artwork.hpp"
#include "adapters/go_librespot_client.hpp"
#include "adapters/mpv_client.hpp"
#include "adapters/system_poweroff.hpp"
#include "core/deck_config.hpp"
#include "core/player_controller.hpp"
#include "core/stations_csv.hpp"
#include "core/version.hpp"
#include "hw/hd44780.hpp"
#include "hw/pigpio_hardware_io.hpp"
#include "hw/pigpio_session.hpp"
#include "hw/st7735.hpp"
#include "ui/lcd_view.hpp"
#include "ui/tft_view.hpp"

namespace {

using namespace winampdeck;

struct Options {
    std::filesystem::path configDir = "/etc/winampdeck";
    std::filesystem::path mpvSocket = "/tmp/mpv-socket";
};

constexpr std::string_view kUsage = "Usage: winampdeck [--config-dir DIR] [--mpv-socket PATH] [--version]\n";

}  // namespace

int main(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--version") {
            std::cout << "WinampDeck controller " << version() << std::endl;
            return 0;
        }
        if (arg == "--help") {
            std::cout << kUsage;
            return 0;
        }
        if ((arg == "--config-dir" || arg == "--mpv-socket") && i + 1 < argc) {
            (arg == "--config-dir" ? options.configDir : options.mpvSocket) = argv[++i];
        } else {
            std::cerr << "error: unexpected " << arg << "\n" << kUsage;
            return 2;
        }
    }

    try {
        // The hand-edited files first, so a mistake in one is reported before
        // anything on the panel moves.
        const DeckConfig config = loadDeckConfig(options.configDir / "config.json");
        auto stations = loadStationsCsv(options.configDir / "stations.csv");
        std::cout << "WinampDeck controller " << version() << ": " << stations.size() << " Stations, "
                  << "TFT brightness " << config.tftBrightness << "%, LCD scroll step "
                  << config.lcdScrollStep.count() << "ms" << std::endl;

        PigpioSession pigpio;
        asio::io_context io;
        asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](const std::error_code&, int) { io.stop(); });

        AsioScheduler scheduler(io);
        St7735::Config tftConfig;
        tftConfig.brightness = config.tftBacklightLevel();
        St7735 tft(tftConfig);
        DeckArtwork artwork(io, options.configDir / "logos");
        ui::TftView tftView(tft, artwork, scheduler);
        Hd44780 lcd;
        ui::LcdView lcdView(lcd, scheduler, config.lcdScrollStep);
        PigpioHardwareIO panel(io, &tftView, &lcdView);

        GoLibrespotClient spotify(io);
        MpvClient radio(io, options.mpvSocket);
        SystemPoweroff system;
        std::mt19937 random{std::random_device{}()};
        PlayerController controller(spotify, radio, panel, scheduler, system, std::move(stations),
                                    [&random](std::size_t count) {
                                        return std::uniform_int_distribution<std::size_t>(0, count - 1)(random);
                                    });
        controller.start();
        io.run();
        std::cout << "Stopping." << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << std::endl;
        return 1;
    }
}
