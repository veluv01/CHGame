# PlatformIO support for WeAct F8U6

Build CHGame games, applications and bootloaders for the **WeAct
CH32X035F8U6 Core Board V1.0**. The project compiles the core, libraries and
bootloader from the current repository checkout; it does not maintain
separate copies of the games or Arduino framework.
The local platform uses a unique project alias so multiple checkouts do
not share a stale PlatformIO platform link.

| Start here | Reference |
|---|---|
| Hardware and connections | [F8U6 wiring guide](../../docs/weact-f8u6.md) |
| Compiler results | [Build verification](../../docs/weact-f8u6-builds.md) |
| Upstream merges and release packaging | [Maintenance guide](../../docs/weact-f8u6-maintenance.md) |

## Quick start

Open the **repository root** in VS Code with the PlatformIO extension.
Select `bringup` for the display/button check, `hello` for the smallest
example, or `game` for CHFour. The standalone `ports/weact-f8u6` project
remains available and uses the same environments.

Requirements: Python 3.10 or newer, PlatformIO Core 6.x, Git and network
access for the first compiler installation. The validated Core version is
6.1.19. From the repository root, install the pinned build tools:

```sh
python -m pip install -r ports/weact-f8u6/requirements.txt
python ports/weact-f8u6/check.py
pio run -e bringup
pio run -e hello
pio run -e game
```

Use the Python interpreter that runs PlatformIO. For the VS Code-managed
environment, use its terminal or its interpreter under
`~/.platformio/penv/bin/python` on Linux/macOS and
`%USERPROFILE%\.platformio\penv\Scripts\python.exe` on Windows. `pyserial`
must be installed in that environment for uploads.

The local platform selects pinned WCH GCC 8.2.0 packages for Linux x64,
Windows x86/x64 and macOS. Only Linux x64 is covered by the recorded build
verification. The macOS package includes Intel executables; Apple Silicon
may require Rosetta. Linux ARM hosts need a compatible toolchain port.

## Build environments

All commands below run from the repository root. Add
`-d ports/weact-f8u6` to use the standalone project.

| Environment | Source | Configuration |
|---|---|---|
| `bringup` — default | `ports/weact-f8u6/src/main.cpp` | LCD and eight buttons, USB Serial, 12 MHz display SPI |
| `hello` | Shared `Hello` example | Minimal CHGame sketch |
| `game` | Shared `CHFour` game | Release; another sketch can be selected |
| `game-debug` | Same game selection | USB Serial and `CHGAME_DEBUG=1`; exports to a separate debug directory |
| `sdreader` | `CHSDtoUSB` | USB mass storage with serial, plain `-Os` |
| `sdserial` | `CHSDtoSerial` | SD access over USB Serial; retains `GFX_CHUNK_ROWS=1` |
| `stlviewer` | `CHStlView` | SD-backed 3D model viewer |
| `bootloader` | Shared bootloader | SD text menu, rainbow colours |
| `bootloader-visual` | Shared bootloader | SD graphic menu, rainbow colours |
| `bootloader-usb` | Shared bootloader | USB only |
| `bootloader-static` | Shared bootloader | SD text menu, static colours |
| `bootloader-visual-static` | Shared bootloader | SD graphic menu, static colours |

`game-debug` enables CHGame's serial inspection protocol. Some games omit
features in debug builds to fit memory. This is separate from source-level
SDI debugging; no PlatformIO debug server is configured.

## Application uploads

A compatible **WF86 bootloader** must already be installed. Connect USB,
close other serial monitors, then run:

```sh
pio run -e bringup -t upload
pio run -e game -t upload
pio device monitor -b 115200
```

The shared CHGame Python host performs the 1200-baud bootloader transition,
checks the target, uploads, verifies and starts the application. If more
than one board is attached, specify the port:

```sh
pio run -e game -t upload --upload-port COM5
# Linux example: --upload-port /dev/ttyACM0
```

The uploader rejects a Rev0 bootloader when the selected target is
`weact-f8u6`. Target identity is stored in the firmware/package rather than
in a game's title.

## Select or develop a game

Select any shared game by name without editing the configuration:

```sh
# Linux/macOS
CHGAME_SKETCH=CHChess pio run -e game

# PowerShell
$env:CHGAME_SKETCH = "CHChess"
pio run -e game
Remove-Item Env:CHGAME_SKETCH
```

Alternatively, add an environment to the root `platformio.ini`:

```ini
[env:my-game]
extends = env:game
custom_chgame_sketch = games/MyGame
```

Use a sketch directory containing `MyGame.ino`, with supporting sources
beside it or under `src/`. Repository-relative and absolute sketch paths
are supported. Start with the shared
[Hello example](../../platform/board/arduino/CHGame/libraries/CHGame/examples/Hello)
or follow [Getting started](../../docs/getting-started.md).

The builder reads the selected sketch's `tools/game.py` for `FQBN` menu
options, `DEFINES` and `SAVE_PAGES`. It substitutes the F8U6 board while
retaining the sketch's required settings. Applications use the existing
`CHGame.h`, CHGfx and CHSd APIs. Sound and status-LED calls are harmless
no-ops on this variant.

Optional environment settings:

```ini
custom_chgame_opt = auto        ; oslto, osstd, o1std, o2std, o3std, ogstd
custom_chgame_usb = auto        ; serial or uploadonly
custom_chgame_rtlib = auto      ; nano, nanofp or full
custom_chgame_save_pages = auto ; 0, 1 or 2
custom_chgame_export = yes      ; copy outputs to the selected sketch
build_flags = -DMY_GAME_OPTION=1
```

Keep `usb=serial` when a sketch uses `Serial`. The fixed `periph=game`
configuration reserves the console wiring and SDI pins; full peripheral
pin defaults are incompatible with this hardware layout.

## Outputs and memory limits

| Output | Location |
|---|---|
| Linked image and map | `.pio/build/<environment>/firmware.elf`, `firmware.map` |
| Flash image | `.pio/build/<environment>/firmware.bin` |
| SD-menu installation package | `.pio/build/<environment>/firmware.chg` — applications only |
| Target, settings and size metadata | `.pio/build/<environment>/chgame-build.json` |
| Shared/custom sketch export | `<sketch>/build/weact-f8u6/release/` or `debug/` |

Applications link at **0x3000**, above the 12 KB bootloader. The maximum
image is **50,944 bytes**; reserving two 256-byte save pages reduces it to
**50,432 bytes**. Static RAM is limited to **18,416 bytes**, with a separate
16-byte retained boot block and 2,048-byte stack. Builds fail if these
budgets or the selected game's save reservation are exceeded.

Each sketch has a separate object directory. Switching `CHGAME_SKETCH`
does not reuse a different game's compiled source files. Exported F8U6
files also have separate paths from Rev0 output.

After a PlatformIO release build, create a shared cart or SD-card directory
using the repository tools:

```sh
python -m pip install -e .
python tools/chgame.py --sketch CHFour export --device weact-f8u6 --no-build
# Build every shared game/app first for a complete casino card:
python ports/weact-f8u6/build_all.py
python tools/chgame.py card --device weact-f8u6 --no-build
```

`.chg` is an installable image for the SD menu. `.chgame` is the richer cart
format for distributing games, artwork and data together. See [the format
guide](../../spec/README.md).

## Initial bootloader installation and recovery

Build the desired WeAct bootloader, for example:

```sh
pio run -e bootloader
```

Install [wchisp](https://github.com/ch32-rs/wchisp) separately, or use its
executable from an installed CHGame Arduino tools package. Put it on PATH
or set `CHGAME_WCHISP` to its full path. Hold BOOT while cycling power to
enter the chip's factory ISP, then explicitly upload the bootloader:

```sh
pio run -e bootloader -t upload
```

**Uploading any `bootloader*` environment uses factory ISP and erases code
flash, including the installed application and its saves.** Ordinary
`game`, `hello`, `bringup` and app uploads use CHGame's USB protocol.
The factory ISP cannot check a board's firmware identity; select a WeAct
image. The preserved PC18/PC19 SDI connector can also program the bootloader
ELF with a compatible WCH-Link programmer.

After installation, upload `bringup` to check the LCD and all eight
buttons, then check the SD menu/card. Holding START for three seconds
returns an application to the menu.

## Troubleshooting

| Symptom | Check |
|---|---|
| `No module named serial` | Install the requirements with PlatformIO's interpreter. |
| No CHGame USB port | Check cable/power, close other serial tools, and confirm a WF86 bootloader is installed. Use factory ISP to recover a crashed sketch. |
| Wrong-board refusal | The device is running another target's bootloader; provision the WeAct bootloader. |
| DOWN/RIGHT or USB-C power behaves incorrectly | Verify SB2/SB3 are open and connector-side R4/R5 CC pulldowns remain fitted. |
| SD initialization fails | Check module supply requirements, shared ground, PB11 CS and short SPI wires; use FAT16/FAT32 rather than exFAT. |
| Application exceeds its budget | Use release/LTO settings and retain the required save-page reservation. |
| Source-contract check fails after a merge | Review the reported upstream change using the [maintenance checklist](../../docs/weact-f8u6-maintenance.md). |

Repeat all firmware builds with `python ports/weact-f8u6/build_all.py`.
Add `--smoke` for the twelve named environments, or
`--install-bootloaders` to refresh the five Arduino-package bootloader
binaries. Neither option flashes hardware. Logs, per-game artifacts and
`results.json` are saved under `ports/weact-f8u6/build/verification/`.
