#include "hw/pigpio_hardware_io.hpp"

#include <pigpio.h>

#include <asio/post.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>

#include "hw/mcp23017_pins.hpp"

namespace winampdeck {

namespace {

constexpr unsigned kI2cBus = 1;
constexpr unsigned kMcp23017Address = 0x20;
constexpr unsigned kInterruptGpio = 27;

// MCP23017 registers, in the power-on IOCON.BANK = 0 layout.
namespace reg {
constexpr std::uint8_t kIodirA = 0x00;
constexpr std::uint8_t kIodirB = 0x01;
constexpr std::uint8_t kIpolA = 0x02;
constexpr std::uint8_t kIpolB = 0x03;
constexpr std::uint8_t kGpintenA = 0x04;
constexpr std::uint8_t kGpintenB = 0x05;
constexpr std::uint8_t kIntconA = 0x08;
constexpr std::uint8_t kIocon = 0x0A;
constexpr std::uint8_t kGppuA = 0x0C;
constexpr std::uint8_t kGppuB = 0x0D;
constexpr std::uint8_t kGpioA = 0x12;
constexpr std::uint8_t kOlatB = 0x15;
}  // namespace reg

// IOCON.MIRROR: either port's changes drive both INTA and INTB, so it doesn't
// matter which one the breakout's combined interrupt pin is. The rest stays at
// its power-on default: active-low, push-pull interrupt output.
constexpr std::uint8_t kIoconMirror = 0x40;

std::string_view toString(Source source) {
    switch (source) {
    case Source::Stopped: return "Stopped";
    case Source::Spotify: return "Spotify";
    case Source::Radio: return "Internet Radio";
    }
    return "?";
}

std::string_view toString(PlaybackStatus status) {
    switch (status) {
    case PlaybackStatus::Stopped: return "stopped";
    case PlaybackStatus::Playing: return "playing";
    case PlaybackStatus::Paused: return "paused";
    }
    return "?";
}

void describe(std::ostream& out, const Screen& screen) {
    if (const auto* list = std::get_if<StationListScreen>(&screen)) {
        out << "Station List, " << list->stations.size() << " Stations";
        if (list->selected < list->stations.size()) {
            out << ", highlighted: " << list->stations[list->selected].name;
        }
        return;
    }
    const auto& nowPlaying = std::get<NowPlayingScreen>(screen);
    out << "Now Playing, " << toString(nowPlaying.source);
    if (nowPlaying.source != Source::Stopped) {
        out << ", " << toString(nowPlaying.status);
    }
    if (!nowPlaying.station.empty()) {
        out << ", Station: " << nowPlaying.station;
    }
}

}  // namespace

PigpioHardwareIO::PigpioHardwareIO(asio::io_context& io) : io_(io), debounce_(io) {
    const int handle = i2cOpen(kI2cBus, kMcp23017Address, 0);
    if (handle < 0) {
        throw std::runtime_error("Can't open I2C bus 1 (pigpio error " + std::to_string(handle) +
                                 "). Is I2C enabled?");
    }
    i2c_ = static_cast<unsigned>(handle);

    try {
        writeRegister(reg::kIocon, kIoconMirror);

        // Port A: the 8 buttons. Inputs with pull-ups (each button switches its
        // pin to GND), interrupting on any change.
        writeRegister(reg::kIodirA, 0xFF);
        writeRegister(reg::kIpolA, 0x00);
        writeRegister(reg::kGppuA, 0xFF);
        writeRegister(reg::kIntconA, 0x00);  // Compare against the previous value.
        writeRegister(reg::kGpintenA, 0xFF);

        // Port B: the 2 LEDs, off. The unconnected pins stay inputs, pulled up
        // so they don't float.
        writeRegister(reg::kOlatB, leds_);
        writeRegister(reg::kIodirB, static_cast<std::uint8_t>(~mcp23017::kLedPins));
        writeRegister(reg::kIpolB, 0x00);
        writeRegister(reg::kGppuB, static_cast<std::uint8_t>(~mcp23017::kLedPins));
        writeRegister(reg::kGpintenB, 0x00);

        // Buttons already held at startup don't count as presses. Reading the
        // port also clears any interrupt left pending from before.
        pressed_ = mcp23017::pressedMask(readRegister(reg::kGpioA));
    } catch (...) {
        i2cClose(i2c_);
        throw;
    }

    gpioSetMode(kInterruptGpio, PI_INPUT);
    gpioSetPullUpDown(kInterruptGpio, PI_PUD_UP);
    gpioSetAlertFuncEx(kInterruptGpio, &PigpioHardwareIO::onInterruptLine, this);
    // A button that changed since the read above has already pulled the line
    // low, and alerts only report changes.
    if (gpioRead(kInterruptGpio) == 0) {
        asio::post(io_, [this] { onInputsChanged(); });
    }
}

PigpioHardwareIO::~PigpioHardwareIO() {
    gpioSetAlertFuncEx(kInterruptGpio, nullptr, nullptr);
    i2cWriteByteData(i2c_, reg::kOlatB, 0x00);
    i2cClose(i2c_);
}

void PigpioHardwareIO::setListener(Listener* listener) {
    listener_ = listener;
}

void PigpioHardwareIO::setLed(Led led, bool on) {
    const std::uint8_t mask = mcp23017::ledMask(led);
    leds_ = static_cast<std::uint8_t>(on ? leds_ | mask : leds_ & ~mask);
    writeRegister(reg::kOlatB, leds_);
}

void PigpioHardwareIO::showLcdText(const std::string& text) {
    std::cout << "  LCD: " << (text.empty() ? "(blank)" : text) << std::endl;
}

void PigpioHardwareIO::showScreen(const Screen& screen) {
    std::cout << "  TFT: ";
    describe(std::cout, screen);
    std::cout << std::endl;
}

void PigpioHardwareIO::onInterruptLine(int /*gpio*/, int level, std::uint32_t /*tick*/, void* self) {
    // The line is active-low: 0 means the MCP23017 has seen a change. Hand it
    // over to the event loop; I2C and listeners are only touched from there.
    if (level == 0) {
        auto* hardware = static_cast<PigpioHardwareIO*>(self);
        asio::post(hardware->io_, [hardware] { hardware->onInputsChanged(); });
    }
}

void PigpioHardwareIO::onInputsChanged() {
    // Reading the port releases the interrupt line, so each further bounce
    // raises it again and pushes the deadline back.
    readRegister(reg::kGpioA);
    debounce_.expires_after(kDebounce);
    debounce_.async_wait([this](const std::error_code& error) {
        if (!error) {
            onInputsSettled();
        }
    });
}

void PigpioHardwareIO::onInputsSettled() {
    const std::uint8_t pressed = mcp23017::pressedMask(readRegister(reg::kGpioA));
    const auto edges = mcp23017::buttonEdges(pressed_, pressed);
    pressed_ = pressed;
    for (const auto& edge : edges) {
        if (listener_ == nullptr) {
            break;
        }
        if (edge.pressed) {
            listener_->onButtonPressed(edge.button);
        } else {
            listener_->onButtonReleased(edge.button);
        }
    }

    // If a change landed between that read and pigpio sampling the line, the
    // line may never have gone high, so no new alert would come.
    if (gpioRead(kInterruptGpio) == 0) {
        onInputsChanged();
    }
}

std::uint8_t PigpioHardwareIO::readRegister(std::uint8_t reg) {
    const int value = i2cReadByteData(i2c_, reg);
    if (value < 0) {
        throw std::runtime_error("MCP23017 at 0x20 didn't answer (pigpio error " +
                                 std::to_string(value) +
                                 "). Check its wiring; `i2cdetect -y 1` should list 20.");
    }
    return static_cast<std::uint8_t>(value);
}

void PigpioHardwareIO::writeRegister(std::uint8_t reg, std::uint8_t value) {
    if (const int status = i2cWriteByteData(i2c_, reg, value); status < 0) {
        throw std::runtime_error("MCP23017 at 0x20 didn't answer (pigpio error " +
                                 std::to_string(status) +
                                 "). Check its wiring; `i2cdetect -y 1` should list 20.");
    }
}

}  // namespace winampdeck
