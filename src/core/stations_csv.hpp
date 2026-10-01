#pragma once

#include <filesystem>
#include <istream>
#include <stdexcept>
#include <vector>

#include "core/types.hpp"

namespace winampdeck {

// A stations.csv that can't be used, with the line at fault in the message.
class StationsCsvError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Parses the hand-edited Station list: a `name,url,logo` header, then one
// Station per line in that order. `logo` is a filename in the logos directory
// next to stations.csv, and may be left empty. Blank lines and lines starting
// with `#` are skipped. There is no quoting, so no field may contain a comma.
// Throws StationsCsvError.
std::vector<Station> parseStationsCsv(std::istream& in);

// Reads and parses the file. Throws StationsCsvError, including when the file
// can't be opened.
std::vector<Station> loadStationsCsv(const std::filesystem::path& path);

}  // namespace winampdeck
