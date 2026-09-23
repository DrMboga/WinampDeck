# Audio device sharing between go-librespot and mpv

Status: accepted

Both go-librespot and mpv run as long-lived processes for the Deck's entire uptime, with only one audible ("the Source") at a time. This requires them to share the Digi+ Pro's ALSA device without an `EBUSY` collision.

Research ([audio-device-sharing.md](../../.tracker/controller-v1/research/audio-device-sharing.md), reading each project's source directly) found: go-librespot already closes its ALSA handle (`snd_pcm_close`) on pause/idle and only reopens on resume — no change needed there. mpv's `pause` IPC property only issues a hardware pause (`snd_pcm_pause`), which keeps the device open; deselecting its audio track (`set_property aid no`) makes it fully close instead, with `aid auto` reselecting/reopening it.

Decided: route both through a `dmix`-wrapped ALSA device (not a raw `hw:` device), so switching Source never risks an open-time collision even at the moment of the switch. The controller mutes/unmutes Radio via `aid no`/`aid auto` (not plain `pause`), and drives go-librespot via its normal pause/resume, which already behaves correctly. PipeWire was considered (already planned later for the spectrum analyzer) but deferred — its systemd-user-session/`linger` requirements are the same class of headless-setup cost already avoided for MPRIS/D-Bus, and it isn't needed just to solve this. The Digi+ Pro requires `dtoverlay=hifiberry-digi-pro` specifically, not the plain `hifiberry-digi` overlay.

This is based on reading source/docs, not a real-hardware test — treat as provisional until smoke-tested on the actual board (see open questions in the research file).

**Update (2026-09-22):** first real-hardware check passed. On the actual Pi 3B with the AOIDE Digi Pro clone, running Raspberry Pi OS 64-bit Lite with `/boot/firmware/config.txt` set to `dtparam=audio=off` plus `dtoverlay=hifiberry-digi-pro`, `aplay -l` lists the card correctly (`card 0: sndrpihifiberry`) and `speaker-test -D hw:0,0 -c 2 -t sine` produced audible sound on both channels. This confirms the overlay and the physical DAC/output path work on the target hardware.

**Update (2026-09-22, second check):** `dmix` mixing itself confirmed working on this driver. A named `dmixer` PCM was defined in `/etc/asound.conf` wrapping `hw:sndrpihifiberry` (per HiFiBerry's own template in the research file, adapted to reference the card by name rather than a numeric index so it survives card-order changes):

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

`pcm.!default` is deliberately left untouched — go-librespot and mpv will each be pointed at `dmixer` explicitly by name once they're installed, rather than relying on the system default.

Two concurrent `speaker-test -D dmixer` processes (440Hz and 880Hz) played simultaneously with both tones audible and no `EBUSY`/device-busy error opening the second stream. This resolves [research open questions 1 and 2](../../.tracker/controller-v1/research/audio-device-sharing.md) — the old `i2s-mmap` overlay dependency is indeed moot on current Raspberry Pi OS, and `dmix` genuinely mixes cleanly on the `hifiberry-digi-pro` driver, at least for two generic ALSA clients.

What's still unvalidated is go-librespot and mpv specifically as the two `dmix` clients — one paused via `aid no`/normal pause, one playing, switching repeatedly — since neither engine is installed on the Pi yet. That remains the open item, closed out by Phase 4 ([delivery-plan.md](../delivery-plan.md)).

**Update (2026-09-23):** first real audio (not just `speaker-test` tones) through `dmixer` surfaced a rate-mismatch bug. go-librespot, configured with `audio_device: dmixer`, played Spotify tracks audibly sped up and pitched up. Cause: `dmixer`'s `slave.rate` is hardcoded to 48000 (the `asound.conf` above), but Spotify's stream is natively 44.1kHz, and opening the bare `dmix`-type PCM directly doesn't perform sample-rate conversion for a mismatched client. Confirmed by a clean A/B on the real board: go-librespot on its default config (not pointed at `dmixer` at all) played at normal speed; the same track through bare `dmixer` sped up; switching `audio_device` to **`plug:dmixer`** (ALSA's `plug` layer wrapping the named `dmixer` PCM, which does automatic rate/format conversion) fixed it, still going through the shared mixer. **Both Engines must reference `plug:dmixer`, never bare `dmixer`** — mpv's radio streams arrive at just as many different rates as Spotify's fixed 44.1kHz, so this isn't a Spotify-specific fix. See [pi-setup.md](../pi-setup.md) for the corrected config.

**Update (2026-09-23, mpv confirmed):** the `pause`-vs-`aid no` device-release distinction this ADR's decision depends on is confirmed for mpv specifically, on the real board, through `--audio-device=alsa/plug:dmixer`: plain `set_property pause true` left `/proc/asound/card0/pcm0p/sub0/status` showing `state: RUNNING` (not `PAUSED` as on a raw `hw:` device — `dmix`/`plug` virtual devices don't expose ALSA's hardware-pause capability, so mpv's ALSA driver never calls `snd_pcm_pause()` here; it just stops feeding new audio while leaving the substream open), while `set_property aid no` closed the substream outright (`status` read failed — file gone) and `aid auto` reopened it (`RUNNING` again). The state label differs from the raw-device case originally described above, but the property that actually matters — held open vs. actually released — is exactly as decided. What's still open is go-librespot and mpv as *simultaneous* `dmix` clients, switching Source repeatedly — that's Phase 4's job.
