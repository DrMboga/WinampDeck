#pragma once

#include <cstddef>
#include <optional>

namespace winampdeck {

// Where the Last Station is kept across power-offs (ADR 0007): a position in
// the Station list, counted from zero. PlayerController decides when it's read
// and saved, and what a position past the end of the list means.
class StationMemory {
public:
    virtual ~StationMemory() = default;

    // The remembered position, or nothing if none was ever saved or it can't
    // be read. Never throws.
    virtual std::optional<std::size_t> rememberedStation() = 0;

    // Remember this position for the next start. A failure is reported by the
    // implementation and otherwise ignored; never throws.
    virtual void rememberStation(std::size_t station) = 0;
};

}  // namespace winampdeck
