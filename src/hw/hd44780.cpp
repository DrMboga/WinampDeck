#include "hw/hd44780.hpp"

#include <pigpio.h>

#include <cstdio>
#include <stdexcept>
#include <string>

namespace winampdeck {

namespace {

constexpr unsigned kI2cBus = 1;

// PCF8574 pins, as the FC-113 backpack wires them.
constexpr std::uint8_t kRs = 0x01;  // Low: command, high: data.
constexpr std::uint8_t kEnable = 0x04;
constexpr std::uint8_t kBacklight = 0x08;
// R/W (0x02) stays low: the controller is only written to.

// HD44780 commands (datasheet table 6).
namespace cmd {
constexpr std::uint8_t kClear = 0x01;
constexpr std::uint8_t kEntryMode = 0x06;      // Cursor moves right, no display shift.
constexpr std::uint8_t kDisplayOff = 0x08;
constexpr std::uint8_t kDisplayOn = 0x0C;      // Cursor and blink off.
constexpr std::uint8_t kFunctionSet = 0x28;    // 4-bit interface, 2 lines, 5×8 dots.
constexpr std::uint8_t kSetDdramAddress = 0x80;  // Row 1 starts at address 0.
}  // namespace cmd

// Clear takes up to 1.52ms; every other command 37µs, which the next I2C
// byte (about 90µs at 100kHz) already covers.
constexpr unsigned kClearMicros = 2'000;

std::string hex(unsigned value) {
    char text[8];
    std::snprintf(text, sizeof text, "%02x", value);
    return text;
}

}  // namespace

Hd44780::Hd44780(unsigned address) : address_(address), backlight_(kBacklight) {
    const int handle = i2cOpen(kI2cBus, address_, 0);
    if (handle < 0) {
        throw std::runtime_error("Can't open I2C bus 1 (pigpio error " + std::to_string(handle) +
                                 "). Is I2C enabled?");
    }
    i2c_ = static_cast<unsigned>(handle);

    try {
        // The datasheet's initialisation by instruction (figure 24): three
        // 8-bit function sets put the controller in a known state whatever
        // mode a previous run left it in, then one 4-bit function set.
        gpioDelay(50'000);  // Power-on: at least 40ms after VCC reaches 2.7V.
        std::vector<std::uint8_t> bytes;
        queueNibble(bytes, 0x3, false);
        send(bytes);
        gpioDelay(4'500);
        send(bytes);
        gpioDelay(150);
        send(bytes);
        gpioDelay(150);
        bytes.clear();
        queueNibble(bytes, 0x2, false);
        send(bytes);
        gpioDelay(150);

        command(cmd::kFunctionSet);
        command(cmd::kDisplayOff);
        command(cmd::kClear);
        gpioDelay(kClearMicros);
        command(cmd::kEntryMode);
        command(cmd::kDisplayOn);
    } catch (...) {
        i2cClose(i2c_);
        throw;
    }
}

Hd44780::~Hd44780() {
    try {
        command(cmd::kClear);
        gpioDelay(kClearMicros);
        backlight_ = 0;
        send({0x00});
    } catch (...) {
        // Nothing to be done about it on the way out.
    }
    i2cClose(i2c_);
}

void Hd44780::writeRow(std::string_view codes) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve((1 + codes.size()) * 4);
    queue(bytes, cmd::kSetDdramAddress, false);
    for (const char code : codes.substr(0, kColumns)) {
        queue(bytes, static_cast<std::uint8_t>(code), true);
    }
    send(bytes);
}

void Hd44780::queue(std::vector<std::uint8_t>& out, std::uint8_t value, bool data) const {
    queueNibble(out, static_cast<std::uint8_t>(value >> 4), data);
    queueNibble(out, static_cast<std::uint8_t>(value & 0x0F), data);
}

void Hd44780::queueNibble(std::vector<std::uint8_t>& out, std::uint8_t nibble, bool data) const {
    // The controller latches D4–D7 on E's falling edge. Each expander write
    // takes about 90µs on the bus, far longer than E's 450ns minimum pulse.
    const auto pins = static_cast<std::uint8_t>((nibble << 4) | backlight_ | (data ? kRs : 0));
    out.push_back(static_cast<std::uint8_t>(pins | kEnable));
    out.push_back(pins);
}

void Hd44780::command(std::uint8_t value) {
    std::vector<std::uint8_t> bytes;
    queue(bytes, value, false);
    send(bytes);
}

void Hd44780::send(const std::vector<std::uint8_t>& bytes) {
    // Each byte of one I2C write lands on the expander's pins in turn.
    std::vector<char> buffer(bytes.begin(), bytes.end());
    if (const int status = i2cWriteDevice(i2c_, buffer.data(), static_cast<unsigned>(buffer.size()));
        status < 0) {
        throw std::runtime_error("LCD backpack at 0x" + hex(address_) +
                                 " didn't answer (pigpio error " + std::to_string(status) +
                                 "). Check its wiring and the level shifter; `i2cdetect -y 1` should list " +
                                 hex(address_) + ".");
    }
}

}  // namespace winampdeck
