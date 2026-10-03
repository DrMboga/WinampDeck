# Delivery Plan

This breaks the [spec](../.tracker/controller-v1/spec.md) into an order to actually build it in. The guiding idea: the architecture was deliberately shaped so the hard part — the button/Source/display decision-making — can be built and fully tested before any Raspberry Pi, Spotify account, or soldering is involved. Hardware gets bolted on afterward, one piece at a time, onto logic that's already proven correct.

Each phase lists what it delivers and how to know it's actually done (exit criteria), not just "code written."

## Phase 0 — Project scaffolding ✅

Set up the CMake project (C++20), pull in the dependencies (Asio, websocketpp, cpp-httplib, nlohmann/json, pigpio), and get a CI job running the test suite on every push.

**Exit criteria:** the project builds clean, and a trivial test runs in CI.

**Status: done (2026-09-23).** The top-level `CMakeLists.txt` splits the code into `winampdeck_core` (the hardware-free library `PlayerController` will live in) and the `winampdeck` binary. [cmake/Dependencies.cmake](../cmake/Dependencies.cmake) pins every dependency (see [ADR 0003](adr/0003-cpp-controller-dependencies.md)'s update for the two forced pins), and GoogleTest is the test framework. The test suite has a trivial version test plus one test per header-only dependency, proving they all compile together as C++20. [.github/workflows/ci.yml](../.github/workflows/ci.yml) builds and tests on every push with warnings as errors, under GCC 12 and GCC 14 (the compilers in Raspberry Pi OS Bookworm and Trixie). Verified locally under GCC 12.4: the build is warning-free and all 5 tests pass. pigpio linking (`-DWINAMPDECK_WITH_PIGPIO=ON`) is wired up but won't be exercised until the first pigpio-backed code exists, in Phase 5.

## Phase 1 — Core orchestration, no hardware at all ✅

Implement `EngineClient`, `RadioClient`, and `HardwareIO` as interfaces with simple in-memory fakes standing in for go-librespot, mpv, and pigpio. Then build `PlayerController` against those interfaces: the Source state machine, Eject's short/long-press split, Play/Pause/Next/Previous dispatch, Shuffle/Repeat behaving differently for Spotify vs. Radio, Station List navigation, and the auto-switch logic (including the device-ID check that stops it firing for Spotify activity elsewhere on the account).

This is the biggest phase in terms of decisions made real, and the one the spec's whole "Testing Decisions" section is about.

**Exit criteria:** every scenario in the spec's button map and user stories is covered by a passing test, with zero real hardware, sockets, or subprocesses involved.

**Status: done (2026-09-24).** [`PlayerController`](../src/core/player_controller.hpp) lives in `winampdeck_core` and talks to the outside only through `EngineClient`, `RadioClient`, and `HardwareIO`, plus two small seams the spec didn't name: `Scheduler` (one-shot timers, for Eject's long press, Asio-backed in production) and `SystemControl` (the Safe Shutdown poweroff). Each interface delivers its events through a `Listener` that `PlayerController` registers itself as. The in-memory fakes are in [tests/fakes.hpp](../tests/fakes.hpp); 65 scenario tests, one file per area (Eject, Source switching, Spotify, Radio, Station List, auto-switch), cover every cell of the button map. Verified warning-free under GCC 12.4. Decisions the spec left open, made here:
- **Auto-switch** keys off an `onSpotifyActiveChanged` event ("this Deck is the account's Connect target") rather than comparing device IDs in the core, matching what Phase 2 found go-librespot's `active`/`inactive` events already give us. It fires only on the *transition* into active-and-playing, so a stale `playing` event arriving just after Eject paused Spotify doesn't bounce the Source straight back.
- **Eject's long press** fires Safe Shutdown at the 2s mark while still held (not on release), after putting "Shutting down" on the LCD; every input is ignored from then on.
- **Returning to Spotify** resumes it only if it was playing when Eject switched away (and the session hasn't moved to another device since). Returning to Radio keeps the tuned Station and its pause state; the first switch to Radio tunes the first Station.
- **Stopped** (startup only) ignores every button except Eject, whose short press goes to Spotify.
- **LEDs** mirror go-librespot's reported shuffle/repeat state, not the button press. In Radio, Shuffle's LED is off and Repeat's means "Station List open."
- **Station stepping** (Previous/Next, both in and out of the Station List) wraps around the ends. Shuffle picks a random Station other than the current one.
- **LCD text** is `Artist — Track` / `Station — Stream Title`, falling back to "Spotify" / the Station name alone when there's nothing more; the actual scrolling and glyph mapping are the LCD view's job in Phase 7.

## Phases 2–3 — Wire in the real engines (can happen in parallel, and don't need the Pi yet)

**Phase 2 — go-librespot.** Run it (a dev machine or WSL is fine to start — it's a normal Linux binary), and implement the real `EngineClient` against its REST + WebSocket API. Smoke-test manually: control it from a temporary CLI standing in for the panel, confirm metadata/event flow, and confirm the auto-switch device-ID filtering actually works when you press Play on a real phone.

**Phase 3 — mpv.** Spawn it, implement the real `RadioClient` against its JSON IPC socket. This is also the first place to sanity-check the `aid no`/`aid auto` mute mechanism from [ADR 0004](adr/0004-audio-device-sharing.md), even before real hardware is involved — mpv's device-release behavior on any Linux box should already show the difference between plain `pause` and deselecting the audio track.

**Exit criteria (both):** Spotify and Radio each play/pause/skip correctly through the *same* `PlayerController` already proven in Phase 1 — only the adapter underneath changed.

**Progress (2026-09-23):** done on the target Pi 3 itself rather than a dev machine, since it was already set up for [ADR 0004](adr/0004-audio-device-sharing.md)'s hardware checks — see [pi-setup.md](pi-setup.md) for the full setup/smoke-test log.
- **Phase 2 (go-librespot): smoke-tested and confirmed.** REST control, WebSocket metadata/event flow, and the auto-switch signal (`active`/`inactive` events — simpler than the device-ID comparison originally anticipated in [spec.md](../.tracker/controller-v1/spec.md#implementation-decisions), worth revisiting there once the C++ `EngineClient` is written) all confirmed against a real phone. Along the way, found and fixed a real-audio-only bug: the shared `dmixer` ALSA device needs to be referenced as `plug:dmixer`, not bare `dmixer`, or playback comes out sped up/pitched up (rate-mismatch — recorded in [ADR 0004](adr/0004-audio-device-sharing.md)'s update log).
- **Phase 3 (mpv): smoke-tested and confirmed.** IPC control (loadfile/pause/resume) works correctly against a real internet radio stream, at the right speed (same `plug:dmixer` fix as Phase 2). The `pause`-keeps-device-open vs. `aid no`-releases-device distinction ADR 0004's mute mechanism depends on is directly confirmed on real hardware — see that ADR's update log for the exact (slightly different-than-expected, but conclusion-unchanged) `/proc/asound` states observed.
- **Not yet done, for either phase: the actual exit criteria above** — that requires the real `EngineClient`/`RadioClient` C++ code running behind `PlayerController`, and no controller code exists yet ([README](../README.md)'s Status section still says so). What's done is the manual-CLI-smoke-test half both phase descriptions call for: both Engines install cleanly, respond correctly to the same commands/IPC calls the real adapters will issue, and the two ADR-0004-relevant behaviors (auto-switch signal, `aid no`/`aid auto` device release) are confirmed on the actual board rather than just from reading source. That derisks writing `EngineClient`/`RadioClient` next — it doesn't replace it. Neither Engine is set up to survive a reboot yet (no systemd units — that's [Phase 10](#phase-10--packaging)).
- **Exit criteria met (2026-10-03), in [Phase 8](#phase-8--config-file-and-stationscsv-):** the real `GoLibrespotClient` and `MpvClient` now drive both Engines through the same `PlayerController`, checked on the Pi.

## Phase 4 — The real HiFiBerry Digi+ Pro, and both engines sharing it ✅

Wire up the actual HAT, configure the `dmix` device, and run go-librespot and mpv side by side, switching Source back and forth repeatedly and rapidly. This is where [ADR 0004](adr/0004-audio-device-sharing.md)'s open questions — which were based on reading source and docs, not a real board — actually get answered.

**Exit criteria:** switching Source many times in a row never produces a device-busy error or an audio glitch.

**Status: done (2026-09-25).** The HAT and `dmix` device were already in place from the earlier hardware checks ([pi-setup.md](pi-setup.md) steps 3–5). Both Engines played mixed at the same time with no `EBUSY`. Then a soak script switched Source 90 times in a row (at 5s, 1s and 0.3s per Source) with zero failed commands, the hardware PCM `RUNNING` after every switch, no busy/XRUN/underrun errors in either Engine's log, no undervoltage, and audibly smooth switching. Procedure and raw results: [pi-setup.md](pi-setup.md#phase-4--both-engines-sharing-the-hat); conclusions: [ADR 0004](adr/0004-audio-device-sharing.md)'s 2026-09-25 update. Like Phases 2–3, this was done by driving the Engines directly, standing in for the controller; no C++ adapter code is involved yet.

Findings the future `RadioClient` must handle (details in ADR 0004):
- `aid no` carries over into the next `loadfile`, so loading a Station while muted silently fails.
- A muted stream isn't read, so after a mute of several minutes the station's server drops the connection and `aid auto` brings back silence. Switching to Radio needs to re-`loadfile` the Station. **Open follow-up:** measure how long a mute a stream actually survives (a 30+ minute mute test was cut short).

Separately, the Pi's Wi-Fi dropped mid-session (`wpa_supplicant: Failed to initiate sched scan`, then no reconnect) and the rest of Phase 4 ran over Ethernet with Wi-Fi turned off. It's unrelated to audio, but it's a risk if the finished Deck runs on Wi-Fi. The HAT sits over the Pi 3's antenna, much like the Zero 2 W problem in [ADR 0005](adr/0005-target-board-pi-3b.md). Worth investigating (Wi-Fi power-save off, signal strength with the HAT fitted) before Phase 9.

## Phase 5 — Buttons and LEDs (can happen in parallel with 2–4) ✅

Wire the MCP23017, and implement the real `HardwareIO` for buttons/LEDs: pigpio for I²C register access, plus the interrupt-driven read off the MCP23017's INT line instead of polling.

**Exit criteria:** every physical button and LED does exactly what the Phase 1 tests already say it should — no new behavior is invented here, only real switches replacing fake ones.

**Status: done (2026-09-30).** Run on the real panel (Raspberry Pi OS Trixie, pigpio built from source). All 8 buttons and both LEDs work. Through `PlayerController`, every button-map behaviour checked on the panel matched the Phase 1 tests. Buttons caused no audio glitches with Spotify playing through the HAT. Results: [pi-setup.md](pi-setup.md#88-results-2026-09-30). How it's built: [`PigpioHardwareIO`](../src/hw/pigpio_hardware_io.hpp) (new `winampdeck_hw` library, built only with `-DWINAMPDECK_WITH_PIGPIO=ON`) configures the MCP23017 per [wiring.md](wiring.md#mcp23017-wiring) and takes button changes off the GPIO27 interrupt line. pigpio's alert thread only posts to the Asio loop, where the port is read and debounced (20ms of quiet). The TFT/LCD calls just print for now (Phases 6–7). [`AsioScheduler`](../src/adapters/asio_scheduler.hpp) is the production `Scheduler`. The pin map and edge detection are hardware-free and unit-tested. `PigpioSession` switches pigpio's DMA pacing from the PCM peripheral (the HAT's I2S block) to PWM, so buttons can't interfere with audio. Phase 6 must keep this in mind for the TFT backlight on GPIO12/PWM0. The bring-up tool `winampdeck-panel-test` has a buttons-only wiring mode and a `--controller` mode running the real `PlayerController` with console stand-ins for the Engines. That second mode is how the exit criterion gets checked. Procedure and checklists: [pi-setup.md](pi-setup.md#phase-5--buttons-and-leds). The Pi 3 has to build with `-j1`: two parallel compiles run it out of memory.

## Phase 6 — TFT ✅

Bring up SPI communication with the ST7735 using the developer's own proven init/addressing sequence, build the 5×7 Latin/Cyrillic bitmap font renderer, then implement the Now Playing view (skin, progress, decorative spectrum) and the Station List view (scrollable list with logos).

**Exit criteria:** the TFT reflects `PlayerController` state changes correctly and promptly, for both Sources and both Station List states.

**Status: done (2026-10-01).** Checked on the Pi with the TFT on a breadboard: the test pattern was right first time, so no offset or colour-order fix was needed. In controller mode, the TFT followed every Source change and every Station List open, move and close made with the real buttons. Spotify played without dropouts while the screen animated. Results: [pi-setup.md](pi-setup.md#96-results). Afterwards the default rotation was turned 180° for how the module will sit in the panel, and checked on the Pi. The backlight stays driven straight from GPIO12's PWM: tying it to 3.3V was brighter, but the PWM level was preferred. What was built:
- **Rendering is hardware-free.** A new `winampdeck_ui` library draws each frame in memory and hands it to a `ui::Display`, the TFT's seam, in the same way `HardwareIO` is the panel's. It holds the [5×7 font](../src/ui/font5x7.cpp) adopted from the SABA Radio project (ASCII, German, Russian; accented Latin letters fall back to their plain letter, anything else to `?`), a small canvas, and [`TftView`](../src/ui/tft_view.hpp). `TftView` draws both screens and sends only the parts of each frame that changed. Like the core, it's tested in CI against fakes: a `ManualScheduler` and an in-memory display.
- **Now Playing** shows the cover or logo at 92×92, a play/pause/stop indicator, the clock in big digits, a decorative 15-bar spectrum, Spotify's progress bar with a yellow knob like the panel's, and two text lines (Artist/Title, Station/Stream Title). Lines that are too long scroll, Winamp-style, with `***` between repeats. The clock keeps running between go-librespot's position reports and blinks while paused. For Radio it counts from when the Station was tuned. Animation runs at 20fps, and only while something on screen is moving.
- **Station List** shows five rows with 21×21 logo thumbnails and a scrollbar. The highlight uses Winamp's playlist blue and sits in the middle row where possible. The tuned Station is in white, and a long highlighted name scrolls.
- **Landscape, 160×128.** The panel's TFT cutout is wider than it is tall. The ST7735's rotation and colour order (MADCTL) are a start-up setting until checked on the real panel.
- **The [`St7735`](../src/hw/st7735.hpp) driver** uses pigpio SPI0/CE0 plus manual RS/RES, with a standard init sequence: hardware reset, SLPOUT, COLMOD 16-bit, MADCTL, INVOFF, NORON, clear, DISPON. It writes only the requested window (CASET/RASET/RAMWR). Phase 5's caveat holds: pigpio's DMA is paced by the PWM peripheral, so hardware PWM isn't available. The backlight on GPIO12 uses pigpio's DMA-timed PWM at 800Hz instead.
- **Station logos** are the SABA Radio project's 92×92 raw little-endian RGB565 `.565` files, now in [data/logos/](../data/logos/). Its station list became [data/stations.csv](../data/stations.csv): 60 Stations in `name,url,logo` form, with the button/frequency columns dropped and duplicates removed. The `stations.csv` parser was pulled forward from Phase 8, because the Station List needs real Stations to be checked.
- **Spotify covers are converted at runtime.** `SpotifyTrack` and `NowPlayingScreen` gained a `coverUrl`, for the real `EngineClient` to fill from go-librespot's `album_cover_url`. `DeckArtwork` downloads covers on a background thread, decodes the JPEG with stb_image, centre-crops it, scales it to 92×92 by area averaging, and keeps the last 8. See [ADR 0003](adr/0003-cpp-controller-dependencies.md)'s 2026-10-01 update for why this is plain HTTP over IPv4 and adds stb_image.
- **Tools.** `winampdeck-tft-preview` builds anywhere and renders the screens to `.bmp` files, including a real downloaded cover, to check the look without a Pi. `winampdeck-panel-test` gained `--tft`, which shows a test pattern and then the real screens. Its `--controller` mode now drives the real TFT with the shipped Stations. Its Spotify stand-in now connects on the first Play and plays three demo tracks with real covers, which also fixes Phase 5's "Spotify shows `stopped`" artifact.

## Phase 7 — LCD ✅

Wire the FC-113/PCF8574 backpack, implement the scrolling first-row text ("Artist — Track" / "Station — Stream Title").

**Exit criteria:** the LCD shows the right text, scrolling at the configured speed.

**Status: done (2026-10-02).** Checked on the Pi with the LCD's backpack behind the TXS0108E. The level shifter left the shared I2C bus working, the LCD showed every sample text correctly, and in controller mode it followed `PlayerController` alongside the TFT and buttons. Spotify played smoothly with both displays running. Results: [pi-setup.md](pi-setup.md#106-results). Still to do: dimming the backlight with a resistor. What was built:
- **Hardware-free, like the TFT.** [`LcdView`](../src/ui/lcd_view.hpp) in `winampdeck_ui` hands 16-character rows to `ui::LcdDisplay`, the LCD's seam, and is tested in CI against a `ManualScheduler` and an in-memory LCD. Text up to 16 characters is shown still. Longer text pauses 1.5s, then scrolls one character per step with `  ***  ` between repeats, like the TFT's marquee. The step (default 300ms) is a constructor parameter, for Phase 8's config. Only rows that changed are written.
- **[`encodeLcdText`](../src/ui/lcd_text.hpp)** maps UTF-8 to the HD44780's A00 ROM, which the module turned out to have:
  - ASCII as itself, except `\` and `~`, which A00 shows as ¥ and →.
  - `ä ö ü ß ñ ° µ` as the ROM's own characters, and `Ä Ö Ü` as `Ae Oe Ue`.
  - Cyrillic transliterated. Custom CGRAM glyphs were turned down: 8 aren't enough for lowercase Russian.
  - Everything else as the TFT font's plain substitute, else `?`.
- **The [`Hd44780`](../src/hw/hd44780.hpp) driver** uses pigpio I2C to the PCF8574 at `0x27` (P0=RS, P2=E, P3=backlight, P4–P7=D4–D7), with the datasheet's initialisation by instruction into 4-bit mode. It writes a whole row as one I2C transfer, and clears the LCD and turns its backlight off on exit.
- **Tools.** `winampdeck-panel-test` gained `--lcd`, which shows a ROM check and then sample texts, and `--lcd-scroll-ms`. Its `--controller` mode now drives the real LCD.

## Phase 8 — Config file and stations.csv ✅

Load the plain config file (brightness, LCD scroll speed) and `stations.csv` (name, URL, logo) at startup, and wire them into the relevant adapters/views.

The `stations.csv` parser and the shipped list ([data/](../data/)) already exist from Phase 6. What's left for it here is reading it from its installed location in the controller binary. Brightness goes to `St7735::Config::brightness`.

**Exit criteria:** hand-editing either file and restarting the controller changes behavior exactly as expected — no other way to change either exists, by design.

**Scope, decided 2026-10-02:** Phase 8 also writes the real `EngineClient` and `RadioClient` and a real `winampdeck` binary. The exit criterion needs a controller to restart, and `main.cpp` was still a placeholder. So this phase also closes Phases 2–3's open exit criteria: Spotify and Radio controlled through `PlayerController` by the real adapters.

**Progress (2026-10-02): software written, not yet run on the Pi.** CI passes (GCC 12 and 14, 182 tests), and the Pi-only files compile against pigpio's header. Decisions: [ADR 0006](adr/0006-config-files-and-engine-processes.md). Checklist: [pi-setup.md](pi-setup.md#phase-8--config-files-and-the-real-controller).
- **[`DeckConfig`](../src/core/deck_config.hpp)** reads `/etc/winampdeck/config.json`: `tft_brightness` (0–100, percent) and `lcd_scroll_ms` (50–5000). Comments are allowed. Unknown keys, wrong types and out-of-range values stop the controller with a message naming the setting. A missing file gives the defaults. The shipped file is [data/config.json](../data/config.json).
- **[`GoLibrespotClient`](../src/adapters/go_librespot_client.hpp)** takes events from `/events` (mapped in [`librespot_events`](../src/adapters/librespot_events.hpp), tested against payloads taken from go-librespot's source) and fetches `GET /status` on every connect. REST commands go out in order on a worker thread. It reconnects while go-librespot is away.
- **[`MpvClient`](../src/adapters/mpv_client.hpp)** applies Phase 4's findings: it mutes with `aid no`, reloads the Station on every unmute, defers a load while muted, and selects the track before `loadfile`. The stream title is mpv's ICY title. It reconnects and replays its state. It's tested against a fake mpv on a Unix socket.
- **`SystemPoweroff`** carries out Safe Shutdown with `systemctl poweroff`.
- **`winampdeck`** wires it all to the real panel. It's built only with pigpio.

**The Pi can't build it comfortably.** Compiling the first version of `go_librespot_client.cpp` ran the Pi 3 out of memory (`Killed signal terminated program cc1plus`): cpp-httplib and websocketpp in one file peaked at 1.28 GB with `-O2 -g`. It was split into [`LibrespotRest`](../src/adapters/librespot_rest.hpp) and [`LibrespotEventStream`](../src/adapters/librespot_event_stream.hpp), each hiding its library behind its header, and now no file peaks above 760 MB. The test file `dependencies_test.cpp` still needs about 1.28 GB.

**Building on the PC and deploying to the Pi (done 2026-10-03).** [tools/deploy-pi.sh](../tools/deploy-pi.sh) does all of the below, using the image from [tools/cross/Dockerfile](../tools/cross/Dockerfile) and [cmake/toolchain-pi-arm64.cmake](../cmake/toolchain-pi-arm64.cmake); how to use it is in [pi-setup.md](pi-setup.md#111-build-on-the-pc-and-deploy). The deployed `winampdeck` runs on the Pi and loads the Pi's own pigpio. It replaces `git pull` and `cmake --build` on the Pi, as planned:
1. **Cross-build in Docker.** A Debian Trixie container with `crossbuild-essential-arm64` matches the Pi: Raspberry Pi OS Trixie, aarch64, GCC 14.2, glibc 2.41, as checked on 2026-10-02. pigpio is cross-built from the commit the Pi has (`c33738a`), only to link against; at runtime the binary uses the Pi's own `/usr/local/lib/libpigpio.so.1`. The build uses the same `-DWINAMPDECK_WITH_PIGPIO=ON` configuration as on the Pi.
2. **Tests stay native.** The hardware-free suite runs in an amd64 container on the PC, as CI does. The Pi only runs the controller and the panel test.
3. **A deploy script** builds, prints the commit it built, and copies `winampdeck`, `winampdeck-panel-test` and `data/` to `~/winampdeck-deploy/` on the Pi with `scp`, as `pi@WinampDeck` with key authentication. `sudo` on the Pi asks for a password, so installing into `/etc/winampdeck` stays a manual `sudo` step.

**Checked on the Pi (2026-10-03).** The real controller ran the panel and both Engines: auto-switch from a phone, every button for both Sources, the Station List, a mute of more than 10 minutes followed by Radio playing again, and an Engine restart. Editing `config.json` and `stations.csv` and restarting changed the brightness, the scroll speed and the Station order, and mistakes in `config.json` stopped the controller with a clear message. That meets this phase's exit criterion and the ones Phases 2–3 left open. One bug was found and fixed: the Shuffle and Repeat buttons did nothing when a session started with the mode already on, because go-librespot sends no event for that; the client now asks for the modes when a session or track arrives. Results: [pi-setup.md](pi-setup.md#117-results).

**Status: done (2026-10-03).** Safe Shutdown was the last check: holding Eject powered the Pi off.

## Phase 9 — Full integration on the assembled panel

Everything above, running together on the real Pi, HAT, and panel — inside the enclosure once it's built. Soak-test it: leave it running for an extended period, switching Sources, connecting different Spotify accounts, browsing stations, to surface anything that only shows up over time (event-stream reconnects, memory growth, and the like).

**Exit criteria:** the Deck runs correctly, unattended, for a multi-day soak.

**Order (decided 2026-10-03):** this phase now comes last, after [Phase 11](#phase-11--last-station) and [Phase 12](#phase-12--real-spectrum-analyzer). Phase 12 changes the audio setup, and the soak only means something on the setup the finished Deck uses. The soak also picks up the checks Phase 10 left out: a killed Engine or controller, a config typo under systemd, and a power cut without a Safe Shutdown.

## Phase 10 — Packaging ✅

systemd unit files for the controller, go-librespot, and mpv, so the Deck comes up fully working on boot with no manual steps or SSH session required.

**Exit criteria:** cold power-on reaches a working Deck, every time.

**Status: done (2026-10-03), ahead of Phase 9**, so that the soak test runs the Deck the way it will really run: started by systemd at boot. After a reboot and after a cold power-on, the Deck came up working by itself. Results: [pi-setup.md](pi-setup.md#126-results). Not yet run: the failure checks under systemd (a killed Engine or controller, a config typo) and a power cut without a Safe Shutdown. They fit naturally into Phase 9's soak. What was built:
- **Three units**, in [systemd/](../systemd/):
  - `winampdeck-librespot` and `winampdeck-mpv` run the Engines as `pi`, the same user for both because the `dmixer` device is only shared within one user ([ADR 0004](adr/0004-audio-device-sharing.md)). They restart whenever they exit.
  - `winampdeck` runs the controller as root, which pigpio needs. It's ordered after the Engines without requiring them: it reconnects by itself, and at shutdown it stops first and switches the panel off. It restarts on failure, but gives up after 5 failures in a minute, so a mistake in `config.json` doesn't loop forever.
- **mpv's socket** is `/run/winampdeck-mpv/socket`, in a runtime directory systemd creates and removes with the unit. go-librespot stays in `/home/pi/go-librespot`, where Phase 2 installed it.
- **[tools/pi-install.sh](../tools/pi-install.sh)**, deployed as `install.sh`, installs the binaries into `/usr/local/bin` and the units into `/etc/systemd/system`, enables them, copies the config files only where they're missing, and restarts the controller. It needs `sudo`, so it's run by hand on the Pi after each `tools/deploy-pi.sh`.

## Phase 11 — Last Station

Added 2026-10-03, after the Deck had been used for real. Spec: [.tracker/last-station/spec.md](../.tracker/last-station/spec.md). Decisions: [ADR 0007](adr/0007-last-station-state-file.md).

After a power-on, the first switch to Internet Radio tunes the Last Station, the one that was tuned at the last Safe Shutdown, instead of the first Station in the list. The Deck still starts in Stopped.

- The Last Station is remembered as a position in `stations.csv`, in `/var/lib/winampdeck/state.json`, which the controller writes. A position past the end of the list, or a missing or damaged file, means the first Station.
- It's saved only at Safe Shutdown. After a power cut or an unplug, the Deck comes back with the Station from the last Safe Shutdown.
- The rules live in `PlayerController`, behind a new `StationMemory` interface, and are tested with a fake like the rest of the core.

**Exit criteria:** tune a Station, Safe Shutdown, power-cycle, and the first switch to Internet Radio plays that Station; with the rules covered by tests in CI.

## Phase 12 — Real spectrum analyzer

Added 2026-10-03. Spec: [.tracker/spectrum-analyzer/spec.md](../.tracker/spectrum-analyzer/spec.md). Research: [.tracker/spectrum-analyzer/research/audio-tap.md](../.tracker/spectrum-analyzer/research/audio-tap.md).

The TFT's 15 bars show the sound that's actually playing, in place of today's decorative animation. The controller has to hear what the Engines play, and ALSA's shared mixer has no built-in way to allow that, so this phase starts with an experiment and is only built if the experiment passes.

**Step 1: the experiment, on the Pi, with no controller code.** Load the kernel's loopback sound card and have each Engine play to two mixers at once: the existing one on the HAT, and a second one on the loopback, which the controller will later record from. The risk is that the two devices' clocks drift apart; the loopback is to be set to take its timing from the HAT. It passes only if all three hold:
1. The [Phase 4](#phase-4--the-real-hifiberry-digi-pro-and-both-engines-sharing-it-) soak, unchanged: 90 Source switches with no failed command, no device-busy error and no audible glitch.
2. Two hours of continuous play on each Engine, with no dropout by ear and no underrun or xrun in the logs.
3. A recording from the loopback while music plays sounds like that music.

If it fails and can't be fixed by adjusting the configuration, restore the previous ALSA configuration, record the result, and try PipeWire in place of the ALSA mixer as a separate experiment with the same three checks. If both fail, the decorative spectrum stays as it is.

**Step 2: the analyzer, if step 1 passed.**
- Inside the controller, behind a new `SpectrumSource` interface. Recording and the Fourier transform run on a background thread, and the TFT view only draws the levels it's given.
- Real bars when the tap is available; flat bars and one log line when it isn't. The decorative animation is removed.
- Starting values, to tune on the real screen: 15 bars spaced evenly in pitch from about 60 Hz to 16 kHz, left and right mixed, decibel height, the existing rise, fall and peak markers, 20 updates a second.
- New dependencies: KISS FFT, and ALSA's client library on the Pi and in the cross-build image.
- First build step: measure the cost on the Pi 3. Check on the real Deck whether the bars need a delay to keep time with the sound.

**Exit criteria:** the bars follow the music for both Sources and fall to nothing on pause and mute; the audio is as clean as before (the three checks above still hold with the analyzer running); and with the loopback unavailable the bars stay flat while everything else works.

## Deliberately not on this plan

The plywood enclosure build is physical work tracked separately from software delivery. The web/OAuth stack, SQLite, and play history are out of scope per the spec — not deferred to a later phase, just not part of this plan at all. The real spectrum analyzer was on this list until 2026-10-03; it's now [Phase 12](#phase-12--real-spectrum-analyzer).
