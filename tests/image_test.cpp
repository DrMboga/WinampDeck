// RGB565 colours, Station logo files, and resampling.

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include "ui/artwork.hpp"
#include "ui/image.hpp"

namespace winampdeck::ui {
namespace {

TEST(ImageTest, Rgb565PacksAndUnpacks) {
    EXPECT_EQ(rgb(255, 255, 255), 0xFFFF);
    EXPECT_EQ(rgb(255, 0, 0), 0xF800);
    EXPECT_EQ(rgb(0, 255, 0), 0x07E0);
    EXPECT_EQ(rgb(0, 0, 255), 0x001F);
    const Rgb888 white = toRgb888(0xFFFF);
    EXPECT_EQ(white.r, 255);
    EXPECT_EQ(white.g, 255);
    EXPECT_EQ(white.b, 255);
}

TEST(ImageTest, RectsIntersect) {
    EXPECT_EQ((Rect{0, 0, 10, 10}.intersect({5, 5, 10, 10})), (Rect{5, 5, 5, 5}));
    EXPECT_TRUE((Rect{0, 0, 10, 10}.intersect({10, 0, 5, 5})).empty());
}

class LogoFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        path_ = std::filesystem::temp_directory_path() /
                ("winampdeck_logo_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
                 ".565");
    }
    void TearDown() override { std::filesystem::remove(path_); }

    void writeBytes(const std::vector<std::uint8_t>& bytes) {
        std::ofstream(path_, std::ios::binary)
            .write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    std::filesystem::path path_;
};

TEST_F(LogoFileTest, LogosAreRawLittleEndianRgb565RowByRow) {
    writeBytes({0x00, 0xF8, 0xE0, 0x07, 0x1F, 0x00, 0xFF, 0xFF});

    const auto image = loadRgb565File(path_, 2, 2);

    ASSERT_TRUE(image);
    EXPECT_EQ(image->at(0, 0), rgb(255, 0, 0));
    EXPECT_EQ(image->at(1, 0), rgb(0, 255, 0));
    EXPECT_EQ(image->at(0, 1), rgb(0, 0, 255));
    EXPECT_EQ(image->at(1, 1), rgb(255, 255, 255));
}

TEST_F(LogoFileTest, AFileOfTheWrongSizeIsRejected) {
    writeBytes({0x00, 0xF8, 0xE0});
    EXPECT_FALSE(loadRgb565File(path_, 2, 2));
    EXPECT_FALSE(loadRgb565File(path_.string() + ".missing", 2, 2));
}

TEST(ImageTest, TheShippedLogosAreAllTheArtworkSize) {
    const std::filesystem::path logos = std::filesystem::path(WINAMPDECK_DATA_DIR) / "logos";
    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(logos)) {
        EXPECT_TRUE(loadRgb565File(entry.path(), Artwork::kSize, Artwork::kSize)) << entry.path();
        ++count;
    }
    EXPECT_GT(count, 0);
}

TEST(ResampleTest, ASolidColourStaysSolid) {
    const Image source(92, 92, rgb(200, 100, 50));
    const Image thumb = resample(source, Artwork::kThumbnailSize, Artwork::kThumbnailSize);
    ASSERT_EQ(thumb.width, Artwork::kThumbnailSize);
    for (const Color pixel : thumb.pixels) {
        EXPECT_EQ(pixel, rgb(200, 100, 50));
    }
}

TEST(ResampleTest, ShrinkingAveragesTheCoveredPixels) {
    // 4x1: black, white, black, white -> 2x1 of mid grey.
    const std::vector<std::uint8_t> source = {0, 0, 0, 255, 255, 255, 0, 0, 0, 255, 255, 255};
    const Image out = resampleRgb888(source, 4, 1, 2, 1);
    EXPECT_EQ(out.at(0, 0), rgb(128, 128, 128));
    EXPECT_EQ(out.at(1, 0), rgb(128, 128, 128));
}

TEST(ResampleTest, UnevenRatiosSplitPixelsBetweenOutputs) {
    // 3x1 -> 2x1: each output covers 1.5 source pixels.
    const std::vector<std::uint8_t> source = {255, 0, 0, 0, 255, 0, 0, 0, 255};
    const Image out = resampleRgb888(source, 3, 1, 2, 1);
    EXPECT_EQ(out.at(0, 0), rgb(170, 85, 0));
    EXPECT_EQ(out.at(1, 0), rgb(0, 85, 170));
}

}  // namespace
}  // namespace winampdeck::ui
