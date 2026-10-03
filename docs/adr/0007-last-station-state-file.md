# The Last Station, in a state file the controller writes

Status: accepted (2026-10-03); not built yet

[ADR 0002](0002-drop-sqlite-and-frontend.md) decided that the Deck keeps no state of its own: everything persistent is the config file and `stations.csv`, both hand-edited and only read by the controller. [ADR 0006](0006-config-files-and-engine-processes.md) put those in `/etc/winampdeck/`. In daily use that turned out to have one cost. After every power-on, the first switch to Internet Radio tunes the first Station in the list, and the Station actually listened to has to be found again.

Decided: the controller remembers one value, the **Last Station**, in a file it writes itself. This amends ADR 0002's rule rather than reversing it. There is still no database, no history and no settings changed at runtime; the reasons for dropping those stand. Spec: [.tracker/last-station/spec.md](../../.tracker/last-station/spec.md).

**It lives in `/var/lib/winampdeck/state.json`, not in `/etc/winampdeck/`.** `/etc/winampdeck/` stays what ADR 0006 made it: files a person edits and the controller only reads. What the controller writes goes in a directory of its own, which systemd creates for the unit. Writing into `config.json` was ruled out: the controller would be rewriting a hand-edited file, losing its comments, and a crash mid-write could damage the settings.

**The Station is identified by its position in `stations.csv`.** The stream URL would survive reordering and renaming, and was the recommendation. Position was chosen because the list is edited rarely and it's the simplest thing that works. The accepted cost: after lines are inserted or reordered, the remembered position refers to whichever Station now sits there. A position past the end of the list means the first Station.

**It is saved only at Safe Shutdown.** Saving a few seconds after each tune would also survive a power cut, and was agreed first, then changed. Saving at Safe Shutdown is one rule, needs no timer, and writes to the SD card at a single, deliberate moment. The accepted cost: after a power cut, an unplug or a reboot, the Deck comes back with the Station from the last Safe Shutdown. If that becomes annoying, saving shortly after each tune is the alternative to return to.

**The state file is read leniently, unlike `config.json`.** A mistake in `config.json` stops the controller, because a person made it and should hear about it. The state file is written by the controller, so a missing, damaged or out-of-range one just means "nothing remembered": the first Station is tuned, and the Deck never refuses to start over it. A failed save is logged and doesn't hold up the Safe Shutdown.

**The rules live in `PlayerController`, behind a new `StationMemory` interface**, in the same way `SystemControl` and `Scheduler` were added, so that when it's saved, what's saved and the fallback are all covered by the hardware-free tests.
