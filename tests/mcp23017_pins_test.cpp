// The MCP23017 pin map and button edge detection, per docs/wiring.md.

#include <gtest/gtest.h>

#include <vector>

#include "hw/mcp23017_pins.hpp"

namespace winampdeck::mcp23017 {
namespace {

using Edges = std::vector<ButtonEdge>;

TEST(Mcp23017PinsTest, ButtonsFollowThePerfoBoardHeaderOrder) {
    const std::vector<Button> expected = {Button::Repeat, Button::Shuffle, Button::Eject,
                                          Button::Next,   Button::Stop,    Button::Pause,
                                          Button::Play,   Button::Previous};
    for (unsigned bit = 0; bit < 8; ++bit) {
        EXPECT_EQ(buttonEdges(0x00, static_cast<std::uint8_t>(1U << bit)),
                  (Edges{{expected[bit], true}}))
            << "GPA" << bit;
    }
}

TEST(Mcp23017PinsTest, LedsAreOnGpb0AndGpb1) {
    EXPECT_EQ(ledMask(Led::Repeat), 0x01);
    EXPECT_EQ(ledMask(Led::Shuffle), 0x02);
    EXPECT_EQ(kLedPins, 0x03);
}

TEST(Mcp23017PinsTest, ButtonsAreActiveLow) {
    EXPECT_EQ(pressedMask(0xFF), 0x00);           // Idle: every pin pulled up.
    EXPECT_EQ(pressedMask(0b1111'1011), 0x04);    // Eject (GPA2) held to GND.
}

TEST(Mcp23017PinsTest, NoChangeMeansNoEdges) {
    EXPECT_TRUE(buttonEdges(0x00, 0x00).empty());
    EXPECT_TRUE(buttonEdges(0x24, 0x24).empty());
}

TEST(Mcp23017PinsTest, ReleaseIsReported) {
    EXPECT_EQ(buttonEdges(0x04, 0x00), (Edges{{Button::Eject, false}}));
}

TEST(Mcp23017PinsTest, SimultaneousChangesAreReportedInBitOrder) {
    // Eject released, Shuffle and Play pressed.
    EXPECT_EQ(buttonEdges(0x04, 0x42),
              (Edges{{Button::Shuffle, true}, {Button::Eject, false}, {Button::Play, true}}));
}

}  // namespace
}  // namespace winampdeck::mcp23017
