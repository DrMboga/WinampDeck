#include "core/stations_csv.hpp"

#include <cstddef>
#include <fstream>
#include <string>
#include <string_view>

namespace winampdeck {

namespace {

std::string_view trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

std::vector<std::string_view> splitFields(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (true) {
        const auto comma = line.find(',', start);
        fields.push_back(trim(line.substr(start, comma - start)));
        if (comma == std::string_view::npos) {
            return fields;
        }
        start = comma + 1;
    }
}

[[noreturn]] void fail(std::size_t lineNumber, const std::string& problem) {
    throw StationsCsvError("stations.csv line " + std::to_string(lineNumber) + ": " + problem);
}

}  // namespace

std::vector<Station> parseStationsCsv(std::istream& in) {
    std::vector<Station> stations;
    bool headerSeen = false;
    std::string line;
    for (std::size_t lineNumber = 1; std::getline(in, line); ++lineNumber) {
        std::string_view text = line;
        if (lineNumber == 1 && text.starts_with("\xEF\xBB\xBF")) {
            text.remove_prefix(3);  // UTF-8 byte order mark, as some editors save it.
        }
        text = trim(text);
        if (text.empty() || text.starts_with('#')) {
            continue;
        }

        const auto fields = splitFields(text);
        if (!headerSeen) {
            if (fields.size() != 3 || fields[0] != "name" || fields[1] != "url" ||
                fields[2] != "logo") {
                fail(lineNumber, "expected the header `name,url,logo`");
            }
            headerSeen = true;
            continue;
        }
        if (fields.size() != 3) {
            fail(lineNumber, "expected 3 fields (name,url,logo), found " +
                                 std::to_string(fields.size()));
        }
        if (fields[0].empty()) {
            fail(lineNumber, "the Station has no name");
        }
        if (fields[1].empty()) {
            fail(lineNumber, "the Station has no stream URL");
        }
        stations.push_back({std::string(fields[0]), std::string(fields[1]), std::string(fields[2])});
    }
    if (!headerSeen) {
        throw StationsCsvError("stations.csv is empty");
    }
    return stations;
}

std::vector<Station> loadStationsCsv(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw StationsCsvError("Can't open " + path.string());
    }
    return parseStationsCsv(in);
}

}  // namespace winampdeck
