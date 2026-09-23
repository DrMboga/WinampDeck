# Third-party dependencies, per ADR 0003 (docs/adr/0003-cpp-controller-dependencies.md).
#
# Everything except pigpio is header-only and fetched at configure time, pinned
# to an exact release/commit so the Pi and CI always build the same code. The
# header-only libraries are downloaded only (SOURCE_SUBDIR points at a directory
# with no CMakeLists.txt) and wrapped in our own INTERFACE targets, so their own
# build systems — some of which predate modern CMake — never run.

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

find_package(Threads REQUIRED)

# --- standalone Asio ---------------------------------------------------------
# Pinned below 1.33: websocketpp still uses asio::io_service, which Asio 1.33
# removed. Revisit together with websocketpp.
FetchContent_Declare(asio
    URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-30-2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _no_cmake)
FetchContent_MakeAvailable(asio)

add_library(winampdeck_asio INTERFACE)
add_library(WinampDeck::asio ALIAS winampdeck_asio)
target_include_directories(winampdeck_asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(winampdeck_asio INTERFACE ASIO_STANDALONE)
target_link_libraries(winampdeck_asio INTERFACE Threads::Threads)

# --- websocketpp ---------------------------------------------------------------
# The 0.8.2 release (2020) doesn't compile as C++20 (simple-template-id
# constructors, removed in C++20); the fix only exists on `develop`, so this is
# pinned to a develop commit rather than a tag.
FetchContent_Declare(websocketpp
    URL https://github.com/zaphoyd/websocketpp/archive/bce258674b2d544f981da4bf149244a76e8126c9.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _no_cmake)
FetchContent_MakeAvailable(websocketpp)

add_library(winampdeck_websocketpp INTERFACE)
add_library(WinampDeck::websocketpp ALIAS winampdeck_websocketpp)
target_include_directories(winampdeck_websocketpp SYSTEM INTERFACE ${websocketpp_SOURCE_DIR})
target_compile_definitions(winampdeck_websocketpp INTERFACE _WEBSOCKETPP_CPP11_STL_)  # std::, not Boost
target_link_libraries(winampdeck_websocketpp INTERFACE WinampDeck::asio)

# --- cpp-httplib ---------------------------------------------------------------
# Plain HTTP to localhost only (go-librespot's REST API), so no OpenSSL/zlib.
FetchContent_Declare(httplib
    URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.58.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _no_cmake)
FetchContent_MakeAvailable(httplib)

add_library(winampdeck_httplib INTERFACE)
add_library(WinampDeck::httplib ALIAS winampdeck_httplib)
target_include_directories(winampdeck_httplib SYSTEM INTERFACE ${httplib_SOURCE_DIR})
target_link_libraries(winampdeck_httplib INTERFACE Threads::Threads)

# --- nlohmann/json -------------------------------------------------------------
# Its own CMake is modern and well-behaved, so use the official target directly.
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(json)

# --- pigpio ----------------------------------------------------------------------
# A system library installed on the Pi (see docs/pi-setup.md), not fetched: it
# drives the SoC's peripherals directly and is useless on any other machine.
if(WINAMPDECK_WITH_PIGPIO)
    find_path(PIGPIO_INCLUDE_DIR pigpio.h REQUIRED)
    find_library(PIGPIO_LIBRARY pigpio REQUIRED)

    add_library(winampdeck_pigpio INTERFACE)
    add_library(WinampDeck::pigpio ALIAS winampdeck_pigpio)
    target_include_directories(winampdeck_pigpio SYSTEM INTERFACE ${PIGPIO_INCLUDE_DIR})
    target_link_libraries(winampdeck_pigpio INTERFACE ${PIGPIO_LIBRARY} Threads::Threads)
    target_compile_definitions(winampdeck_pigpio INTERFACE WINAMPDECK_WITH_PIGPIO)
endif()

# --- GoogleTest (tests only) -----------------------------------------------------
if(WINAMPDECK_BUILD_TESTS)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()
