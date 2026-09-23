#include <gtest/gtest.h>

#include "core/version.hpp"

TEST(Version, MatchesCMakeProjectVersion) {
    EXPECT_EQ(winampdeck::version(), "0.1.0");
}
