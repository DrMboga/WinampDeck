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

## Phase 4 — The real HiFiBerry Digi+ Pro, and both engines sharing it

Wire up the actual HAT, configure the `dmix` device, and run go-librespot and mpv side by side, switching Source back and forth repeatedly and rapidly. This is where [ADR 0004](adr/0004-audio-device-sharing.md)'s open questions — which were based on reading source and docs, not a real board — actually get answered.

**Exit criteria:** switching Source many times in a row never produces a device-busy error or an audio glitch.

## Phase 5 — Buttons and LEDs (can happen in parallel with 2–4)

Wire the MCP23017, and implement the real `HardwareIO` for buttons/LEDs: pigpio for I²C register access, plus the interrupt-driven read off the MCP23017's INT line instead of polling.

**Exit criteria:** every physical button and LED does exactly what the Phase 1 tests already say it should — no new behavior is invented here, only real switches replacing fake ones.

## Phase 6 — TFT

Bring up SPI communication with the ST7735 using the developer's own proven init/addressing sequence, build the 5×7 Latin/Cyrillic bitmap font renderer, then implement the Now Playing view (skin, progress, decorative spectrum) and the Station List view (scrollable list with logos).

**Exit criteria:** the TFT reflects `PlayerController` state changes correctly and promptly, for both Sources and both Station List states.

## Phase 7 — LCD

Wire the FC-113/PCF8574 backpack, implement the scrolling first-row text ("Artist — Track" / "Station — Stream Title").

**Exit criteria:** the LCD shows the right text, scrolling at the configured speed.

## Phase 8 — Config file and stations.csv

Load the plain config file (brightness, LCD scroll speed) and `stations.csv` (name, URL, logo) at startup, and wire them into the relevant adapters/views.

**Exit criteria:** hand-editing either file and restarting the controller changes behavior exactly as expected — no other way to change either exists, by design.

## Phase 9 — Full integration on the assembled panel

Everything above, running together on the real Pi, HAT, and panel — inside the enclosure once it's built. Soak-test it: leave it running for an extended period, switching Sources, connecting different Spotify accounts, browsing stations, to surface anything that only shows up over time (event-stream reconnects, memory growth, and the like).

**Exit criteria:** the Deck runs correctly, unattended, for a multi-day soak.

## Phase 10 — Packaging

systemd unit files for the controller, go-librespot, and mpv, so the Deck comes up fully working on boot with no manual steps or SSH session required.

**Exit criteria:** cold power-on reaches a working Deck, every time.

## Deliberately not on this plan

The plywood enclosure build is physical work tracked separately from software delivery. The real spectrum analyzer (PipeWire-based), the web/OAuth stack, SQLite, and play history are out of scope per the spec — not deferred to a later phase, just not part of this plan at all.
