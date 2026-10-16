#include "adapters/state_file.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

namespace winampdeck {

namespace {

using nlohmann::json;

constexpr const char* kStationKey = "station";

void reportUnreadable(const std::filesystem::path& path, const std::string& why) {
    std::cerr << "Last Station: " << path.string() << " " << why << "; tuning the first Station"
              << std::endl;
}

// Writes all of text to fd and flushes it to the card. False if any step fails.
bool writeAndSync(int fd, const std::string& text) {
    const char* data = text.data();
    std::size_t left = text.size();
    while (left > 0) {
        const ssize_t written = ::write(fd, data, left);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        data += written;
        left -= static_cast<std::size_t>(written);
    }
    return ::fsync(fd) == 0;
}

}  // namespace

StateFile::StateFile(std::filesystem::path directory) : path_(std::move(directory) / "state.json") {}

std::optional<std::size_t> StateFile::rememberedStation() {
    std::error_code error;
    if (!std::filesystem::exists(path_, error)) {
        return std::nullopt;  // Nothing saved yet, which is normal.
    }
    std::ifstream in(path_, std::ios::binary);
    if (!in) {
        reportUnreadable(path_, "can't be opened");
        return std::nullopt;
    }
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};

    const json document = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (document.is_discarded()) {
        reportUnreadable(path_, "isn't valid JSON");
        return std::nullopt;
    }
    if (!document.is_object() || !document.contains(kStationKey)) {
        reportUnreadable(path_, "has no \"station\"");
        return std::nullopt;
    }
    // nlohmann/json keeps non-negative whole numbers as unsigned.
    const json& station = document[kStationKey];
    if (!station.is_number_unsigned()) {
        reportUnreadable(path_, "has a \"station\" that isn't a position (" + station.dump() + ")");
        return std::nullopt;
    }
    return station.get<std::size_t>();
}

void StateFile::rememberStation(std::size_t station) {
    const std::string text = json{{kStationKey, station}}.dump() + "\n";
    const std::filesystem::path temporary = path_.string() + ".tmp";

    const int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        std::cerr << "Last Station: can't save to " << temporary.string() << ": " << std::strerror(errno)
                  << std::endl;
        return;
    }
    const bool written = writeAndSync(fd, text);
    const int writeErrno = errno;
    ::close(fd);
    if (!written) {
        std::cerr << "Last Station: can't save to " << temporary.string() << ": " << std::strerror(writeErrno)
                  << std::endl;
        std::remove(temporary.c_str());
        return;
    }
    if (std::rename(temporary.c_str(), path_.c_str()) != 0) {
        std::cerr << "Last Station: can't replace " << path_.string() << ": " << std::strerror(errno)
                  << std::endl;
        std::remove(temporary.c_str());
        return;
    }
    // The rename itself lives in the directory, which needs flushing too.
    if (const int dir = ::open(path_.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC); dir >= 0) {
        ::fsync(dir);
        ::close(dir);
    }
    std::cout << "Last Station: saved Station " << station << " to " << path_.string() << std::endl;
}

}  // namespace winampdeck
