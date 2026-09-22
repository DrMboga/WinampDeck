# Audio device sharing between go-librespot and mpv

Status: accepted

Both go-librespot and mpv run as long-lived processes for the Deck's entire uptime, with only one audible ("the Source") at a time. This requires them to share the Digi+ Pro's ALSA device without an `EBUSY` collision.

Research ([audio-device-sharing.md](../../.tracker/controller-v1/research/audio-device-sharing.md), reading each project's source directly) found: go-librespot already closes its ALSA handle (`snd_pcm_close`) on pause/idle and only reopens on resume — no change needed there. mpv's `pause` IPC property only issues a hardware pause (`snd_pcm_pause`), which keeps the device open; deselecting its audio track (`set_property aid no`) makes it fully close instead, with `aid auto` reselecting/reopening it.

Decided: route both through a `dmix`-wrapped ALSA device (not a raw `hw:` device), so switching Source never risks an open-time collision even at the moment of the switch. The controller mutes/unmutes Radio via `aid no`/`aid auto` (not plain `pause`), and drives go-librespot via its normal pause/resume, which already behaves correctly. PipeWire was considered (already planned later for the spectrum analyzer) but deferred — its systemd-user-session/`linger` requirements are the same class of headless-setup cost already avoided for MPRIS/D-Bus, and it isn't needed just to solve this. The Digi+ Pro requires `dtoverlay=hifiberry-digi-pro` specifically, not the plain `hifiberry-digi` overlay.

This is based on reading source/docs, not a real-hardware test — treat as provisional until smoke-tested on the actual board (see open questions in the research file).

**Update (2026-09-22):** first real-hardware check passed. On the actual Pi 3B with the AOIDE Digi Pro clone, running Raspberry Pi OS 64-bit Lite with `/boot/firmware/config.txt` set to `dtparam=audio=off` plus `dtoverlay=hifiberry-digi-pro`, `aplay -l` lists the card correctly (`card 0: sndrpihifiberry`) and `speaker-test -D hw:0,0 -c 2 -t sine` produced audible sound on both channels. This confirms the overlay and the physical DAC/output path work on the target hardware.

**Update (2026-09-22, second check):** `dmix` mixing itself confirmed working on this driver. A named `dmixer` PCM was defined in `/etc/asound.conf` wrapping `hw:sndrpihifiberry` (per HiFiBerry's own template in the research file), and two concurrent `speaker-test -D dmixer` processes (440Hz and 880Hz) played simultaneously with both tones audible and no `EBUSY`/device-busy error opening the second stream. This resolves [research open questions 1 and 2](../../.tracker/controller-v1/research/audio-device-sharing.md) — the old `i2s-mmap` overlay dependency is indeed moot on current Raspberry Pi OS, and `dmix` genuinely mixes cleanly on the `hifiberry-digi-pro` driver, at least for two generic ALSA clients.

What's still unvalidated is go-librespot and mpv specifically as the two `dmix` clients — one paused via `aid no`/normal pause, one playing, switching repeatedly — since neither engine is installed on the Pi yet. That remains the open item, closed out by Phase 4 ([delivery-plan.md](../delivery-plan.md)).
