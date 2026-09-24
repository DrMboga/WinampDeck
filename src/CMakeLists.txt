# winampdeck_core: the hardware-free decision-making (PlayerController and the
# EngineClient/RadioClient/HardwareIO interfaces it drives). It must never
# link pigpio, sockets, or subprocess code, so the whole state machine stays
# testable in plain CI.
add_library(winampdeck_core
    core/player_controller.cpp
    core/version.cpp)
target_include_directories(winampdeck_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(winampdeck_core PRIVATE WINAMPDECK_VERSION="${PROJECT_VERSION}")
target_link_libraries(winampdeck_core PRIVATE winampdeck_warnings)

# winampdeck: the controller binary that runs on the Pi, wiring the core to the
# real adapters (go-librespot, mpv, pigpio).
add_executable(winampdeck
    main.cpp)
target_link_libraries(winampdeck PRIVATE winampdeck_core winampdeck_warnings)
if(WINAMPDECK_WITH_PIGPIO)
    target_link_libraries(winampdeck PRIVATE WinampDeck::pigpio)
endif()
