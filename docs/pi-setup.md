# Raspberry Pi 3 Setup — Phases 2–5

This is a from-scratch setup guide for the Raspberry Pi 3 Model B ([ADR 0005](adr/0005-target-board-pi-3b.md)) running the Digi Pro HAT, taking it from a blank SD card through [Phase 2 and Phase 3](delivery-plan.md#phases-23--wire-in-the-real-engines-can-happen-in-parallel-and-dont-need-the-pi-yet) of the delivery plan (getting the real go-librespot and mpv Engines installed and smoke-tested on the actual board), Phase 4 (both Engines sharing the HAT), and [Phase 5](#phase-5--buttons-and-leds) (building the controller on the Pi and bringing up the buttons and LEDs). It assumes default Raspberry Pi OS (64-bit), no monitor/keyboard — everything over SSH.

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

### 6.5 Optional: run both Engines together as a first look at Phase 4 (superseded by [Phase 4](#phase-4--both-engines-sharing-the-hat) below)

Not required for Phase 3's exit criteria, but if go-librespot from Phase 2 is still running, this is a cheap early look at what Phase 4 will validate properly later: start Spotify playing, then start mpv playing a station too (both on `dmixer`), and confirm no `EBUSY` and no audio glitch. Then use go-librespot's `/player/pause` and mpv's `aid no`/`aid auto` to switch which one is actually audible a few times in a row.

**Phase 3 exit criteria met when:** mpv responds correctly to loadfile/pause/resume over IPC, and you've directly observed the `pause`-keeps-device-open vs. `aid no`-releases-device distinction on this board. **Confirmed 2026-09-23.**

---

## Phase 4 — both Engines sharing the HAT

**Goal**: run go-librespot and mpv side by side on `plug:dmixer` and switch Source repeatedly and rapidly, confirming no device-busy error and no audio glitch ([delivery plan](delivery-plan.md#phase-4--the-real-hifiberry-digi-pro-and-both-engines-sharing-it)). **Done 2026-09-25.**

### 7.1 Start both Engines with log files

```bash
tmux kill-session -t librespot 2>/dev/null; tmux kill-session -t mpv 2>/dev/null
tmux new -d -s librespot "cd ~/go-librespot && ./go-librespot --config_dir ~/go-librespot 2>&1 | tee ~/librespot.log"
tmux new -d -s mpv "mpv --idle --no-video --input-ipc-server=/tmp/mpv-socket --audio-device=alsa/plug:dmixer --log-file=$HOME/mpv.log"
```

Then start Spotify on WinampDeck from the phone, and load a Station. **Select the audio track before loading.** If `aid` was left at `no` (for example by the soak script's last step), mpv drops the new stream immediately (`No video or audio streams selected` in `mpv.log`), even though `loadfile` replies `success`:

```bash
echo '{"command": ["set_property", "aid", "auto"]}' | socat - /tmp/mpv-socket
echo '{"command": ["loadfile", "https://streams.radiobob.de/ozzyosbourne/mp3-192"]}' | socat - /tmp/mpv-socket
# both audible, mixed. Make Spotify the Source:
echo '{"command": ["set_property", "aid", "no"]}' | socat - /tmp/mpv-socket
```

### 7.2 The switching soak script

`~/switch-soak.sh <cycles> <seconds-per-source>` switches Source the way ADR 0004 decided: go-librespot through `pause`/`resume`, mpv through `aid auto`/`aid no`. Each switch mutes the outgoing Engine and then unmutes the incoming one. After every switch the script records both Engines' replies and the hardware PCM state, and it refuses to start if mpv has no Station loaded.

```bash
cat > ~/switch-soak.sh <<'EOF'
#!/usr/bin/env bash
# Usage: ./switch-soak.sh <cycles> <seconds-per-source>
N=${1:-20}; DWELL=${2:-3}
LS=http://localhost:3678; SOCK=/tmp/mpv-socket
LOG=~/soak-${N}x${DWELL}-$(date +%H%M%S).log
mpv() { echo "{\"command\": $1}" | socat - "$SOCK" | grep -o '"error":"[^"]*"'; }
mpv_idle() { echo '{"command": ["get_property","idle-active"]}' | socat - "$SOCK" | grep -o '"data":[a-z]*'; }
ls_post() { curl -s -o /dev/null -w '%{http_code}' -X POST "$LS/player/$1"; }
ls_paused() { curl -s "$LS/status" | python3 -c 'import sys,json;d=json.load(sys.stdin);print("paused" if d.get("paused") else ("stopped" if d.get("stopped") else "playing"))' 2>/dev/null || echo "status-err"; }
hw() { head -1 /proc/asound/card0/pcm0p/sub0/status 2>/dev/null | tr -d ' ' || echo gone; }
[[ $(mpv_idle) == '"data":false' ]] || { echo "ABORT: mpv has no station loaded - loadfile first"; exit 1; }
bad=0; silent=0
for i in $(seq 1 "$N"); do
  a=$(ls_post pause); b=$(mpv '["set_property","aid","auto"]')          # -> Radio
  sleep "$DWELL"
  h=$(hw); echo "$i RADIO   ls_pause=$a mpv_aid=$b ls=$(ls_paused) hw=$h" | tee -a "$LOG"
  [[ $a == 2* && $b == *success* ]] || bad=$((bad+1)); [[ $h == *RUNNING* ]] || silent=$((silent+1))
  c=$(mpv '["set_property","aid","no"]'); d=$(ls_post resume)          # -> Spotify
  sleep "$DWELL"
  h=$(hw); echo "$i SPOTIFY mpv_aid=$c ls_resume=$d ls=$(ls_paused) hw=$h" | tee -a "$LOG"
  [[ $c == *success* && $d == 2* ]] || bad=$((bad+1)); [[ $h == *RUNNING* ]] || silent=$((silent+1))
done
echo "DONE: $N cycles, dwell ${DWELL}s, failed commands: $bad, device-not-running: $silent, log: $LOG" | tee -a "$LOG"
EOF
chmod +x ~/switch-soak.sh

~/switch-soak.sh 10 5 && ~/switch-soak.sh 30 1 && ~/switch-soak.sh 50 0.3
```

Afterwards, check the logs. Note that the pattern `err` also matches inside "hifib**err**y", so filter `mpv.log` by log level instead:

```bash
grep -iE "busy|xrun|underrun|underflow" ~/librespot.log ~/mpv.log
grep -E "\]\[(e|w|f)\]" ~/mpv.log | tail -20
dmesg | grep -iE "snd|hifiberry|i2s|xrun" | tail -20
vcgencmd get_throttled
```

### 7.3 Results (2026-09-25)

- **Mixing:** both Engines audible at the same time, no `EBUSY`.
- **Soak:** 90 switches (10×5s, 30×1s, 50×0.3s), 0 failed commands, hardware PCM `RUNNING` after every switch, no busy/XRUN/underrun in either log, `throttled=0x0`. Switching was audibly smooth with no clicks, and Radio came back with no delay, resuming where it was muted rather than at the live position.
- **Long mute:** after a mute of a few minutes, the station's server had dropped the connection (`tls: IO error: End of file` / `Stream ends prematurely` in `mpv.log`), so `aid auto` brought back silence (`demuxer-cache-state` `eof: true`, PCM `closed`, yet `idle-active: false`). Switching to Radio in the controller therefore needs a re-`loadfile`. How long a mute a stream survives is still unmeasured.
- **`dmesg`:** some `bcm2835-i2s ... FIFO clear timed out: no PCM clock` warnings, only when the last client released the device in an odd state (an Engine killed mid-play, a soak run with no Station loaded). Nothing audible. Treated as harmless; see [ADR 0004](adr/0004-audio-device-sharing.md).
- **Network:** Wi-Fi dropped partway through (`wpa_supplicant: wlan0: Failed to initiate sched scan`, NetworkManager `link timed out`, no reconnect). The Pi stayed up and both Engines kept running, but it was unreachable until Ethernet was plugged in (new DHCP address). The rest ran over Ethernet with `sudo nmcli radio wifi off`. Turn Wi-Fi back on with `sudo nmcli radio wifi on`.

---

## Phase 5 — buttons and LEDs

**Goal** ([delivery plan](delivery-plan.md#phase-5--buttons-and-leds-can-happen-in-parallel-with-24)): the real `HardwareIO` drives the 8 buttons and 2 LEDs through the MCP23017, and every button and LED does exactly what the Phase 1 tests say. This is the first time the C++ code runs on the Pi, so this section also covers getting the repo onto the Pi and building it there.

The tool for this phase is `winampdeck-panel-test`. It has two modes:

- **Buttons mode** (no arguments) checks the wiring. It prints every press and release, and Shuffle and Repeat toggle their own LED.
- **Controller mode** (`--controller`) runs the real `PlayerController` on the real buttons and LEDs. The Engines are simulated and print the commands they receive. This is the phase's actual exit criterion.

Neither mode touches go-librespot or mpv, and Safe Shutdown is only printed, never carried out.

### 8.1 Check the MCP23017 answers on I2C

With the MCP23017 and both perfo boards connected per [wiring.md](wiring.md#mcp23017-wiring):

```bash
i2cdetect -y 1
# expect: 20 (the MCP23017) and UU at 3b (the WM8804, as before)
```

If `20` is missing, don't go further. Check VDD/GND, SDA→pin 3, SCL→pin 5, A0/A1/A2 to GND, and RESET pulled up to 3.3V through 10kΩ. A floating RESET keeps the chip silent.

### 8.2 Give the Pi an SSH key for GitHub

Do this once. It lets the Pi clone and pull the repo over SSH (and push, if you ever commit from the Pi).

1. Generate a key on the Pi. Press Enter to accept the default file. A passphrase is optional; with an empty one, `git pull` never prompts.
   ```bash
   ssh-keygen -t ed25519 -C "winampdeck-pi"
   ```
2. Print the public half and copy the whole line (it starts with `ssh-ed25519`):
   ```bash
   cat ~/.ssh/id_ed25519.pub
   ```
3. On GitHub: your avatar → **Settings** → **SSH and GPG keys** → **New SSH key**. Title: `winampdeck-pi`, key type: **Authentication Key**. Paste the line and save.

   Alternative: if the Pi should only ever *read* this one repo, add the key as a **deploy key** under the repo's **Settings** → **Deploy keys** instead (leave "Allow write access" unticked). It can then clone and pull WinampDeck and nothing else.
4. Test it:
   ```bash
   ssh -T git@github.com
   ```
   The first time, it asks you to confirm GitHub's host key. The ED25519 fingerprint should be `SHA256:+DiY3wvvV6TuJJhbpZisF/zLDA0zPMSvHdkr4UvCOqU` ([GitHub's published fingerprints](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/githubs-ssh-key-fingerprints)). Type `yes`. Expect: `Hi DrMboga! You've successfully authenticated, but GitHub does not provide shell access.`
5. Only if you'll commit from the Pi, set your identity:
   ```bash
   git config --global user.name "Your Name"
   git config --global user.email "you@example.com"
   ```

### 8.3 Install the build tools and pigpio

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git
grep VERSION_CODENAME /etc/os-release
```

pigpio is packaged in Raspberry Pi OS **Bookworm**, but was dropped in **Trixie**:

- **Bookworm:**
  ```bash
  sudo apt install -y libpigpio-dev
  ```
- **Trixie:** build it from source. It installs into `/usr/local`, which CMake searches by default:
  ```bash
  git clone https://github.com/joan2937/pigpio.git ~/pigpio-src
  cd ~/pigpio-src && make -j2
  sudo make install
  sudo ldconfig
  cd ~
  ```
  `sudo make install` is expected to fail at the end with `ModuleNotFoundError: No module named 'distutils'` / `make: *** [Makefile:107: install] Error 1`. That's pigpio's Python bindings: `distutils` was removed from Python 3.12+. The C library, headers and tools are already installed by then, and only the man pages and the `ldconfig` it would have run are skipped, which is why `ldconfig` is run separately above. Confirm the parts the controller needs are there:
  ```bash
  ls /usr/local/include/pigpio.h /usr/local/lib/libpigpio.so*
  # expect: pigpio.h, libpigpio.so, libpigpio.so.1
  ```

The controller uses pigpio as a library, which needs exclusive access to the GPIO hardware. The `pigpiod` daemon must not run at the same time. If it's installed, stop and disable it:

```bash
systemctl is-active pigpiod && sudo systemctl disable --now pigpiod
```

### 8.4 Clone and build

```bash
cd ~
git clone git@github.com:DrMboga/WinampDeck.git
cd WinampDeck
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWINAMPDECK_WITH_PIGPIO=ON
cmake --build build -j1
ctest --test-dir build --output-on-failure
```

- The first `cmake` downloads the pinned header-only dependencies and GoogleTest, so it needs internet access.
- `-j1` matters. With 1GB of RAM, even two compiles at once ran the Pi 3 out of memory (`c++: fatal error: Killed signal terminated program cc1plus` on `dependencies_test.cpp`, 2026-09-30). One at a time works. If a build is killed partway, re-running the same command carries on from where it stopped.
- To try the panel sooner, `cmake --build build -j1 --target winampdeck-panel-test` builds only the bring-up tool and skips the heavy test files.
- `ctest` runs the same unit tests CI runs, now on the Pi's own compiler.

To pick up new commits later:

```bash
cd ~/WinampDeck && git pull && cmake --build build -j1
```

### 8.5 Buttons mode — check the wiring

pigpio maps the SoC's registers directly, so it needs root:

```bash
sudo ./build/src/winampdeck-panel-test
```

Both LEDs blink once, then every press prints a line:

```
Buttons mode. Blinking both LEDs...
Press any button; Shuffle and Repeat toggle their LEDs. Ctrl+C quits.
pressed  Shuffle
  Shuffle LED on
released Shuffle (held 143 ms)
```

Go through this checklist:

- [ ] Both LEDs blinked at startup.
- [ ] Each of the 8 buttons prints its **own** name: Previous, Stop, Pause, Play, Next, Eject, Shuffle, Repeat. A wrong name means that button's wire is on the wrong MCP23017 pin. Compare with [wiring.md](wiring.md#buttons-on-perfo-boards).
- [ ] Each press prints exactly **one** `pressed` and one `released`, even when you tap quickly or press hard. Doubled lines mean contact bounce is getting through the 20ms debounce.
- [ ] Shuffle toggles the Shuffle LED and Repeat toggles the Repeat LED, not the other way round.
- [ ] Holding Eject for about 3 seconds reports `held` ≈ 3000 ms. That's the timing the Eject long press will rely on.
- [ ] Pressing two buttons together reports both.
- [ ] Ctrl+C prints `Bye.` and both LEDs go off.

**Troubleshooting**

- `pigpio failed to start`: run it with `sudo`, and check that `pigpiod` isn't running (8.3).
- `MCP23017 at 0x20 didn't answer`: go back to 8.1.
- LEDs work but no button ever prints anything: the interrupt line isn't arriving. Check the MCP23017's `ITB/ITA` pin → GPIO27 (physical pin 13). To check the buttons without the interrupt, run the tool once and quit it, which leaves the pull-ups configured. Then hold a button and read port A directly:
  ```bash
  i2cget -y 1 0x20 0x12   # 0xff with nothing pressed; a cleared bit per held button
  ```
- Everything works once and then stops responding: note exactly what you pressed and report it. The interrupt line is probably stuck low.

### 8.6 Controller mode — the Phase 5 exit criterion

```bash
sudo ./build/src/winampdeck-panel-test --controller
```

Now each press is followed by what `PlayerController` did about it. That includes Engine commands, LED changes, and what the LCD and TFT would show (Phases 6–7 put those on the real displays):

```
pressed  Eject
released Eject
  LCD: Spotify
  TFT: Now Playing, Spotify, stopped
```

The Spotify stand-in echoes each command back as go-librespot would report it. So `shuffle on` is followed a moment later by the Shuffle LED turning on, because LEDs follow the Engine's reported state, not the button. The three Stations are placeholders.

Walk through this checklist in order:

- [ ] **Stopped at startup.** Every button except Eject prints only `pressed`/`released`, and nothing else happens.
- [ ] **Eject (short) → Spotify.** LCD: `Spotify`.
- [ ] **Spotify buttons.** Play, Pause, Next and Previous each print their `Spotify:` command. Stop does nothing.
- [ ] **Spotify Shuffle/Repeat.** Shuffle prints `shuffle on`, then the Shuffle LED turns on. Pressing it again turns it off. Repeat works the same with the Repeat LED. Leave one of them on for the next step.
- [ ] **Eject → Internet Radio.** `Spotify: pause` (only if you'd pressed Play), `Radio: unmute`, `Radio: tune Station One`, LCD `Station One`. **Both LEDs go off**, because Shuffle has no LED in Radio.
- [ ] **Radio buttons.** Next/Previous tune the adjacent Station and wrap around the ends. Shuffle tunes a random *other* Station, and its LED stays off. Pause/Play print `Radio: pause`/`resume`.
- [ ] **Station List.** Repeat opens it: Repeat LED on, TFT `Station List`. Next/Previous move the highlight without tuning anything. Play tunes the highlighted Station and closes the list (LED off). Opening it again and pressing Repeat closes it without tuning.
- [ ] **Eject → back to Spotify.** `Radio: mute`, and the Shuffle/Repeat LEDs come back as you left them in Spotify.
- [ ] **Eject held ~2s.** At the 2-second mark, *while still held*, the LCD shows `Shutting down` and `System: Safe Shutdown (not carried out by the panel test)` prints. After that, every button is ignored. Ctrl+C to quit.

### 8.7 Check the buttons don't disturb audio

pigpio normally paces its GPIO sampling with the SoC's PCM block, which is the I2S interface the HAT plays through. `PigpioSession` switches it to the PWM block instead. To confirm that on the real board, start the Engines as in [7.1](#71-start-both-engines-with-log-files), play Spotify on the Deck from your phone, and run either panel-test mode for a minute or two while pressing buttons. Expect no dropouts or clicks, and nothing new in:

```bash
dmesg | grep -iE "snd|hifiberry|i2s" | tail
```

Note that the panel test doesn't control the Engines, so the music keeps playing no matter which buttons you press.

### 8.8 Results (2026-09-30)

Run on Raspberry Pi OS **Trixie** (64-bit), with pigpio built from source (8.3).

- **Build:** `-j2` ran out of memory compiling `dependencies_test.cpp` (`cc1plus` killed); `-j1` built everything, and all 80 tests passed on the Pi. 8.4 now uses `-j1`.
- **Buttons mode:** all 8 buttons reported their own names, with exactly one `pressed`/`released` per press, so no bounce got through the debounce. Shuffle and Repeat toggled their own LEDs. A 4-second Eject hold measured `4044 ms`.
- **Controller mode:** every item on the 8.6 checklist behaved as the Phase 1 tests specify:
  - Stopped ignores everything but Eject.
  - Spotify's Play/Pause/Next/Previous each sent their command, Stop did nothing, and the Shuffle/Repeat LEDs followed the Engine's reported state.
  - Switching to Radio paused Spotify, tuned the first Station and turned both LEDs off.
  - Next/Previous wrapped around the Stations, and Shuffle tuned a different Station with its LED staying off.
  - The Station List opened and closed with Repeat. Play re-tuned only when the highlight differed from the tuned Station, and Shuffle was ignored inside the list.
  - Returning to Spotify resumed it and restored its LEDs.
  - Holding Eject fired Safe Shutdown at the 2s mark while still held. Every button was ignored afterwards.
  - Radio Pause/Play weren't pressed on the panel; the unit tests cover them.
- **Audio (8.7):** with Spotify playing through the HAT, a few minutes of the panel test and button presses caused no dropouts or clicks, and `dmesg` showed nothing new beyond boot-time lines. Pacing pigpio off PWM instead of PCM keeps it clear of I2S.
- **Known artifact of the stand-in:** the TFT line shows Spotify as `stopped` even after Play, because the stand-in never reports the Deck as the active Connect device. That's where the TFT gets play status from. It goes away with the real `EngineClient`.

## Phase 6 — TFT

**Goal** ([delivery plan](delivery-plan.md#phase-6--tft)): the ST7735 shows `PlayerController`'s screens, Now Playing and the Station List, correctly and promptly for both Sources. The tool is `winampdeck-panel-test` again, with two new things:

- **TFT mode** (`--tft`) draws a test pattern for checking the panel's orientation, colour order and edges. After 10 seconds it cycles through the real screens. It doesn't use the buttons.
- **Controller mode** (`--controller`) now draws on the real TFT, with the 60 Stations from `data/stations.csv`. The Spotify stand-in connects on the first Play and plays three demo tracks with real album covers. That needs internet access, and without it the screen shows a placeholder instead of the cover.

Both modes take the same TFT options:

| Option | Default | What it's for |
|---|---|---|
| `--madctl 0xNN` | `0xA0` | Rotation, mirroring and colour order (the ST7735's MADCTL register) |
| `--offset COL,ROW` | `0,0` | Where the visible area starts in the controller's memory |
| `--spi-hz N` | `16000000` | SPI clock |
| `--brightness N` | `255` | Backlight, 0–255 |
| `--data DIR` | `data` | Where `stations.csv` and `logos/` are |

Run the tool from the repo root so `data` is found.

### 9.1 Before powering on

Wire the TFT per [wiring.md](wiring.md#st7735-tft-wiring). Check that **LEDA goes to GPIO12, not 5V**, because 5V can damage the screen.

SPI doesn't need enabling in `config.txt`. pigpio drives the SPI0 block's registers itself, the same way it drives I2C, so leave `dtparam=spi=on` commented out and the kernel's SPI driver won't claim the pins.

### 9.2 Build

```bash
cd ~/WinampDeck && git pull
cmake --build build -j1 --target winampdeck-panel-test   # the tool first
cmake --build build -j1 && ctest --test-dir build --output-on-failure
```

The build re-runs CMake, which downloads the new stb_image dependency, so the Pi needs internet access for it.

### 9.3 TFT mode: orientation, colours, edges

```bash
sudo ./build/src/winampdeck-panel-test --tft
```

The backlight comes on and the test pattern appears. The console prints how long a full-screen write took.

- [ ] **Orientation.** `TOP LEFT` reads normally in the top-left corner, and `BOTTOM RIGHT` in the bottom right. If not, try the other landscape values in turn: `--madctl 0x60` (rotated 180° from the default), then `0x20` and `0xE0` (the two mirror images).
- [ ] **Colours.** The bars read RED, GREEN and BLUE in those colours. If red and blue are swapped, add `0x08` to whichever `--madctl` value was right (`0xA0` → `0xA8`).
- [ ] **Edges.** The white frame is visible on all four edges, with no stray line of noise along any edge. If an edge is missing or noisy, the visible area is offset. Try `--offset 1,2` or `--offset 2,1`, which are common for 128×160 ST7735 modules.
- [ ] **Speed.** A full-screen write takes about 20–30ms at the default 16MHz. If any pixels are garbled, retry with `--spi-hz 8000000`. If everything is clean, try `24000000` and `32000000` and keep the fastest that stays clean. pigpio derives the clock from the Pi 3's core clock, which changes with load, so leave some margin.
- [ ] **The real screens**, one every 6 seconds after the first 10 seconds:
  - Stopped: `WINAMP` placeholder, `Press Eject to start`.
  - Spotify: the *Discovery* cover, clock running from 1:15, spectrum moving, progress bar.
  - Radio: the first Station's logo, with the stream title scrolling.
  - Station List: logo thumbnails, with the 4th Station highlighted in blue.
- [ ] Ctrl+C turns the backlight off and blanks the screen.

Note the `--madctl`, `--offset` and `--spi-hz` values that worked. They become the defaults in `St7735::Config`, so these flags won't be needed afterwards.

### 9.4 Controller mode: the Phase 6 exit criterion

```bash
sudo ./build/src/winampdeck-panel-test --controller   # plus any TFT options from 9.3
```

Walk through this checklist in order:

- [ ] **Stopped at startup:** `WINAMP` placeholder, stop indicator, `Press Eject to start`.
- [ ] **Eject → Spotify:** `SPOTIFY` placeholder, `Connect from Spotify app`.
- [ ] **Play:** the stand-in connects. Within a second or two the *Discovery* cover replaces the placeholder. The artist and title show, the clock counts up from 0:00, the spectrum moves and the progress bar fills.
- [ ] **Pause:** pause indicator. The clock freezes and blinks, and the spectrum falls to nothing.
- [ ] **Next/Previous:** the cover and text change. The long *Get Lucky* title scrolls after a moment. Going back to a track seen before shows its cover immediately, because covers are cached.
- [ ] **Eject → Radio:** the first Station's logo and name, with the clock from 0:00.
- [ ] **Next/Previous/Shuffle:** the logo and name follow the tuned Station, and the clock restarts at 0:00 each time.
- [ ] **Repeat:** the Station List opens with the tuned Station highlighted. Next/Previous move the highlight, the list scrolls to keep it in the middle row, and a long highlighted name scrolls. Play tunes it and returns to Now Playing. Repeat closes the list without tuning.
- [ ] **Promptness:** every press shows on the screen without noticeable lag, including fast repeated Next presses in the Station List.
- [ ] **Eject held ~2s:** `Shutting down` on the LCD line of the console. The TFT stays on its last screen.

### 9.5 Check the TFT doesn't disturb audio

As in [8.7](#87-check-the-buttons-dont-disturb-audio): start the Engines, play Spotify on the Deck from your phone, and leave `--tft` cycling through its screens for a few minutes. The spectrum animates the whole time, so SPI and the backlight PWM are busy throughout. Expect no dropouts or clicks, and nothing new in `dmesg | grep -iE "snd|hifiberry|i2s" | tail`. Also check `top`: the panel test should use only a few percent CPU while animating.

### 9.6 Results

**9.3, TFT mode (2026-10-01).** Run with the module on a breadboard, with no options, so with the defaults at the time: `--madctl 0x60`, `--offset 0,0`, 16MHz SPI.
- Orientation, colour order and all four edges were right first time, so no offset is needed.
- All four real screens drew correctly: the Stopped placeholder; Spotify with the *Discovery* cover downloaded and converted on the Pi, the clock, the spectrum and the progress bar; Radio with the Station logo and the scrolling Cyrillic stream title; and the Station List with thumbnails, highlight and scrollbar.
- The module will be mounted in the panel the other way round from the breadboard. So the default MADCTL is now `0xA0`, the same picture turned 180°, and the checks above were run before that change.

**9.4, controller mode:** not run yet.

---

## What's next

Phases 2–4 are Engine-only tests — no real `EngineClient`/`RadioClient` C++ code exists yet, and none of this needs to survive a reboot cleanly (that's [Phase 10](delivery-plan.md#phase-10--packaging)'s systemd units). Next:

- The actual `EngineClient` (REST+WebSocket client for go-librespot) and `RadioClient` (JSON IPC client for mpv) get implemented in the C++ controller, against the interfaces already proven correct in Phase 1.
- `RadioClient` must handle the two Phase 4 findings above: select the audio track before `loadfile`, and re-`loadfile` the Station when switching Source to Radio rather than relying on `aid auto` alone.
- Still open: a long-mute test (30+ minutes muted, then `aid auto`) to measure how long a muted stream survives, and Wi-Fi reliability with the HAT fitted.
