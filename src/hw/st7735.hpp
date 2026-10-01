#pragma once

#include <cstdint>
#include <initializer_list>
#include <span>

#include "ui/display.hpp"

namespace winampdeck {

// The 1.77" ST7735 TFT, driven directly through pigpio: SPI0/CE0 for data,
// GPIO23 for RS (command/data), GPIO24 for RES, and the backlight on GPIO12.
// Wiring: docs/wiring.md. Requires a live PigpioSession.
//
// The panel holds the display in landscape, so it's addressed as 160×128.
class St7735 final : public ui::Display {
public:
    struct Config {
        // ST7735S's write cycle is specified down to 66ns (~15MHz); these
        // modules usually run well above that, but start within spec.
        unsigned spiBaud = 16'000'000;
        // MADCTL: MY/MX/MV choose the rotation and mirroring, BGR the colour
        // order. 0xA0 (MY|MV) is landscape for the panel's mounting, with the
        // module's pin header on the left. 0x60 (MX|MV) is the same turned
        // 180°, which is how it sat on the breadboard for the 9.3 checks.
        std::uint8_t madctl = 0xA0;
        // Some panels' visible area doesn't start at the controller's (0, 0).
        int columnOffset = 0;
        int rowOffset = 0;
        // Backlight PWM duty, 0 (off) to 255 (full).
        unsigned brightness = 255;
    };

    // Resets and initialises the panel, clears it to black, and turns the
    // backlight on. Throws std::runtime_error if SPI can't be opened.
    explicit St7735(const Config& config);
    // Blanks the panel and turns the backlight off.
    ~St7735() override;

    St7735(const St7735&) = delete;
    St7735& operator=(const St7735&) = delete;

    void write(const ui::Rect& area, std::span<const ui::Color> pixels) override;

    void setBrightness(unsigned brightness);

private:
    void command(std::uint8_t command, std::initializer_list<std::uint8_t> data = {});
    void setWindow(const ui::Rect& area);
    void sendData(const std::uint8_t* bytes, std::size_t count);

    Config config_;
    unsigned spi_ = 0;
};

}  // namespace winampdeck
