# Drop SQLite, the Frontend, and Spotify Web API auth from v1

Status: accepted

The original design planned a SQLite database (`settings`, `radio_station`, `spotify_auth`, `play_history` tables) and a web Configuration Server for Spotify auth, radio station management, device settings, and diagnostics.

Two upstream decisions removed most of the need for this. go-librespot's Zeroconf-based Spotify Connect auth needs no OAuth or token storage ([ADR 0001](0001-use-go-librespot-for-spotify-connect.md)), eliminating `spotify_auth` and the login flow. Separately, `settings` (brightness, LCD scroll speed) turn out to be set once during debug and never changed at runtime, so they don't need to be database-backed or web-editable; and `play_history` had no consumer — nothing in the design reads it back.

Decided: settings live in a plain config file read once at startup, hand-edited over SSH. The radio station list lives in a hand-edited `stations.csv` loaded into memory at startup and browsed on-device via the Station List UI (Repeat button, Radio Source). `play_history` is dropped outright. The web Configuration Server is dropped entirely — diagnostics happen over SSH and logs, not a UI. SQLite3 is dropped from the dependency list; nothing left in the design needs relational storage.
