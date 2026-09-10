# Hardware Wiring

This is the 40-pin header allocation for all of WinampDeck's hardware, targeting the **Raspberry Pi 3 Model B** ([ADR 0005](adr/0005-target-board-pi-3b.md)): the Digi Pro HAT (a Chinese clone, sold as "AOIDE Digi Pro"/"DollaTek HiFi Digi Pro"), the ST7735 TFT, the MCP23017 button/LED expander, and the HD44780 LCD's FC-113 (PCF8574) backpack. The 40-pin layout itself is identical across the whole 2B/3B/3B+/4B family, so none of this changed when the target board did.

Everything here is built from primary sources — Raspberry Pi's own GPIO header documentation, the actual `hifiberry-digi-pro` device-tree overlay source, Microchip's MCP23017 datasheet — kept in `.tracker/controller-v1/research/` for anyone who wants to check the sourcing. Two things could **not** be verified from any authoritative source and need a real-hardware check before you commit to soldering; they're called out plainly in [Verify before you solder](#verify-before-you-solder) rather than guessed at.

## Pins the HAT already claims — do not reuse these

The genuine HiFiBerry Digi+ Pro's overlay (which this clone is built to work with — same WM8804 chip, driven the same way) claims more pins than just the audio bus. Read directly from the overlay source:

| Physical pin | BCM GPIO | Claimed for |
|---|---|---|
| 3 | GPIO2 | I2C1 SDA — onboard WM8804 config, address `0x3B` |
| 5 | GPIO3 | I2C1 SCL — onboard WM8804 config |
| 12 | GPIO18 | I2S BCLK |
| 29 | GPIO5 | Clock-source select (44.1kHz family) — **Pro-only**, easy to miss |
| 31 | GPIO6 | Clock-source select (48kHz family) — **Pro-only**, easy to miss |
| 35 | GPIO19 | I2S LRCLK |
| 40 | GPIO21 | I2S DOUT |

Two more are reserved on every Pi HAT, regardless of which one:

| Physical pin | BCM GPIO | Claimed for |
|---|---|---|
| 27 | GPIO0 | ID_SD — HAT ID EEPROM only, never general-purpose |
| 28 | GPIO1 | ID_SC — HAT ID EEPROM only, never general-purpose |

Note the I2C1 bus (pins 3/5) is *shared*, not exclusive — our own I2C devices (MCP23017, LCD backpack) go on this same bus, at their own addresses, alongside the WM8804. That's normal I2C; it's not a conflict.

One more pin to steer clear of, for a different reason: this specific clone board has a **built-in IR receiver** the genuine HiFiBerry doesn't have. No schematic exists for this exact product, but the closest thing found — a build script from the same AOIDE product family — wires its IR receiver to **GPIO26** (physical pin 37). Treat that as a caution, not a confirmed fact (see below), and avoid assigning anything of ours there.

## Everything else: our assignments

| Physical pin | BCM GPIO | Signal | Goes to |
|---|---|---|---|
| 1 | — (3.3V) | Power | MCP23017 VDD, LCD backpack VCC (see voltage note below) |
| 6 / 9 / 14 / 20 / 25 / 30 / 34 / 39 | — (GND) | Ground | Common ground, all peripherals |
| 18 | GPIO24 | TFT DC / RS | ST7735 — command/data select |
| 22 | GPIO25 | TFT RESET | ST7735 — hard reset |
| 19 | GPIO10 (MOSI) | TFT SPI data | ST7735 — SPI0 |
| 23 | GPIO11 (SCLK) | TFT SPI clock | ST7735 — SPI0 |
| 24 | GPIO8 (CE0) | TFT chip select | ST7735 — SPI0 |
| 32 | GPIO12 (PWM0) | TFT backlight | ST7735 BLK — PWM, for the config file's brightness setting |
| 13 | GPIO27 | MCP23017 interrupt | MCP23017 INTA |

`GPIO9` (MISO, pin 21) comes bundled with enabling SPI0 but the TFT never drives it — leave unconnected. `GPIO7` (CE1, pin 26) is free if a second SPI device is ever added.

## The shared I2C1 bus

Three devices, three addresses, no collisions:

| Device | Address |
|---|---|
| WM8804 (on the HAT itself) | `0x3B` |
| MCP23017 | `0x20` (all three address pins A0/A1/A2 tied to GND) |
| LCD backpack | `0x27` **or** `0x3F` — depends on which chip is actually on your FC-113 (see below) |

## MCP23017 wiring

- **VDD** → 3.3V, **VSS** → GND.
- **A0, A1, A2** → GND (gives address `0x20`).
- **RESET** → 3.3V (held high; we don't need software-triggered resets).
- **INTA** → GPIO27 (pin 13). Only port A's interrupt is used, so INTB is left unconnected — no need to mirror them.
- **GPA0–GPA7** → the 8 buttons (Previous, Stop, Pause, Play, Next, Eject, Shuffle, Repeat), each with `GPINTEN` enabled so a press raises INTA.
- **GPB0–GPB1** → the 2 LEDs (Shuffle, Repeat). Add a current-limiting resistor per LED as usual.

## ST7735 TFT wiring

Standard 8-signal breakout: **VCC** → 3.3V, **GND** → GND, **SCL** → GPIO11, **SDA** → GPIO10, **CS** → GPIO8, **RES** → GPIO25, **DC** (a.k.a. RS/A0) → GPIO24, **BLK** → GPIO12 (PWM, for brightness).

## LCD backpack wiring

**GND/VCC/SDA/SCL** are the only four pins the FC-113 exposes — SDA/SCL go on the shared I2C1 bus above.

## Verify before you solder

Two things below have no authoritative source and need a real check on your actual boards — everything above this section does have one, cited in `.tracker/controller-v1/research/`.

1. **The clone HAT's exact pin usage is inferred, not confirmed.** Everything in the first table comes from the *genuine* HiFiBerry Digi+ Pro's overlay source, on the reasoning that this clone uses the same WM8804 chip and is driven by the same `hifiberry-digi-pro` overlay (a Raspberry Pi forum thread on this exact card supports that). But nobody has published this clone's own schematic, so it's not a certainty — and the IR receiver's GPIO26 is a weaker version of the same problem: it's a plausible convention pulled from the same product family, not a verified fact. **Before wiring anything to GPIO26, or trusting the HAT's pin list, run `raspi-gpio funcs` / `i2cdetect -y 1` with the HAT connected and nothing else attached**, and confirm the WM8804 shows up at `0x3B` and nothing unexpected is toggling on GPIO26.
2. **LCD backpack voltage.** The FC-113 is a generic, undocumented module — some are built around the PCF8574 (default address `0x27`), others the PCF8574A (`0x3F`); check which you actually have with `i2cdetect`. More importantly: **it must run its I2C side at 3.3V, not 5V, because it shares a bus with the MCP23017 and the WM8804, both fixed at the Pi's native 3.3V logic level** — mixing bus voltages on one shared I2C bus without a level shifter risks damaging the Pi's GPIO. Running the backpack at 3.3V may dim the LCD's contrast (HD44780 panels are nominally rated for 5V), which is a cosmetic tradeoff, not a safety one — confirm the display is legible at 3.3V before finalizing, rather than reaching for 5V to fix contrast.

(A Raspberry Pi 4B board-revision compatibility issue was found and ruled the Pi 4 out entirely — see [ADR 0005](adr/0005-target-board-pi-3b.md) — which is why this doc no longer targets it.)
