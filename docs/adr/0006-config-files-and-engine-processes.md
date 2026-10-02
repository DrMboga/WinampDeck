# Config files in /etc/winampdeck, and the Engines as separate processes

Status: accepted

[ADR 0002](0002-drop-sqlite-and-frontend.md) settled that the Deck's settings live in a plain config file and its Stations in a hand-edited `stations.csv`, both read once at startup. It didn't settle where they live, what the config file looks like, or what happens when one of them has a mistake in it. Building the real controller in Phase 8 needed answers, and it also needed a decision the spec had made differently: who starts mpv.

**Where the files live.** All of them go in `/etc/winampdeck/`: `config.json`, `stations.csv`, and the `logos/` directory that `stations.csv` refers to. `/etc` is where hand-edited system configuration goes, and keeping all three together means one place to edit over SSH. The controller's `--config-dir` option points elsewhere for development. A per-user directory such as `~/.config` was turned down, because the controller runs as root (pigpio needs it) under systemd, not as a login user. Splitting the config into `/etc` and the data into `/usr/local/share` was turned down because it would mean two places to edit.

**The config file.** It's JSON, parsed with nlohmann/json, which the controller already needs ([ADR 0003](0003-cpp-controller-dependencies.md)), with `//` and `/* */` comments allowed so the shipped file can explain each setting. There are two settings:
- `tft_brightness`, from 0 to 100, in percent. It's percent rather than the 0–255 PWM level because that's how a person thinks about brightness.
- `lcd_scroll_ms`, from 50 to 5000, in milliseconds per scroll step.

**Mistakes stop the controller.** Unknown keys, wrong types and out-of-range values are all errors. The controller won't start, and it says which file and setting is at fault. Ignoring an unknown key would let a typo (`tft_brightnes`) leave a setting at its default without anyone noticing. A *missing* `config.json` is not an error, though: it just means the defaults. `stations.csv` must exist.

**The Engines are separate processes.** The spec had the controller spawn mpv as a subprocess. Instead, mpv runs as its own process, as go-librespot already does, and the controller connects to its IPC socket and keeps retrying until it can. That matches [Phase 10](../delivery-plan.md#phase-10--packaging)'s plan of one systemd unit per process. It also means restarting the controller, for example after editing a config file, doesn't interrupt a stream. Both clients reconnect on their own if their Engine restarts, and they replay what they need when they do: the mpv client the Station and mute and pause state, the go-librespot client the current state from `GET /status`.

**go-librespot commands go out on a worker thread.** go-librespot's REST calls don't reply until the work is under way; a skip, for example, waits for the next track to start loading. Making those calls on the event loop would stall the displays, so they're made in order on one background thread. Events still arrive on the event loop through websocketpp, as ADR 0003 planned.
