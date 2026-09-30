#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "core/types.hpp"

// Which MCP23017 pin each panel button and LED is wired to (docs/wiring.md,
// "Buttons on perfo boards"). Hardware-free, so the mapping and the edge
// detection built on it are unit-tested without a Pi.
namespace winampdeck::mcp23017 {

// GPA0..GPA7, in bit order. Chosen to match the perfo-board header order, not
// the panel's left-to-right order.
inline constexpr std::array<Button, 8> kButtonOnPortABit = {
    Button::Repeat, Button::Shuffle, Button::Eject, Button::Next,
    Button::Stop,   Button::Pause,   Button::Play,  Button::Previous,
};

// Bit mask of the LED's pin on port B (GPB0 = Repeat, GPB1 = Shuffle).
constexpr std::uint8_t ledMask(Led led) {
    return led == Led::Repeat ? std::uint8_t{0x01} : std::uint8_t{0x02};
}

// Port B pins wired as LED outputs; the rest are unconnected.
inline constexpr std::uint8_t kLedPins = 0x03;

struct ButtonEdge {
    Button button;
    bool pressed;

    bool operator==(const ButtonEdge&) const = default;
};

// Buttons are active-low (each switches its pin to GND), so a port A reading
// is inverted to get a "bit set = pressed" mask.
constexpr std::uint8_t pressedMask(std::uint8_t portA) {
    return static_cast<std::uint8_t>(~portA);
}

// The press/release edges between two debounced "bit set = pressed" masks, in
// bit order.
inline std::vector<ButtonEdge> buttonEdges(std::uint8_t before, std::uint8_t after) {
    std::vector<ButtonEdge> edges;
    for (unsigned bit = 0; bit < kButtonOnPortABit.size(); ++bit) {
        const unsigned mask = 1U << bit;
        if ((before & mask) != (after & mask)) {
            edges.push_back({kButtonOnPortABit[bit], (after & mask) != 0});
        }
    }
    return edges;
}

}  // namespace winampdeck::mcp23017
