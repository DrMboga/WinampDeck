# Hardware Wiring

This is the 40-pin header allocation for all of WinampDeck's hardware, targeting the **Raspberry Pi 3 Model B** ([ADR 0005](adr/0005-target-board-pi-3b.md)): the Digi Pro HAT (a Chinese clone, sold as "AOIDE Digi Pro"/"DollaTek HiFi Digi Pro"), the ST7735 TFT, the MCP23017 button/LED expander, and the HD44780 LCD's FC-113 (PCF8574) backpack. The 40-pin layout itself is identical across the whole 2B/3B/3B+/4B family, so none of this changed when the target board did.

Everything here is built from primary sources — Raspberry Pi's own GPIO header documentation, the actual `hifiberry-digi-pro` device-tree overlay source, Microchip's MCP23017 datasheet, and the manufacturer's own manuals for the specific TFT, LCD/I2C-adapter, and MCP23017 (CJMCU-2317) breakout boards actually being used, purchased from AZ-Delivery (kept locally in `docs/`, not committed — see below) — kept in `.tracker/controller-v1/research/` for anyone who wants to check the sourcing. One thing could **not** be verified from any authoritative source and needs a real-hardware check before you commit to soldering; it's called out plainly in [Verify before you solder](#verify-before-you-solder) rather than guessed at.

The three AZ-Delivery product manuals this doc draws on aren't tracked in git (they're large vendor PDFs, not something the repo needs to carry) — if you're picking this up fresh, they cover the 1.77" TFT SPI screen, the 16x02 LCD + I2C adapter, and the MCP23017 I2C port expander, all available from AZ-Delivery's own site/product pages.

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
| 1 | — (3.3V) | Power | MCP23017 VDD, LCD backpack VCC |
| 4 | — (5V) | Power | TFT VCC — the board has its own onboard regulator down to 3.3V; AZ-Delivery's own Raspberry Pi wiring diagram for this exact board feeds it 5V here |
| 6 / 9 / 14 / 20 / 25 / 30 / 34 / 39 | — (GND) | Ground | Common ground, all peripherals |
| 16 | GPIO23 | TFT RS / DC | ST7735 — command/data select |
| 18 | GPIO24 | TFT RES | ST7735 — hard reset |
| 19 | GPIO10 (MOSI) | TFT SDA (SPI data) | ST7735 — SPI0 |
| 23 | GPIO11 (SCLK) | TFT SCK (SPI clock) | ST7735 — SPI0 |
| 24 | GPIO8 (CE0) | TFT CS (chip select) | ST7735 — SPI0 |
| 32 | GPIO12 (PWM0) | TFT LEDA (backlight) | ST7735 — PWM, for the config file's brightness setting |
| 13 | GPIO27 | MCP23017 interrupt | MCP23017's combined interrupt pin (see below) |

`GPIO9` (MISO, pin 21) comes bundled with enabling SPI0 but the TFT never drives it — leave unconnected. `GPIO7` (CE1, pin 26) and `GPIO25` (pin 22) are free if anything else needs them later.

These TFT pin choices (RS→GPIO23, RES→GPIO24) and the VCC→5V decision come directly from AZ-Delivery's own manual for this exact board, not an arbitrary pick — worth keeping if you ever cross-reference their diagram.

## The shared I2C1 bus

Three devices, three addresses, no collisions:

| Device | Address |
|---|---|
| WM8804 (on the HAT itself) | `0x3B` |
| MCP23017 | `0x20` (all three address pins A0/A1/A2 tied to GND) |
| LCD I2C adapter | `0x27` (confirmed — AZ-Delivery's own manual documents this adapter as PCF8574-based, factory-set to `0x27` with all address pads open; the A0/A1/A2 solder pads on the adapter can move it anywhere from `0x20`–`0x27` if that address is ever needed for something else) |

## MCP23017 wiring

The specific board is a **CJMCU-2317** breakout, per AZ-Delivery's own manual — its silkscreen doubles up pin names for the SPI variant of this chip family (`SDA/SI`, `SCL/SCK`), and it exposes only **one combined interrupt pin** (labelled `ITB/ITA`) rather than separate INTA/INTB — convenient, since only one interrupt line is needed here anyway.

- **VDD** → 3.3V, **VSS (GND)** → GND.
- **A0, A1, A2** → GND (gives address `0x20`).
- **RESET** → 3.3V through a **10kΩ pull-up resistor**, not a direct tie — this is AZ-Delivery's own documented reference wiring for this board, safer than hard-wiring it.
- **ITB/ITA** (the combined interrupt pin) → GPIO27 (pin 13).
- **GPA0–GPA7** (labelled `B0/A0`–`B7/A7` on this board's silkscreen — confusingly, "A0–A7" here means GPIO port A bits 0–7, unrelated to the I2C address pins of the same name) → the 8 buttons (Previous, Stop, Pause, Play, Next, Eject, Shuffle, Repeat), each with `GPINTEN` enabled so a press raises the interrupt pin.
- **GPB0–GPB1** (labelled `B1/A1`, `B2/A2` etc. — port B) → the 2 LEDs (Shuffle, Repeat). Add a current-limiting resistor per LED as usual.

## ST7735 TFT wiring

This specific AZ-Delivery board has an onboard 3.3V regulator, so per their own tested Raspberry Pi wiring diagram: **VCC** → 5V, **LEDA** → GPIO12 (PWM, for brightness — direct to 3.3V also works if dimming isn't needed, but never to 5V, which can damage the screen), **GND** → GND, **SCK** → GPIO11, **SDA** → GPIO10, **CS** → GPIO8, **RES** → GPIO24, **RS** (labelled "REG. SEL." in their manual) → GPIO23. All the logic pins are 3.3V-only (the display isn't 5V-tolerant) — fine directly off a Pi's GPIO, no level shifter needed, since the Pi's logic is already 3.3V (AZ-Delivery's own guide only calls for a level converter on 5V-logic boards like an Arduino Uno).

## LCD backpack wiring

**GND/VCC/SDA/SCL** are the only four pins the I2C adapter exposes — SDA/SCL go on the shared I2C1 bus above. AZ-Delivery's datasheet states this adapter's operating range is 3.3V–5V, so running it at 3.3V (required here, since it shares a bus with the MCP23017 and the WM8804, both fixed at the Pi's native 3.3V logic level) is officially within spec, not a workaround. Note the adapter's backlight is a physical on/off jumper and its contrast a physical potentiometer — neither is software-controllable, so the config file's brightness setting only ever applies to the TFT, not this LCD.

## Verify before you solder

One thing below has no authoritative source and needs a real check on your actual board — everything else in this document does have one (cited in `.tracker/controller-v1/research/`, or the AZ-Delivery manuals referenced above).

1. **The clone HAT's exact pin usage is inferred, not confirmed.** Everything in the first table comes from the *genuine* HiFiBerry Digi+ Pro's overlay source, on the reasoning that this clone uses the same WM8804 chip and is driven by the same `hifiberry-digi-pro` overlay (a Raspberry Pi forum thread on this exact card supports that). But nobody has published this clone's own schematic, so it's not a certainty — and the IR receiver's GPIO26 is a weaker version of the same problem: it's a plausible convention pulled from the same product family, not a verified fact. **Before wiring anything to GPIO26, or trusting the HAT's pin list, run `raspi-gpio funcs` / `i2cdetect -y 1` with the HAT connected and nothing else attached**, and confirm the WM8804 shows up at `0x3B` and nothing unexpected is toggling on GPIO26.

(A Raspberry Pi 4B board-revision compatibility issue was found and ruled the Pi 4 out entirely — see [ADR 0005](adr/0005-target-board-pi-3b.md) — which is why this doc no longer targets it.)
