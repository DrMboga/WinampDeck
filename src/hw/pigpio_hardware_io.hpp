#pragma once

#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#include "core/hardware_io.hpp"

namespace winampdeck {

// The real panel, through pigpio: the 8 buttons and 2 LEDs on the MCP23017
// (I2C1, address 0x20), with presses signalled on its interrupt line (GPIO27)
// rather than polled. Wiring: docs/wiring.md.
//
// The TFT and LCD aren't driven yet (Phases 6 and 7); until then, what they
// would show is printed to stdout.
//
// Listener callbacks run on the io_context's thread, never on pigpio's.
// Requires a live PigpioSession.
class PigpioHardwareIO final : public HardwareIO {
public:
    // How long the buttons must be still before a change counts. Mechanical
    // switches bounce for a few milliseconds when pressed and released.
    static constexpr std::chrono::milliseconds kDebounce{20};

    // Configures the MCP23017 and turns both LEDs off. Throws
    // std::runtime_error if the chip doesn't answer.
    explicit PigpioHardwareIO(asio::io_context& io);
    // Turns both LEDs off.
    ~PigpioHardwareIO() override;

    PigpioHardwareIO(const PigpioHardwareIO&) = delete;
    PigpioHardwareIO& operator=(const PigpioHardwareIO&) = delete;

    void setListener(Listener* listener) override;
    void setLed(Led led, bool on) override;
    void showLcdText(const std::string& text) override;
    void showScreen(const Screen& screen) override;

private:
    // pigpio's alert thread, on any level change of the interrupt line.
    static void onInterruptLine(int gpio, int level, std::uint32_t tick, void* self);
    // The rest run on the io_context's thread.
    void onInputsChanged();
    void onInputsSettled();

    std::uint8_t readRegister(std::uint8_t reg);
    void writeRegister(std::uint8_t reg, std::uint8_t value);

    asio::io_context& io_;
    asio::steady_timer debounce_;
    unsigned i2c_ = 0;
    Listener* listener_ = nullptr;
    std::uint8_t pressed_ = 0;  // Debounced; bit set = pressed, in port A bit order.
    std::uint8_t leds_ = 0;     // Port B output latch.
};

}  // namespace winampdeck
