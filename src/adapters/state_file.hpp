#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>

#include "core/station_memory.hpp"

namespace winampdeck {

// The Last Station in DIR/state.json, which only the controller writes
// (ADR 0007): {"station": 12}. Unlike config.json it's read leniently: a
// missing, unreadable or malformed file just means nothing is remembered, and
// the reason goes to the journal. Saving writes a temporary file next to it
// and renames it over, so a power loss leaves the old content or the new.
class StateFile final : public StationMemory {
public:
    explicit StateFile(std::filesystem::path directory);

    std::optional<std::size_t> rememberedStation() override;
    void rememberStation(std::size_t station) override;

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace winampdeck
