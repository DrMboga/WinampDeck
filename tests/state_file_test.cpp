// state.json: the Last Station on disk, read leniently and saved atomically.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

#include "adapters/state_file.hpp"

namespace winampdeck {
namespace {

class StateFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        directory_ = std::filesystem::temp_directory_path() /
                     ("winampdeck_state_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
                      "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::remove_all(directory_);
        std::filesystem::create_directories(directory_);
    }
    void TearDown() override { std::filesystem::remove_all(directory_); }

    void write(const std::string& text) { std::ofstream(directory_ / "state.json", std::ios::binary) << text; }

    std::string read() const {
        std::ifstream in(directory_ / "state.json", std::ios::binary);
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }

    std::filesystem::path directory_;
};

TEST_F(StateFileTest, IsStateJsonInTheDirectory) {
    EXPECT_EQ(StateFile(directory_).path(), directory_ / "state.json");
}

TEST_F(StateFileTest, RoundTrip) {
    StateFile(directory_).rememberStation(12);

    EXPECT_EQ(StateFile(directory_).rememberedStation(), 12u);
}

TEST_F(StateFileTest, SavesSmallReadableJson) {
    StateFile(directory_).rememberStation(12);

    EXPECT_EQ(read(), "{\"station\":12}\n");
}

TEST_F(StateFileTest, ASaveReplacesTheOldValueAndLeavesNoTemporaryFile) {
    StateFile state(directory_);
    state.rememberStation(3);
    state.rememberStation(0);

    EXPECT_EQ(state.rememberedStation(), 0u);
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(directory_), std::filesystem::directory_iterator()),
              1);
}

TEST_F(StateFileTest, MissingFileRemembersNothing) {
    EXPECT_EQ(StateFile(directory_).rememberedStation(), std::nullopt);
}

TEST_F(StateFileTest, MissingDirectoryRemembersNothing) {
    EXPECT_EQ(StateFile(directory_ / "nowhere").rememberedStation(), std::nullopt);
}

TEST_F(StateFileTest, EmptyFileRemembersNothing) {
    write("");
    EXPECT_EQ(StateFile(directory_).rememberedStation(), std::nullopt);
}

TEST_F(StateFileTest, InvalidJsonRemembersNothing) {
    write("{\"station\": 12");
    EXPECT_EQ(StateFile(directory_).rememberedStation(), std::nullopt);
}

TEST_F(StateFileTest, WrongShapeRemembersNothing) {
    for (const char* text : {"12", "[12]", "{}", "{\"stations\": 12}", "null"}) {
        write(text);
        EXPECT_EQ(StateFile(directory_).rememberedStation(), std::nullopt) << text;
    }
}

TEST_F(StateFileTest, WrongTypeRemembersNothing) {
    for (const char* text : {R"({"station": "12"})", R"({"station": 1.5})", R"({"station": true})",
                             R"({"station": null})"}) {
        write(text);
        EXPECT_EQ(StateFile(directory_).rememberedStation(), std::nullopt) << text;
    }
}

TEST_F(StateFileTest, NegativeNumberRemembersNothing) {
    write(R"({"station": -1})");
    EXPECT_EQ(StateFile(directory_).rememberedStation(), std::nullopt);
}

TEST_F(StateFileTest, ExtraKeysAreIgnored) {
    write(R"({"station": 5, "note": "hand-edited"})");
    EXPECT_EQ(StateFile(directory_).rememberedStation(), 5u);
}

TEST_F(StateFileTest, APositionPastTheEndIsStillReturned) {
    // Whether it's past the end of the list is PlayerController's call.
    write(R"({"station": 100000})");
    EXPECT_EQ(StateFile(directory_).rememberedStation(), 100000u);
}

TEST_F(StateFileTest, AFailedSaveDoesNotThrow) {
    // A directory that doesn't exist can't be written into, even as root.
    StateFile state(directory_ / "nowhere");

    EXPECT_NO_THROW(state.rememberStation(4));
    EXPECT_EQ(state.rememberedStation(), std::nullopt);
}

TEST_F(StateFileTest, AFailedSaveKeepsTheOldValue) {
    write(R"({"station": 7})");
    // The temporary file's name taken by a directory makes the save fail.
    std::filesystem::create_directory(directory_ / "state.json.tmp");

    EXPECT_NO_THROW(StateFile(directory_).rememberStation(4));
    EXPECT_EQ(StateFile(directory_).rememberedStation(), 7u);
}

}  // namespace
}  // namespace winampdeck
