# Audio device sharing between go-librespot and mpv

Status: accepted

Both go-librespot and mpv run as long-lived processes for the Deck's entire uptime, with only one audible ("the Source") at a time. This requires them to share the Digi+ Pro's ALSA device without an `EBUSY` collision.

Research ([audio-device-sharing.md](../../.tracker/controller-v1/research/audio-device-sharing.md), reading each project's source directly) found: go-librespot already closes its ALSA handle (`snd_pcm_close`) on pause/idle and only reopens on resume — no change needed there. mpv's `pause` IPC property only issues a hardware pause (`snd_pcm_pause`), which keeps the device open; deselecting its audio track (`set_property aid no`) makes it fully close instead, with `aid auto` reselecting/reopening it.

Decided: route both through a `dmix`-wrapped ALSA device (not a raw `hw:` device), so switching Source never risks an open-time collision even at the moment of the switch. The controller mutes/unmutes Radio via `aid no`/`aid auto` (not plain `pause`), and drives go-librespot via its normal pause/resume, which already behaves correctly. PipeWire was considered (already planned later for the spectrum analyzer) but deferred — its systemd-user-session/`linger` requirements are the same class of headless-setup cost already avoided for MPRIS/D-Bus, and it isn't needed just to solve this. The Digi+ Pro requires `dtoverlay=hifiberry-digi-pro` specifically, not the plain `hifiberry-digi` overlay.

This is based on reading source/docs, not a real-hardware test — treat as provisional until smoke-tested on the actual board (see open questions in the research file).
