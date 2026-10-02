#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "ui/lcd_display.hpp"

namespace winampdeck {

// The 1602 LCD's HD44780, driven through pigpio over its FC-113 (PCF8574)
// backpack on I2C1, behind the TXS0108E level shifter. Wiring:
// docs/wiring.md.
//
// The backpack wires the expander's P0 to RS, P1 to R/W, P2 to E, P3 to the
// backlight transistor and P4–P7 to D4–D7, so the controller runs in 4-bit
// mode, and is only ever written to. Requires a live PigpioSession.
class Hd44780 final : public ui::LcdDisplay {
public:
    static constexpr unsigned kDefaultAddress = 0x27;

    // Initialises the controller, clears both rows and turns the backlight
    // on. Throws std::runtime_error if the backpack doesn't answer.
    explicit Hd44780(unsigned address = kDefaultAddress);
    // Clears the display and turns the backlight off.
    ~Hd44780() override;

    Hd44780(const Hd44780&) = delete;
    Hd44780& operator=(const Hd44780&) = delete;

    void writeRow(std::string_view codes) override;

private:
    // Queues one byte for the controller as two E-strobed nibbles.
    void queue(std::vector<std::uint8_t>& out, std::uint8_t value, bool data) const;
    void queueNibble(std::vector<std::uint8_t>& out, std::uint8_t nibble, bool data) const;
    void command(std::uint8_t value);
    void send(const std::vector<std::uint8_t>& bytes);

    unsigned address_;
    unsigned i2c_ = 0;
    std::uint8_t backlight_;
};

}  // namespace winampdeck
