cmake_minimum_required(VERSION 3.24)

project(WinampDeck
    VERSION 0.1.0
    DESCRIPTION "Controller for the WinampDeck hardware media player"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WINAMPDECK_BUILD_TESTS "Build the unit test suite" ON)
# pigpio only makes sense on the Pi itself (it maps the SoC's peripheral
# registers directly), so the hardware adapters are opt-in. Everything else —
# including the whole test suite — builds and runs on any Linux box.
option(WINAMPDECK_WITH_PIGPIO "Link the pigpio-backed HardwareIO adapter (Raspberry Pi only)" OFF)

include(cmake/Dependencies.cmake)

# Warnings for our own code only; third-party headers are pulled in as SYSTEM.
add_library(winampdeck_warnings INTERFACE)
target_compile_options(winampdeck_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)

add_subdirectory(src)

if(WINAMPDECK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
