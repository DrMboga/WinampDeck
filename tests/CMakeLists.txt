include(GoogleTest)

add_executable(winampdeck_tests
    version_test.cpp
    dependencies_test.cpp
    auto_switch_test.cpp
    eject_test.cpp
    radio_test.cpp
    source_switching_test.cpp
    spotify_test.cpp
    station_list_test.cpp
    mcp23017_pins_test.cpp
    asio_scheduler_test.cpp
    stations_csv_test.cpp
    font_test.cpp
    canvas_test.cpp
    image_test.cpp
    tft_view_test.cpp
    cover_art_test.cpp)
target_link_libraries(winampdeck_tests PRIVATE
    winampdeck_core
    winampdeck_ui
    winampdeck_adapters
    winampdeck_warnings
    WinampDeck::asio
    WinampDeck::websocketpp
    WinampDeck::httplib
    nlohmann_json::nlohmann_json
    GTest::gtest_main)

# The Station list and logos that ship in data/, checked by the tests.
target_compile_definitions(winampdeck_tests PRIVATE WINAMPDECK_DATA_DIR="${PROJECT_SOURCE_DIR}/data")

gtest_discover_tests(winampdeck_tests)
