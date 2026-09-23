# Raspberry Pi 3 Setup — Phases 2–3

This is a from-scratch setup guide for the Raspberry Pi 3 Model B ([ADR 0005](adr/0005-target-board-pi-3b.md)) running the Digi Pro HAT, taking it from a blank SD card through [Phase 2 and Phase 3](delivery-plan.md#phases-23--wire-in-the-real-engines-can-happen-in-parallel-and-dont-need-the-pi-yet) of the delivery plan: getting the real go-librespot and mpv Engines installed and smoke-tested on the actual board. It assumes default Raspberry Pi OS (64-bit), no monitor/keyboard — everything over SSH.

Steps marked **✅ Already done** are recorded here for completeness (so this doc works as a full rebuild guide too) — skip them on the current board and jump to [Phase 2: go-librespot](#phase-2--go-librespot). Steps marked **☐ To do** are the actual work ahead of you right now.

Read [CONTEXT.md](../CONTEXT.md) first if any term here (Source, Engine, Eject, Station) is unfamiliar — this doc uses that vocabulary throughout.

## 1. Flash the OS — ✅ already done, recorded for rebuilds

1. Download [Raspberry Pi Imager](https://www.raspberrypi.com/software/) on your PC.
2. Pick **Raspberry Pi OS Lite (64-bit)** — no desktop needed, this is a headless appliance. Pick **Raspberry Pi 3** as the device.
3. Before writing, click the gear icon (or press Ctrl+Shift+X) for the advanced options and set, in one pass:
   - Hostname (e.g. `winampdeck`)
   - Enable SSH, "Use password authentication" (or paste a public key if you prefer key-based auth)
   - Username/password
   - Wi-Fi SSID/password + your country code, *if* you're not using Ethernet
   - Locale/timezone/keyboard layout
4. Write the image, insert the SD card into the Pi, connect the Digi Pro HAT to the 40-pin header (see [wiring.md](wiring.md) if this is a fresh board), and power on with the 5V/2.5A micro-USB supply ([ADR 0005](adr/0005-target-board-pi-3b.md) — the Pi 3 powers over micro-USB, unlike the Pi 4's USB-C).
5. From your PC:
   ```bash
   ssh <username>@<hostname>.local
   # or, if mDNS resolution doesn't work on your network, find the IP from your router and:
   ssh <username>@<ip-address>
   ```

## 2. Base system update — ✅ already done, recorded for rebuilds

```bash
sudo apt update && sudo apt full-upgrade -y
sudo reboot
```

Reconnect via SSH after the reboot. Useful tools for everything below:

```bash
sudo apt install -y git curl socat alsa-utils i2c-tools
```

(`socat` talks to mpv's JSON IPC socket in Phase 3; `alsa-utils` gives you `aplay`/`speaker-test`; `i2c-tools` gives you `i2cdetect`.)

## 3. Enable the Digi Pro HAT's audio — ✅ already done

The Digi Pro HAT (a HiFiBerry Digi+ Pro clone) needs its own overlay, not the Pi's onboard audio. In `/boot/firmware/config.txt`:

```ini
dtparam=audio=off
dtoverlay=hifiberry-digi-pro
```

Reboot, then verify:

```bash
aplay -l
# should list: card 0: sndrpihifiberry ...

speaker-test -D hw:0,0 -c 2 -t sine
# should produce audible sound on both channels; Ctrl+C to stop
```

Confirmed working on the real board 2026-09-22 — see [ADR 0004](adr/0004-audio-device-sharing.md).

## 4. Enable I2C — ✅ already done

Needed for the MCP23017 (buttons/LEDs) and the LCD's I2C backpack (Phases 5/7 — not needed yet for Phases 2–3, but already on from the hardware bring-up). In `/boot/firmware/config.txt`:

```ini
dtparam=i2c_arm=on
```

And make sure the kernel module is loaded (wasn't automatic on this image):

```bash
echo i2c-dev | sudo tee -a /etc/modules
sudo reboot
```

Verify:

```bash
i2cdetect -y 1
# should show 0x3B as UU (WM8804, already owned by its kernel driver — expected)
```

See [wiring.md](wiring.md#the-shared-i2c1-bus) for the full bus layout.

## 5. Set up the shared `dmix` audio device — ✅ already done

Both Engines (go-librespot and mpv) need to share the one physical output without an `EBUSY` collision when both are running ([ADR 0004](adr/0004-audio-device-sharing.md)). `/etc/asound.conf`:

```
pcm.hifiberry {
    type hw
    card sndrpihifiberry
}

pcm.dmixer {
    type dmix
    ipc_key 1024
    ipc_key_add_uid false
    slave {
        pcm "hifiberry"
        channels 2
        rate 48000
    }
}

ctl.dmixer {
    type hw
    card sndrpihifiberry
}
```

`pcm.!default` is deliberately left alone — both Engines below are pointed at `dmixer` explicitly by name, not at the system default. Already confirmed on this board: two concurrent `speaker-test -D dmixer` processes (440Hz/880Hz) played simultaneously with no `EBUSY`.

---

## Phase 2 — go-librespot

**Goal** (per the [delivery plan](delivery-plan.md)): get go-librespot running for real, control it like the panel eventually will, confirm metadata/event flow, and confirm the auto-switch device-ID filtering actually works from a real phone.

### 5.1 Install go-librespot — ✅ done 2026-09-23

Pick **one** of the two options below, not both — the prebuilt binary is simpler and sufficient for this smoke test.

**Option A (preferred): prebuilt binary.** Check the [go-librespot releases page](https://github.com/devgianlu/go-librespot/releases) for a prebuilt `linux-arm64` (or `arm64`) binary — this is a single static Go binary, no runtime dependencies:

```bash
mkdir -p ~/go-librespot && cd ~/go-librespot
curl -LO https://github.com/devgianlu/go-librespot/releases/latest/download/go-librespot_linux_arm64.tar.gz
tar xzf go-librespot_linux_arm64.tar.gz
chmod +x go-librespot
```

(Confirm the exact release asset filename on the actual releases page — naming may have changed since this was written. Verify what you got with `file go-librespot` — it should report an ELF aarch64 executable, not a directory.)

If this worked, skip Option B entirely and go straight to [5.2 Configure it](#52-configure-it--to-do).

**Option B (fallback): build from source** — only if no prebuilt arm64 binary is published:

```bash
sudo apt install -y golang-go
git clone https://github.com/devgianlu/go-librespot.git
cd go-librespot
go build -o go-librespot ./cmd/daemon   # check the repo's own README for the exact build target — this may have moved
```

### 5.2 Configure it — ✅ done 2026-09-23

Create `~/go-librespot/config.yml`:

```yaml
device_name: WinampDeck
device_type: speaker

audio_backend: alsa
audio_device: plug:dmixer

zeroconf_enabled: true

server:
  enabled: true
  address: localhost
  port: 3678

mpris_enabled: false
```

- `audio_device: plug:dmixer` points it at the shared `dmix` device from step 5 — through ALSA's `plug` layer, not the bare `dmixer` name. The `dmixer` device in `/etc/asound.conf` fixes its rate at 48000, but Spotify's stream is natively 44.1kHz; opening it bare produced audibly sped-up/pitched-up playback on real hardware (confirmed 2026-09-23 — see [ADR 0004](adr/0004-audio-device-sharing.md)). `plug:` wraps any named ALSA PCM with automatic sample-rate/format conversion, so the client's real rate gets resampled to match the mixer transparently. Use `plug:dmixer` for mpv too (Phase 3) for the same reason — internet radio streams arrive at all kinds of rates.
- `zeroconf_enabled: true` is what lets any phone's Spotify app pick "WinampDeck" and hand over credentials directly — no OAuth flow to build ([ADR 0001](adr/0001-use-go-librespot-for-spotify-connect.md)).
- `server.address: localhost` keeps the REST/WebSocket API off the LAN — go-librespot's control API has no documented authentication, so it must never be reachable from outside the Pi itself.

Check go-librespot's own `config.yml.example`/README on whatever version you installed — field names may have shifted since this was researched.

### 5.3 Run it — ✅ done 2026-09-23

Use `tmux` (or `screen`) so it survives your SSH session disconnecting — a real systemd unit is Phase 10's job, not now:

go-librespot only reads `config.yml` from a **config directory** (default `~/.config/go-librespot`), set with the `--config_dir` flag — it does *not* accept a config file path as a plain positional argument (that gets silently ignored, leaving it running on defaults with the API server disabled). This CLI uses `pflag`, which requires a **double dash** for long flag names (single-dash is reserved for one-letter shorthands, e.g. `-c` for `--conf`) — `-config_dir` gets misparsed as `-c` plus a mangled value, not as this flag. Point `--config_dir` at the folder you just created `config.yml` in:

```bash
sudo apt install -y tmux
tmux new -s librespot
cd ~/go-librespot
./go-librespot --config_dir ~/go-librespot
# Ctrl+B then D to detach; `tmux attach -t librespot` to come back
```

### 5.4 Smoke-test control and metadata — ✅ done 2026-09-23

From your phone: open the Spotify app, tap the Connect/devices icon, and pick **WinampDeck**. Start playing something.

From the Pi (or another machine on the LAN if you SSH-tunnel, but simplest is directly on the Pi):

```bash
# Control — standing in for what the panel's buttons will eventually call:
curl -X POST http://localhost:3678/player/pause
curl -X POST http://localhost:3678/player/resume
curl -X POST http://localhost:3678/player/next
curl -X POST http://localhost:3678/player/prev

# Current state/metadata:
curl -s http://localhost:3678/status | python3 -m json.tool
```

You should hear the pause/resume/skip happen through the amp, and the phone's own Spotify UI should reflect it — confirming local control actually reaches the same Spirc/Connect session state the phone sees (this is the whole reason go-librespot was picked over mainline librespot — see [ADR 0001](adr/0001-use-go-librespot-for-spotify-connect.md)).

For the `/events` WebSocket stream (track changes, play/pause, shuffle/repeat), install a small WebSocket CLI client — `websocat` is the simplest:

```bash
# not packaged in apt on Raspberry Pi OS — grab the prebuilt binary instead
# (aarch64-unknown-linux-musl matches Raspberry Pi OS 64-bit)
curl -L -o ~/websocat https://github.com/vi/websocat/releases/latest/download/websocat.aarch64-unknown-linux-musl
chmod +x ~/websocat
~/websocat ws://localhost:3678/events
```

Change tracks, toggle shuffle/repeat from the phone, and confirm events stream through live.

### 5.5 Confirm the auto-switch signal — ✅ confirmed 2026-09-23

This is the trickiest part of the auto-switch logic (spec user story 11): the Deck must only treat Spotify activity as "switch me to Spotify" when it's playing *on the Deck itself*, not when you press Play on your phone's own speaker.

The spec's Implementation Decisions section anticipated needing to match a reported device ID against the Deck's own — but real hardware shows go-librespot already scopes this for you, more simply: with `websocat ws://localhost:3678/events` running,

1. Switching Spotify playback *away* from WinampDeck (picking the phone's own speaker in the Connect picker and pressing Play there) fires `{"type":"inactive","data":null}` followed by `{"type":"stopped",...}` on WinampDeck's own event stream — nothing here looks like "this Deck is playing."
2. Switching *back* to WinampDeck fires `{"type":"active","data":null}`, followed by the normal `will_play`/`metadata`/`playing` sequence resuming.

So the real `EngineClient` doesn't need to compare device IDs at all — it just needs to track the `active`/`inactive` events, which are already scoped to this Deck's own go-librespot instance. Worth revisiting that line in [spec.md](../.tracker/controller-v1/spec.md#implementation-decisions) once the C++ implementation gets here, since it's simpler than what was originally anticipated.

**Phase 2 exit criteria met when:** you've controlled go-librespot via REST like the panel eventually will, watched metadata/events flow over the WebSocket, and confirmed the `active`/`inactive` distinction above. **All three confirmed on real hardware 2026-09-23.**

---

## Phase 3 — mpv

**Goal**: spawn mpv, drive it over its JSON IPC socket the way the real `RadioClient` will, and sanity-check the `aid no`/`aid auto` mute mechanism from [ADR 0004](adr/0004-audio-device-sharing.md) — confirming plain `pause` leaves the audio device held open while deselecting the audio track actually releases it.

### 6.1 Install mpv — ✅ done 2026-09-23

```bash
sudo apt install -y mpv
```

### 6.2 Spawn it with its IPC socket enabled — ✅ done 2026-09-23

Check for a stale session before creating one — reusing a name that's already taken silently runs the following commands in your plain shell instead of inside tmux, which is exactly what happened with go-librespot in Phase 2 (you won't be able to detach, and `tmux new` will print `duplicate session: mpv`):

```bash
tmux ls
```

If `mpv` is already listed, clear it first: `tmux kill-session -t mpv`. Then start clean, and don't move to the next command until you've confirmed no "duplicate session" error:

```bash
tmux new -s mpv
```

Only once you're actually inside the new session:

```bash
mpv --idle --no-video --input-ipc-server=/tmp/mpv-socket --audio-device=alsa/plug:dmixer
# Ctrl+B then D to detach
```

`--idle` keeps it running with nothing loaded (mirroring how the real controller will spawn it once, for the Deck's whole uptime); `--audio-device=alsa/plug:dmixer` points it at the same shared `dmix` device go-librespot uses — through `plug`, not the bare `dmixer` name, so mismatched-rate radio streams get resampled correctly instead of playing back sped up (confirmed necessary with go-librespot in Phase 2 — see [ADR 0004](adr/0004-audio-device-sharing.md)).

### 6.3 Drive it over the socket — ✅ done 2026-09-23

From another SSH session:

```bash
# Tune a station (any real internet radio stream URL works, e.g. this one):
echo '{"command": ["loadfile", "https://streams.radiobob.de/ozzyosbourne/mp3-192"]}' | socat - /tmp/mpv-socket

# Check it's actually playing:
echo '{"command": ["get_property", "pause"]}' | socat - /tmp/mpv-socket

# Previous/Next in the spec map to loading a different station URL — nothing built-in here,
# this is purely "does loadfile/pause/stop work at all."
echo '{"command": ["set_property", "pause", true]}' | socat - /tmp/mpv-socket
echo '{"command": ["set_property", "pause", false]}' | socat - /tmp/mpv-socket
```

You should hear the stream start, pause, and resume through the amp.

### 6.4 Confirm the `aid no`/`aid auto` device-release behavior — ✅ confirmed 2026-09-23

This is the actual point of Phase 3 per the delivery plan — confirming the difference ADR 0004's research found between plain `pause` (keeps the ALSA device open) and deselecting the audio track (actually releases it):

```bash
# 1. Play something, then check the PCM's ALSA state while running:
cat /proc/asound/card0/pcm0p/sub0/status | grep state
# expect: state: RUNNING

# 2. Plain pause:
echo '{"command": ["set_property", "pause", true]}' | socat - /tmp/mpv-socket
cat /proc/asound/card0/pcm0p/sub0/status | grep state

# 3. Un-pause, then deselect the audio track instead — this should fully release the device:
echo '{"command": ["set_property", "pause", false]}' | socat - /tmp/mpv-socket
echo '{"command": ["set_property", "aid", "no"]}' | socat - /tmp/mpv-socket
cat /proc/asound/card0/pcm0p/sub0/status
# expect: no output, or an error reading the file — the PCM substream is gone, i.e. actually released

# 4. Reselect the audio track to resume:
echo '{"command": ["set_property", "aid", "auto"]}' | socat - /tmp/mpv-socket
cat /proc/asound/card0/pcm0p/sub0/status | grep state
# expect: state: RUNNING again
```

**Real-hardware result (2026-09-23), through the `plug:dmixer` device from Phase 3's setup:** step 2 showed `state: RUNNING`, not `PAUSED` — `dmix`/`plug` virtual devices don't support ALSA's hardware-pause capability the way a raw `hw:` device does, so mpv's ALSA driver never calls `snd_pcm_pause()` here; it just stops feeding new audio while leaving the substream open. That's a variant on what was originally expected (a raw-hw-device test would likely show `PAUSED`), but the distinction that actually matters is unaffected and fully confirmed: plain `pause` never releases the substream (`RUNNING` = still held open) while `aid no` closes it outright (`closed`) and `aid auto` reopens it. That confirms the [ADR 0004](adr/0004-audio-device-sharing.md) decision — the controller must always use `aid no`/`aid auto` to mute Radio, never plain `pause` alone, on this dmix-based setup.

### 6.5 Optional: run both Engines together as a first look at Phase 4

Not required for Phase 3's exit criteria, but if go-librespot from Phase 2 is still running, this is a cheap early look at what Phase 4 will validate properly later: start Spotify playing, then start mpv playing a station too (both on `dmixer`), and confirm no `EBUSY` and no audio glitch. Then use go-librespot's `/player/pause` and mpv's `aid no`/`aid auto` to switch which one is actually audible a few times in a row.

**Phase 3 exit criteria met when:** mpv responds correctly to loadfile/pause/resume over IPC, and you've directly observed the `pause`-keeps-device-open vs. `aid no`-releases-device distinction on this board. **Confirmed 2026-09-23.**

---

## What's next

Phases 2 and 3 are Engine-only smoke tests — no real `EngineClient`/`RadioClient` C++ code exists yet, and none of this needs to survive a reboot cleanly (that's [Phase 10](delivery-plan.md#phase-10--packaging)'s systemd units). Once both are confirmed working here:

- The actual `EngineClient` (REST+WebSocket client for go-librespot) and `RadioClient` (JSON IPC client for mpv) get implemented in the C++ controller, against the interfaces already proven correct in Phase 1.
- [Phase 4](delivery-plan.md#phase-4--the-real-hifiberry-digi-pro-and-both-engines-sharing-it) is where the two Engines get soak-tested sharing the HAT for real — switching Source repeatedly and rapidly — closing out the still-open item at the bottom of [ADR 0004](adr/0004-audio-device-sharing.md).
