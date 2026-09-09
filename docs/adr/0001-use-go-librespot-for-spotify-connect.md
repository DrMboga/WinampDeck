# Use go-librespot, not mainline librespot, as the Spotify Connect Engine

Status: accepted

The Deck needs its Spotify Connect Engine to do two things: report "now playing" metadata locally (for the LCD/TFT), and accept local playback control (Play/Pause/Next/Previous from the physical buttons) — both regardless of which Spotify account is currently connected via Zeroconf, since any family member/guest can connect their own account.

Research ([librespot-events-vs-spotify-api-polling.md](../../.tracker/controller-v1/research/librespot-events-vs-spotify-api-polling.md), [librespot-local-playback-control.md](../../.tracker/controller-v1/research/librespot-local-playback-control.md)) found that mainline librespot (Rust, the reference implementation) covers metadata well via its `--onevent` hook, but exposes **no local control surface at all** — a maintainer confirmed playback control can only arrive via the Spotify Connect protocol from a remote client, not from a local process. MPRIS support exists only as long-unmerged PRs. The underlying `play()`/`next()`/`prev()` calls exist in librespot's Rust internals but are only reachable by embedding librespot as a library and building our own control+event wrapper around it.

go-librespot (a Go reimplementation) ships both needs today, in one dependency: a local REST/WebSocket API delivering full track metadata and accepting `play`/`pause`/`next`/`prev`/`seek` commands, bound to localhost, no OAuth or D-Bus required, and correctly scoped to whichever account is currently connected (not account-locked). We're taking on a smaller, single-maintainer project over the org-maintained reference implementation specifically to get local control without writing our own Rust wrapper.
