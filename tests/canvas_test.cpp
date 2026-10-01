// Drawing into an Image, and finding what changed between two frames.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ui/canvas.hpp"
#include "ui/display.hpp"

namespace winampdeck::ui {
namespace {

constexpr Color kInk = rgb(255, 255, 255);

// The image as rows of '#' (any non-black pixel) and '.'.
std::vector<std::string> ascii(const Image& image, const Rect& area) {
    std::vector<std::string> rows;
    for (int y = area.y; y < area.bottom(); ++y) {
        std::string row;
        for (int x = area.x; x < area.right(); ++x) {
            row += image.at(x, y) != 0 ? '#' : '.';
        }
        rows.push_back(row);
    }
    return rows;
}

TEST(CanvasTest, DrawsTextGlyphByGlyph) {
    Image image(12, 7);
    Canvas canvas(image);

    const int end = canvas.text(0, 0, "LI", kInk);

    EXPECT_EQ(end, 12);
    EXPECT_EQ(ascii(image, {0, 0, 12, 7}), (std::vector<std::string>{
                                               "#......###..",
                                               "#.......#...",
                                               "#.......#...",
                                               "#.......#...",
                                               "#.......#...",
                                               "#.......#...",
                                               "#####..###..",
                                           }));
}

TEST(CanvasTest, ScaledTextDrawsEachPixelAsABlock) {
    Image image(10, 14);
    Canvas canvas(image);

    canvas.text(0, 0, "L", kInk, 2);

    EXPECT_EQ(ascii(image, {0, 0, 10, 2}), (std::vector<std::string>{"##........", "##........"}));
    EXPECT_EQ(ascii(image, {0, 12, 10, 2}), (std::vector<std::string>{"##########", "##########"}));
}

TEST(CanvasTest, CyrillicAndUmlautsRender) {
    Image image(18, 7);
    Canvas canvas(image);
    canvas.text(0, 0, "ЖÖ?", kInk);
    // Top rows: Ж's three strokes, Ö's two dots, the top of ?.
    EXPECT_EQ(ascii(image, {0, 0, 18, 1})[0], "#.#.#..#.#...###..");
}

TEST(CanvasTest, EverythingIsClipped) {
    Image image(8, 8);
    Canvas canvas(image);

    canvas.setClip({2, 2, 4, 4});
    canvas.fill(kInk);
    canvas.text(-3, 0, "WWWW", kInk);
    canvas.resetClip();
    canvas.fill({-5, 7, 100, 100}, kInk);  // Off the image: clipped, not a crash.

    EXPECT_EQ(ascii(image, {0, 0, 8, 8}), (std::vector<std::string>{
                                             "........",
                                             "........",
                                             "..####..",
                                             "..####..",
                                             "..####..",
                                             "..####..",
                                             "........",
                                             "########",
                                         }));
}

TEST(CanvasTest, BlitCopiesAnImageClipped) {
    Image image(4, 4);
    Image sprite(3, 3, kInk);
    sprite.at(1, 1) = 0;
    Canvas canvas(image);

    canvas.blit(sprite, 2, 2);

    EXPECT_EQ(ascii(image, {0, 0, 4, 4}),
              (std::vector<std::string>{"....", "....", "..##", "..#."}));
}

TEST(CanvasTest, TextWidthIncludesTheGapAfterEachCharacter) {
    EXPECT_EQ(Canvas::textWidth(0), 0);
    EXPECT_EQ(Canvas::textWidth(26), 156);
    EXPECT_EQ(Canvas::textWidth(5, 2), 60);
}

TEST(ChangedAreasTest, NothingChangedMeansNothingToSend) {
    const Image frame(160, 128, kInk);
    EXPECT_TRUE(changedAreas(frame, frame).empty());
}

TEST(ChangedAreasTest, ChangesAreBoundedPerEightRowBand) {
    const Image before(160, 128);
    Image after = before;
    after.at(10, 3) = kInk;
    after.at(20, 5) = kInk;    // Same band as the first.
    after.at(150, 100) = kInk; // Band 12.

    EXPECT_EQ(changedAreas(before, after),
              (std::vector<Rect>{{10, 3, 11, 3}, {150, 100, 1, 1}}));
}

TEST(ChangedAreasTest, CropCopiesTheAreaRowByRow) {
    Image image(4, 3);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            image.at(x, y) = static_cast<Color>(y * 10 + x);
        }
    }
    EXPECT_EQ(crop(image, {1, 1, 2, 2}), (std::vector<Color>{11, 12, 21, 22}));
}

}  // namespace
}  // namespace winampdeck::ui
