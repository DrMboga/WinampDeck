#pragma once

#include <string_view>

namespace winampdeck {

// The controller's version, from the top-level CMake project() call.
std::string_view version();

}  // namespace winampdeck
