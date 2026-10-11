# CHGame on WeAct CH32X035F8U6

This fork supports a **WeAct CH32X035F8U6 Core Board V1.0** with an ST7735S
128 × 128 TFT, an SPI microSD adapter and eight buttons. The variant shares
CHGame's core, libraries, games and bootloader with Rev0. It omits the
external piezo buzzer and status LED and preserves WCH SDI debugging.

| Property | F8U6 variant |
|---|---|
| Device / Arduino FQBN | `weact-f8u6` / `CHGame:ch32v:weact-f8u6` |
| MCU | CH32X035F8U6, QFN20, 48 MHz |
| Target identity | `WF86` / `0x36384657` |
| Logic and TFT supply | 3.3 V |
| Flash / SRAM | 62 KB / 20 KB |
| Application origin | `0x3000` |
| Image limit / two-page save limit | 50,944 / 50,432 bytes |
| Static RAM / stack / retained block | 18,416 / 2,048 / 16 bytes |
| Development | [PlatformIO guide](../ports/weact-f8u6/README.md) |

This is a development-board variant maintained in the fork; the upstream
CHGame Boards Manager release does not include it. Rev0 remains the
default for the repository's Arduino/cart commands. The reserved
`rev1` / `CGR1` identity is unchanged. Rev0 and WF86 firmware have different
pin assignments and must be built for their respective targets.

## Wiring

Header references below follow the WeAct **V1.0 F8U6 board**. They do not
describe the QFN28 G8U6 handheld. Connect each button between its signal
and GND; the firmware enables internal pull-ups.

| Function | MCU signal | WeAct header |
|---|---|---|
| UP | PA0 | P1.4 |
| B | PA1 | P1.5 |
| START | PA2 | P1.6 |
| SELECT | PA3 | P1.7 |
| TFT CS | PA4 | P1.8 |
| TFT + SD SCK | PA5 | P1.9 |
| SD MISO | PA6 | P1.10 |
| TFT + SD MOSI | PA7 | P1.11 |
| TFT DC | PB0 | P2.10 |
| A | PB1 | P2.9 |
| LEFT | PB3 | P2.8 |
| SD CS | PB11 | P2.7 |
| TFT RESET | PB12 | P2.6 |
| DOWN | PC14 | P2.5 |
| RIGHT | PC15 | P2.4 |
| TFT VCC | VDD33, 3.3 V | P2.1 or P2.2 |
| SD module VCC — regulated 5 V-input module | USB VBUS, 5 V | P1.1 |
| GND — all modules and buttons | GND | P1.3, P1.12, P2.3 or P2.12 |
| SDI clock — reserved | PC19 | P3.3 |
| SDI data — reserved | PC18 | P3.4 |

P3.1 is 3.3 V and P3.2 is GND for the debug connector. PC16/PC17 retain the
board's USB D−/D+ connections. There are no unallocated signal pins with
all eight buttons and SDI retained.

The TFT and SD share SCK/MOSI but use separate chip selects. TFT MISO is
not required. Keep breadboard SPI wires short and place the module grounds
on the same ground rail. Power the TFT backlight according to its module's
resistor/driver requirements; no GPIO is allocated to backlight control.

### USB-C links

Open **SB2 — PC14 to CC1** and **SB3 — PC15 to CC2** before using DOWN and
RIGHT. These links use **0 Ω parts**. Preserve **R4/R5**, the connector-side
**5.1 kΩ USB-C CC pulldowns**. If R4/R5 were removed, restore them while
leaving SB2/SB3 open. The pulldowns and the removable links serve different
functions.

### Power

The MCU and SPI signals use **3.3 V logic**. The TFT supply is 3.3 V.
A microSD module containing an **AMS1117-3.3 regulator and LVC125 buffers**
normally needs **5 V at VCC**, from USB VBUS/P1.1. Supplying 3.3 V through
its regulator can leave the card undervoltage. A bare 3.3 V adapter instead
needs 3.3 V. Confirm the adapter's markings/schematic before wiring VCC.

P1.1 is USB VBUS, P1.2 is the board's VCC input rail, and P2.1/P2.2 are
regulated 3.3 V. The 5 V module connection assumes normal USB power.
The SD card communicates over SPI; it does not require USB host support.
CHSDtoUSB presents the card to a PC through USB device mode.

### Onboard LED and debug connector

The WeAct onboard LED is on **PB12**, also used for TFT RESET. PB12 is driven
only for display reset. `LED_BUILTIN`, `PIN_LED` and `PIN_BUZZER` are
undefined in the variant, and CHGame's LED/audio functions do not drive
GPIOs. The onboard LED may respond to reset activity; removing an external
status LED does not remove this board component.

`CH32V_LOCK_DEBUG` prevents automatic remapping of PC18/PC19. Use a
compatible WCH SDI programmer; the pins are not assigned to UART, I2C or
console controls in this variant. PlatformIO does not configure a debug
server.

## Behavior and compatibility

The game and bootloader input paths map the eight physical controls to
CHGame's existing logical button bits. START held for three seconds keeps
its exit-to-menu behavior. CHGfx and CHSd use the shared SPI pin mapping.

The public audio API stays callable. `audio::begin()`, effects, notes,
music and LED patterns do nothing on F8U6; `audio::on()`, `playing()` and
`musicPlaying()` return false. No sound timer is configured. Rev0 retains
its existing sound and status LED implementation.

WF86 is registered consistently in the cart tools, CHG packers, Python/Go
uploaders and bootloader. USB uploads and SD installs check target
identity. Factory ISP has no board-identity guard and requires the correct
bootloader image to be selected.

## First use

From the repository root:

```sh
python -m pip install -r ports/weact-f8u6/requirements.txt
python ports/weact-f8u6/check.py
pio run -e bringup
```

An unprovisioned board needs a WeAct bootloader installed first. Follow
[initial bootloader installation](../ports/weact-f8u6/README.md#initial-bootloader-installation-and-recovery);
factory ISP provisioning erases the installed application and saves.
After provisioning, upload `bringup`, check all eight controls and the
display, then test the SD menu and applications.

Read the [build verification](weact-f8u6-builds.md) for compiler results and
their limits. Host/build tests establish software behavior; physical
display, SD and USB operation require checks on the assembled hardware.
For updates, use the [maintenance guide](weact-f8u6-maintenance.md).

## Hardware references

- [WeAct CH32X035 Core Board schematics and pinouts](https://github.com/WeActStudio/WeActStudio.CH32X035CoreBoard)
- [WCH CH32X035 documentation](https://www.wch-ic.com/products/CH32X035.html)
- [CHGame hardware revision registry](hardware-revisions.md)
