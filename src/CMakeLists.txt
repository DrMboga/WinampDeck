# winampdeck_core: the hardware-free decision-making (PlayerController and the
# EngineClient/RadioClient/HardwareIO interfaces it drives). It must never
# link pigpio, sockets, or subprocess code, so the whole state machine stays
# testable in plain CI.
add_library(winampdeck_core
    core/deck_config.cpp
    core/player_controller.cpp
    core/stations_csv.cpp
    core/version.cpp)
target_include_directories(winampdeck_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(winampdeck_core PRIVATE WINAMPDECK_VERSION="${PROJECT_VERSION}")
target_link_libraries(winampdeck_core PRIVATE winampdeck_warnings nlohmann_json::nlohmann_json)

# winampdeck_ui: what the TFT and LCD show, worked out in memory (font, canvas,
# the Now Playing and Station List views, the LCD's scrolling row). Hardware-free
# like the core, so it's tested in CI too; it reaches the glass through the
# ui::Display and ui::LcdDisplay interfaces.
add_library(winampdeck_ui
    ui/canvas.cpp
    ui/display.cpp
    ui/font5x7.cpp
    ui/image.cpp
    ui/lcd_text.cpp
    ui/lcd_view.cpp
    ui/tft_view.cpp)
target_link_libraries(winampdeck_ui
    PUBLIC winampdeck_core
    PRIVATE winampdeck_warnings)

# winampdeck_adapters: production adapters that don't need the Pi (the Asio
# scheduler, album covers downloaded over HTTP, Station logos from disk, the
# go-librespot and mpv clients, Safe Shutdown through systemd).
add_library(winampdeck_adapters
    adapters/cover_art.cpp
    adapters/deck_artwork.cpp
    adapters/go_librespot_client.cpp
    adapters/librespot_event_stream.cpp
    adapters/librespot_events.cpp
    adapters/librespot_rest.cpp
    adapters/mpv_client.cpp
    adapters/stb_image.cpp
    adapters/system_poweroff.cpp)
target_link_libraries(winampdeck_adapters
    PUBLIC winampdeck_ui WinampDeck::asio
    PRIVATE winampdeck_warnings WinampDeck::httplib WinampDeck::websocketpp WinampDeck::stb
            nlohmann_json::nlohmann_json)

# Renders the TFT's screens to .bmp files, to check the look without a Pi.
add_executable(winampdeck-tft-preview
    tools/tft_preview.cpp)
target_link_libraries(winampdeck-tft-preview PRIVATE winampdeck_adapters winampdeck_warnings)

# winampdeck_hw: the pigpio-backed panel (buttons/LEDs on the MCP23017, the
# ST7735 TFT, the HD44780 LCD). Only builds on the Pi.
if(WINAMPDECK_WITH_PIGPIO)
    add_library(winampdeck_hw
        hw/hd44780.cpp
        hw/pigpio_session.cpp
        hw/pigpio_hardware_io.cpp
        hw/st7735.cpp)
    target_link_libraries(winampdeck_hw
        PUBLIC winampdeck_adapters WinampDeck::asio WinampDeck::pigpio
        PRIVATE winampdeck_warnings)

    # Bring-up tool: exercises the real buttons, LEDs, TFT and LCD on their own, or
    # behind PlayerController (see docs/pi-setup.md, Phases 5 to 7).
    add_executable(winampdeck-panel-test
        tools/panel_test.cpp)
    target_link_libraries(winampdeck-panel-test PRIVATE winampdeck_hw winampdeck_warnings)

    # winampdeck: the controller itself, wiring PlayerController to the real
    # panel and Engines (go-librespot, mpv).
    add_executable(winampdeck
        main.cpp)
    target_link_libraries(winampdeck PRIVATE winampdeck_hw winampdeck_warnings)
endif()
