include(GoogleTest)

add_executable(winampdeck_tests
    version_test.cpp
    dependencies_test.cpp
    auto_switch_test.cpp
    eject_test.cpp
    radio_test.cpp
    source_switching_test.cpp
    spotify_test.cpp
    station_list_test.cpp)
target_link_libraries(winampdeck_tests PRIVATE
    winampdeck_core
    winampdeck_warnings
    WinampDeck::asio
    WinampDeck::websocketpp
    WinampDeck::httplib
    nlohmann_json::nlohmann_json
    GTest::gtest_main)

gtest_discover_tests(winampdeck_tests)
