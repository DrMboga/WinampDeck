# Dependency list for the C++ controller

Status: accepted

The original brainstorm's library list (Asio, libcurl, nlohmann/json, SQLite3, libgpiod) predates several architecture changes since: go-librespot replaces mainline librespot ([ADR 0001](0001-use-go-librespot-for-spotify-connect.md)), and SQLite plus the web frontend are dropped entirely ([ADR 0002](0002-drop-sqlite-and-frontend.md)). This records the list matching the current design.

- **standalone Asio** — the controller's single event loop: timers (LCD scroll, Eject long-press detection), the Unix-domain socket to mpv's JSON IPC, and the transport for the WebSocket client below. Nothing else in the stack needs Boost, so standalone Asio over Boost.Asio.
- **websocketpp** — WebSocket client for go-librespot's `/events` stream, layered on the same Asio `io_context` rather than running a second event loop.
- **cpp-httplib** — REST client for go-librespot's control endpoints (play/pause/next/prev/seek). Replaces libcurl: with the Spotify Web API gone, the controller never talks to the real internet directly (go-librespot and mpv each manage their own internet connections independently) — everything it calls is unencrypted localhost, so a header-only client with no TLS/system dependency fits better than libcurl.
- **nlohmann/json** — parsing go-librespot's REST/WS payloads, mpv's JSON IPC, and the controller's own config file (reused rather than adding a TOML/INI parser for something read once at startup).
- **pigpio** — all hardware access, standardized on one library: SPI plus manual RS/RES pin control for the ST7735 TFT (validated by prior hands-on experience with this exact chip), I2C for the MCP23017 (buttons/LEDs) and the LCD's FC-113/PCF8574 backpack, and GPIO edge-detection callbacks for the MCP23017's interrupt line.
- **stations.csv** (name, stream URL, logo filename) — hand-parsed, no CSV library.

Dropped: libcurl, SQLite3 ([ADR 0002](0002-drop-sqlite-and-frontend.md)), libgpiod and raw i2c-dev/spidev ioctl (folded into pigpio).

**Update (2026-09-23):** wired up in Phase 0 ([cmake/Dependencies.cmake](../../cmake/Dependencies.cmake)). The header-only libraries are fetched at configure time and pinned exactly; pigpio stays a system library on the Pi, linked only with `-DWINAMPDECK_WITH_PIGPIO=ON`. GoogleTest is added as a test-only dependency. Two of the pins are forced, and were confirmed by compiling against the alternatives: websocketpp's last release (0.8.2, 2020) doesn't compile as C++20, so it's pinned to a `develop` commit that has the fix; and that commit still uses `asio::io_service`, which Asio 1.33 removed, so Asio is held at 1.30.2. Any bump of either has to move both together.
