#include "hw/st7735.hpp"

#include <pigpio.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace winampdeck {

namespace {

constexpr unsigned kSpiChannel = 0;  // CE0, GPIO8.
constexpr unsigned kRsGpio = 23;     // Low: command, high: data.
constexpr unsigned kResetGpio = 24;  // Active low.
// PWM0, but pigpio's hardware PWM isn't available: PigpioSession has the PWM
// peripheral pacing pigpio's DMA, to keep it off the HAT's PCM/I2S block. So
// the backlight uses pigpio's DMA-timed PWM instead, which works on any GPIO.
constexpr unsigned kBacklightGpio = 12;
constexpr unsigned kBacklightFrequency = 800;  // Hz; pigpio's default, and flicker-free.

// pigpio sends SPI in one go per call; keep each call modest.
constexpr std::size_t kMaxTransfer = 4096;

// ST7735 commands (datasheet section 10.1).
namespace cmd {
constexpr std::uint8_t kSleepOut = 0x11;
constexpr std::uint8_t kNormalMode = 0x13;
constexpr std::uint8_t kInversionOff = 0x20;
constexpr std::uint8_t kDisplayOff = 0x28;
constexpr std::uint8_t kDisplayOn = 0x29;
constexpr std::uint8_t kColumnAddress = 0x2A;
constexpr std::uint8_t kRowAddress = 0x2B;
constexpr std::uint8_t kMemoryWrite = 0x2C;
constexpr std::uint8_t kMemoryAccess = 0x36;  // MADCTL.
constexpr std::uint8_t kPixelFormat = 0x3A;   // COLMOD.
constexpr std::uint8_t kSleepIn = 0x10;
}  // namespace cmd

constexpr std::uint8_t kPixelFormat16Bit = 0x05;

std::uint8_t high(int value) { return static_cast<std::uint8_t>((value >> 8) & 0xFF); }
std::uint8_t low(int value) { return static_cast<std::uint8_t>(value & 0xFF); }

}  // namespace

St7735::St7735(const Config& config) : config_(config) {
    const int handle = spiOpen(kSpiChannel, config_.spiBaud, 0);  // Mode 0, CE0 active low.
    if (handle < 0) {
        throw std::runtime_error("Can't open SPI0 for the TFT (pigpio error " +
                                 std::to_string(handle) + ").");
    }
    spi_ = static_cast<unsigned>(handle);

    gpioSetMode(kRsGpio, PI_OUTPUT);
    gpioSetMode(kResetGpio, PI_OUTPUT);
    gpioSetMode(kBacklightGpio, PI_OUTPUT);
    gpioWrite(kBacklightGpio, 0);  // Dark until there's something to see.

    // Hardware reset: RES low for at least 10µs, then up to 120ms to settle.
    gpioWrite(kResetGpio, 1);
    gpioDelay(5'000);
    gpioWrite(kResetGpio, 0);
    gpioDelay(20'000);
    gpioWrite(kResetGpio, 1);
    gpioDelay(150'000);

    command(cmd::kSleepOut);
    gpioDelay(120'000);  // The datasheet's wait before the next command.
    command(cmd::kPixelFormat, {kPixelFormat16Bit});
    command(cmd::kMemoryAccess, {config_.madctl});
    command(cmd::kInversionOff);
    command(cmd::kNormalMode);

    // Clear the controller's memory before showing it: it powers up with noise.
    const std::vector<ui::Color> black(static_cast<std::size_t>(kWidth * kHeight), 0);
    write({0, 0, kWidth, kHeight}, black);

    command(cmd::kDisplayOn);
    gpioDelay(20'000);

    gpioSetPWMfrequency(kBacklightGpio, kBacklightFrequency);
    gpioSetPWMrange(kBacklightGpio, 255);
    setBrightness(config_.brightness);
}

St7735::~St7735() {
    gpioPWM(kBacklightGpio, 0);
    gpioWrite(kBacklightGpio, 0);
    command(cmd::kDisplayOff);
    command(cmd::kSleepIn);
    spiClose(spi_);
}

void St7735::setBrightness(unsigned brightness) {
    config_.brightness = std::min(brightness, 255u);
    gpioPWM(kBacklightGpio, config_.brightness);
}

void St7735::write(const ui::Rect& area, std::span<const ui::Color> pixels) {
    const ui::Rect visible = area.intersect({0, 0, kWidth, kHeight});
    if (visible.empty() || visible != area ||
        pixels.size() != static_cast<std::size_t>(area.width * area.height)) {
        return;  // Callers only ever write whole, on-screen areas.
    }

    setWindow(area);
    command(cmd::kMemoryWrite);

    // The controller takes each pixel high byte first.
    std::vector<std::uint8_t> bytes;
    bytes.reserve(pixels.size() * 2);
    for (const ui::Color pixel : pixels) {
        bytes.push_back(static_cast<std::uint8_t>(pixel >> 8));
        bytes.push_back(static_cast<std::uint8_t>(pixel & 0xFF));
    }
    sendData(bytes.data(), bytes.size());
}

void St7735::command(std::uint8_t command, std::initializer_list<std::uint8_t> data) {
    gpioWrite(kRsGpio, 0);
    char byte = static_cast<char>(command);
    spiWrite(spi_, &byte, 1);
    if (data.size() > 0) {
        sendData(data.begin(), data.size());
    }
}

void St7735::setWindow(const ui::Rect& area) {
    const int x0 = area.x + config_.columnOffset;
    const int x1 = area.right() - 1 + config_.columnOffset;
    const int y0 = area.y + config_.rowOffset;
    const int y1 = area.bottom() - 1 + config_.rowOffset;
    command(cmd::kColumnAddress, {high(x0), low(x0), high(x1), low(x1)});
    command(cmd::kRowAddress, {high(y0), low(y0), high(y1), low(y1)});
}

void St7735::sendData(const std::uint8_t* bytes, std::size_t count) {
    gpioWrite(kRsGpio, 1);
    // pigpio's API takes a non-const buffer it doesn't modify for writes.
    char* data = const_cast<char*>(reinterpret_cast<const char*>(bytes));
    for (std::size_t sent = 0; sent < count;) {
        const std::size_t chunk = std::min(kMaxTransfer, count - sent);
        spiWrite(spi_, data + sent, static_cast<unsigned>(chunk));
        sent += chunk;
    }
}

}  // namespace winampdeck
