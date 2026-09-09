# WinampDeck

A Raspberry Pi–based hardware deck, built into a 3D-printed Winamp-skin panel, that plays Spotify Connect and internet radio out through a digital (optical S/PDIF) output. Physical buttons and two small displays let it be operated without a phone or browser.

## Language

**Deck**:
The physical device as a whole — enclosure, panel, Raspberry Pi, HAT, and controller software together.
_Avoid_: box, unit, device (when it could mean just the Pi)

**Source**:
Which single Engine is currently audible: Spotify, Internet Radio, or Stopped. Exactly one Source is audible at a time; switching mutes/pauses the previously-audible Engine and unmutes/resumes the new one. Engines themselves keep running regardless of which Source is audible.
_Avoid_: player mode, state, active engine

**Engine**:
One of the two long-lived external processes that produce audio — librespot for Spotify, mpv for Internet Radio. Both run for the Deck's entire uptime so librespot stays visible as a Spotify Connect endpoint at all times; the controller orchestrates which one is audible but does not stop either outright.
_Avoid_: backend, player

**Eject**:
The physical panel button whose short press toggles the Source between Spotify and Internet Radio, and whose long press (~2s) triggers a Safe Shutdown. Named for its position on the original Winamp skin, not for any physical-media function.
_Avoid_: power button, source button

**Safe Shutdown**:
Issuing a clean OS poweroff, triggered by a long press of Eject, so it's safe to remove power from the Deck afterward.
_Avoid_: power off, halt

**Station**:
A named internet radio stream the Deck can tune to, with an associated logo image. Maintained as a fixed, hand-edited list, not managed through the Deck itself.
_Avoid_: channel, preset

**Station List**:
The TFT screen mode showing a scrollable list of Stations to browse, opened and closed with Repeat while Internet Radio is the Source.
_Avoid_: menu, station menu
