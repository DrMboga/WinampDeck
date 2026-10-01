// stations.csv: the hand-edited Station list.

#include <gtest/gtest.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "core/stations_csv.hpp"

namespace winampdeck {
namespace {

std::vector<Station> parse(const std::string& text) {
    std::istringstream in(text);
    return parseStationsCsv(in);
}

TEST(StationsCsvTest, ReadsStationsInFileOrder) {
    const auto stations = parse(
        "name,url,logo\n"
        "RA Heavy Metal,https://stream.rockantenne.de/heavy-metal/stream/mp3,RA_HM.565\n"
        "NASHE Radio (Moscow),https://nashe1.hostingradio.ru/nashe-128.mp3,Nashe.565\n");

    ASSERT_EQ(stations.size(), 2u);
    EXPECT_EQ(stations[0], (Station{"RA Heavy Metal",
                                    "https://stream.rockantenne.de/heavy-metal/stream/mp3",
                                    "RA_HM.565"}));
    EXPECT_EQ(stations[1].name, "NASHE Radio (Moscow)");
}

TEST(StationsCsvTest, SkipsCommentsAndBlankLinesAndTrimsFields) {
    const auto stations = parse(
        "# Rock\n"
        "\n"
        " name , url , logo \r\n"
        "  Alpha ,  http://alpha.example/stream  ,  alpha.565  \r\n"
        "# Pop\n"
        "\n");

    ASSERT_EQ(stations.size(), 1u);
    EXPECT_EQ(stations[0], (Station{"Alpha", "http://alpha.example/stream", "alpha.565"}));
}

TEST(StationsCsvTest, AcceptsAByteOrderMarkAndNonAsciiNames) {
    const auto stations = parse("\xEF\xBB\xBFname,url,logo\nНаше Радио,http://nashe.example,\n");

    ASSERT_EQ(stations.size(), 1u);
    EXPECT_EQ(stations[0].name, "Наше Радио");
    EXPECT_EQ(stations[0].logo, "");  // A Station without a logo is fine.
}

TEST(StationsCsvTest, ProblemsNameTheLine) {
    EXPECT_THROW(parse(""), StationsCsvError);
    EXPECT_THROW(parse("button,frequency,name,url,logo\n"), StationsCsvError);

    try {
        parse("name,url,logo\nAlpha,http://a,a.565\nBeta,http://b\n");
        FAIL() << "expected StationsCsvError";
    } catch (const StationsCsvError& error) {
        EXPECT_NE(std::string(error.what()).find("line 3"), std::string::npos) << error.what();
    }

    EXPECT_THROW(parse("name,url,logo\n,http://a,a.565\n"), StationsCsvError);
    EXPECT_THROW(parse("name,url,logo\nAlpha,,a.565\n"), StationsCsvError);
    EXPECT_THROW(parse("name,url,logo\nAlpha,http://a,a.565,extra\n"), StationsCsvError);
}

TEST(StationsCsvTest, AMissingFileIsAnError) {
    EXPECT_THROW(loadStationsCsv("does/not/exist/stations.csv"), StationsCsvError);
}

// The list that ships in data/: every Station has a logo that loads.
TEST(StationsCsvTest, TheShippedListParses) {
    const std::filesystem::path data = WINAMPDECK_DATA_DIR;
    const auto stations = loadStationsCsv(data / "stations.csv");
    EXPECT_FALSE(stations.empty());
    for (const Station& station : stations) {
        EXPECT_TRUE(std::filesystem::exists(data / "logos" / station.logo)) << station.logo;
    }
}

}  // namespace
}  // namespace winampdeck
