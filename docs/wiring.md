# Hardware Wiring

This is the 40-pin header allocation for all of WinampDeck's hardware, targeting the **Raspberry Pi 3 Model B** ([ADR 0005](adr/0005-target-board-pi-3b.md)): the Digi Pro HAT (a Chinese clone, sold as "AOIDE Digi Pro"/"DollaTek HiFi Digi Pro"), the ST7735 TFT, the MCP23017 button/LED expander, and the HD44780 LCD's FC-113 (PCF8574) backpack, behind a TXS0108E logic level converter. The 40-pin layout itself is identical across the whole 2B/3B/3B+/4B family, so none of this changed when the target board did.

Everything here is built from primary sources — Raspberry Pi's own GPIO header documentation, the actual `hifiberry-digi-pro` device-tree overlay source, Microchip's MCP23017 datasheet, and the manufacturer's own manuals for the specific TFT, LCD/I2C-adapter, and MCP23017 (CJMCU-2317) breakout boards actually being used, purchased from AZ-Delivery (kept locally in `docs/`, not committed — see below) — kept in `.tracker/controller-v1/research/` for anyone who wants to check the sourcing. One thing could **not** be verified from any authoritative source and needed a real-hardware check before committing to soldering; it's called out, along with the check's result, in [Verify before you solder](#verify-before-you-solder).

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

Note the I2C1 bus (pins 3/5) is *shared*, not exclusive — the MCP23017 goes directly on this same bus, at its own address, alongside the WM8804. That's normal I2C; it's not a conflict. The LCD's I2C adapter also lives logically on this bus, but not electrically directly on it — see [LCD backpack wiring](#lcd-backpack-wiring) below; it needs a level shifter in between.

One more pin to steer clear of, for a different reason: this specific clone board has a **built-in IR receiver** the genuine HiFiBerry doesn't have. No schematic exists for this exact product, but the closest thing found — a build script from the same AOIDE product family — wires its IR receiver to **GPIO26** (physical pin 37). Treat that as a caution, not a confirmed fact (see below), and avoid assigning anything of ours there.

## Everything else: our assignments

| Physical pin | BCM GPIO | Signal | Goes to |
|---|---|---|---|
| 1 | — (3.3V) | Power | MCP23017 VDD; level shifter VA + OE (OE via 10kΩ pull-up resistor — see [LCD backpack wiring](#lcd-backpack-wiring)) |
| 2 | — (5V) | Power | Level shifter VB (its high-voltage side, feeding the LCD adapter) |
| 4 | — (5V) | Power | TFT VCC — the board has its own onboard regulator down to 3.3V; AZ-Delivery's own Raspberry Pi wiring diagram for this exact board feeds it 5V here |
| 6 / 9 / 14 / 20 / 25 / 30 / 34 / 39 | — (GND) | Ground | Common ground, all peripherals (including the level shifter) |
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

**Software prerequisite, confirmed on real hardware (2026-09-22):** the I2C1 controller itself comes up automatically with the HAT overlay (needed for the WM8804), but `/dev/i2c-1` for userspace tools/pigpio does not exist until I2C is explicitly enabled. In `/boot/firmware/config.txt`, uncomment `dtparam=i2c_arm=on`, and make sure the `i2c-dev` kernel module is loaded (`echo i2c-dev | sudo tee -a /etc/modules` to persist it — on this image it wasn't auto-loaded by the `dtparam` alone). Confirmed working: `i2cdetect -y 1` shows the WM8804 at `0x3B` (as `UU`, meaning a kernel driver already owns that address — expected, not an error).

## MCP23017 wiring

The specific board is a **CJMCU-2317** breakout, per AZ-Delivery's own manual — its silkscreen doubles up pin names for the SPI variant of this chip family (`SDA/SI`, `SCL/SCK`), and it exposes only **one combined interrupt pin** (labelled `ITB/ITA`) rather than separate INTA/INTB — convenient, since only one interrupt line is needed here anyway.

| MCP23017 pin | Connects to | Note |
|---|---|---|
| VDD | 3.3V | |
| VSS | GND | |
| SDA/SI | GPIO2 / SDA1 (pin 3) | I2C1 data — same shared bus as the WM8804, in parallel with the level shifter's low-voltage side (see [LCD backpack wiring](#lcd-backpack-wiring)) |
| SCL/SCK | GPIO3 / SCL1 (pin 5) | I2C1 clock — same shared bus |
| A0, A1, A2 | GND | gives address `0x20` |
| RESET | 3.3V, through a **10kΩ pull-up resistor** | not a direct tie — AZ-Delivery's own documented reference wiring, safer than hard-wiring it |
| ITB/ITA (combined interrupt pin) | GPIO27 (pin 13) | |
| GPA0–GPA7 (silkscreen `B0/A0`–`B7/A7`) | The 8 buttons: Previous, Stop, Pause, Play, Next, Eject, Shuffle, Repeat | each with `GPINTEN` enabled so a press raises the interrupt pin. Note the silkscreen's "A0–A7" here means GPIO port A bits 0–7 — unrelated to the I2C address pins of the same name |
| GPB0–GPB1 (silkscreen `B1/A1`, `B2/A2`) | The 2 LEDs: Shuffle, Repeat, each through a **220Ω current-limiting resistor** to the LED anode (cathode to GND) | 220Ω assumes a typical ~2V-Vf indicator LED (red/yellow/green) driven from the MCP23017's 3.3V output, targeting ~6mA — a conservative, not-too-dim value. Not sourced from AZ-Delivery's manual (it doesn't specify LED part/resistor); if your LEDs are blue/white (~3.0–3.2V Vf), drop to ~100Ω for similar brightness, or check the LED's own datasheet if you have one |
| GPB2–GPB7 | unused | not connected — only 2 of the 8 GPB lines are wired |

## ST7735 TFT wiring

This specific AZ-Delivery board has an onboard 3.3V regulator, so per their own tested Raspberry Pi wiring diagram, VCC is fed 5V while every logic pin stays 3.3V — the display itself isn't 5V-tolerant, and AZ-Delivery's guide only calls for a level converter on 5V-logic boards like an Arduino Uno. Since the Pi's GPIO is already native 3.3V, none is needed here.

| TFT pin | Connects to | Note |
|---|---|---|
| VCC | 5V (pin 4) | onboard regulator steps this down to 3.3V for the chip |
| GND | GND | |
| SCK | GPIO11 / SCLK (pin 23) | SPI0 clock |
| SDA | GPIO10 / MOSI (pin 19) | SPI0 data |
| CS | GPIO8 / CE0 (pin 24) | SPI0 chip select |
| RES | GPIO24 (pin 18) | hard reset |
| RS (labelled "REG. SEL." in AZ-Delivery's manual) | GPIO23 (pin 16) | command/data select |
| LEDA | GPIO12 / PWM0 (pin 32) | backlight — PWM for the config file's brightness setting; a direct 3.3V tie also works if dimming isn't needed, but never 5V — that can damage the screen |

## LCD backpack wiring

**Correction from an earlier version of this doc**: the LCD's I2C adapter is **not** 3.3V-capable — AZ-Delivery's own manual states plainly that "the I2C adapter only works in the 5V range," and their tested Raspberry Pi wiring puts a **TXS0108E logic level converter** between the Pi and the adapter. Their manual's reasoning applies here just as much as it does to their own reference build: the adapter's SDA/SCL pull-ups are referenced to 5V, and driving them from the Pi's 3.3V-only GPIO directly is exactly the unsafe combination they call out — it's not a coincidence we should route around, it's the vendor telling us how this specific part actually works.

The fix doesn't cost any extra Pi pins, since the level shifter's low-voltage side just taps the same GPIO2/GPIO3 pins the MCP23017 is already on. Wiring, per AZ-Delivery's own tested diagram:

| I2C adapter pin | Level shifter pin |
|---|---|
| SCL | B1 |
| SDA | B2 |
| VCC | VB |
| GND | GND |

| Level shifter pin | Raspberry Pi pin |
|---|---|
| VA | 3.3V (pin 1) |
| A1 | GPIO3 / SCL1 (pin 5) |
| A2 | GPIO2 / SDA1 (pin 3) |
| OE | 3.3V (pin 1), through a **10kΩ pull-up resistor** |
| GND | GND (pin 20) |
| VB | 5V (pin 2) |
| A3–A8, B3–B8 | unused — not connected, only 2 of the TXS0108E's 8 channels are wired |

The 10kΩ on OE isn't from AZ-Delivery's manual (this doc's earlier version left the value out because it wasn't captured from there) — it's TI's own TXS0108E datasheet recommendation: tying OE to VCCA through a resistor rather than hard-wiring it, so the chip's auto-direction-sensing power-up sequence completes before the outputs enable. If you have AZ-Delivery's manual in hand and it specifies a different value, prefer that instead.

The MCP23017 stays wired directly to GPIO2/GPIO3 as before, in parallel with the level shifter's low-voltage side — it's the LCD adapter specifically that sits behind the shifter, not the whole bus.

Note the adapter's backlight is a physical on/off jumper and its contrast a physical potentiometer — neither is software-controllable, so the config file's brightness setting only ever applies to the TFT, not this LCD.

## Verify before you solder

One thing below had no authoritative source and needed a real check on the actual board — everything else in this document does have one (cited in `.tracker/controller-v1/research/`, or the AZ-Delivery manuals referenced above). That check has since passed, per the result recorded below.

1. **The clone HAT's exact pin usage is inferred, not confirmed.** Everything in the first table comes from the *genuine* HiFiBerry Digi+ Pro's overlay source, on the reasoning that this clone uses the same WM8804 chip and is driven by the same `hifiberry-digi-pro` overlay (a Raspberry Pi forum thread on this exact card supports that). But nobody has published this clone's own schematic, so it's not a certainty — and the IR receiver's GPIO26 is a weaker version of the same problem: it's a plausible convention pulled from the same product family, not a verified fact. **Before wiring anything to GPIO26, or trusting the HAT's pin list, run `raspi-gpio funcs` / `i2cdetect -y 1` with the HAT connected and nothing else attached**, and confirm the WM8804 shows up at `0x3B` and nothing unexpected is toggling on GPIO26.

   **Checked on real hardware (2026-09-22):** passed. `i2cdetect -y 1` shows `UU` at `0x3B` (a kernel driver already owns that address — the `snd_soc_wm8804_i2c` driver, confirmed bound and working via a successful `speaker-test`), confirming the WM8804 is exactly where the genuine HiFiBerry's overlay says it should be. `pinctrl` (the current replacement for `raspi-gpio` on this OS image) shows GPIO26 as plain `input`, `hi`, with no alt-function claimed by any overlay — nothing is actively driving or claiming it. This doesn't rule out the IR receiver being physically present (an idle receiver output also idles high), but confirms nothing software-side is fighting you there. Safe to proceed with wiring the MCP23017/TFT/LCD per the tables above.

(A Raspberry Pi 4B board-revision compatibility issue was found and ruled the Pi 4 out entirely — see [ADR 0005](adr/0005-target-board-pi-3b.md) — which is why this doc no longer targets it.)
