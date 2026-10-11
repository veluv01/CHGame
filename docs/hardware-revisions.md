<!-- Modified 2026-10-11: WeAct F8U6 hardware variant; see docs/weact-f8u6.md. -->
# Hardware revisions

What to do when the CHGame handheld gets a new board: whether it needs a
new device, what it is called, what already keeps a game or a bootloader
off the wrong board, and the checklist for adding one. The rules
themselves (names, target ids, what readers do) are normative in
[spec/chgame.md, "Devices and revisions"](../spec/chgame.md#devices-and-revisions);
this page is the working guide.

## The registry

| Device | Board | Target id | State |
|---|---|---|---|
| `rev0` | CHGame Rev0, CH32X035G8U6 | `CX35` (`0x35335843`) | released; named after its MCU before boards had ids, kept for good |
| `weact-f8u6` | WeAct CH32X035F8U6 | `WF86` (`0x36384657`) | supported fork variant; [wiring and use](weact-f8u6.md) |
| `rev1` | CHGame Rev1 | `CGR1` (`0x31524743`) | **reserved** (2026-10-07): pins to change for compatibility; nothing else settled |
| `rev2` ... `rev9` | | `CGR2` ... `CGR9` | free, in that order |
| `rev10` ... `rev99` | | `CG10` ... `CG99` | free |

The table in spec/chgame.md is the authority: register a board there first.
These stay in step with it (chcart's and the uploaders' tests check):
`tools/chcart/model.py` `DEVICES` / `RESERVED_DEVICES`, `tools/chgpack.py`
`BOARDS` / `RESERVED`, `platform/bootloader/shared/chg_format.h`
`CHG_TARGET_*`, the uploaders' `chg.go` / `chg.py`.

## Does the new board need a new device?

Yes when a binary built for the board before might not run correctly on
it. Typical reasons:

- a pin moved: the screen (CS, DC, RST, SPI), the SD card's CS, a button,
  the buzzer, the LED;
- another MCU, clock or crystal;
- another flash or RAM map, or another bootloader reservation;
- another screen or panel controller.

No when every existing binary runs on it unchanged: another PCB colour,
battery, case or connector, or a part swapped for an equivalent on the same
pins. Such a board keeps its device (it is still `rev0`) and needs nothing
here.

When in doubt, make it a new device. A needless one costs a rebuild per
game. A missing one means games that run but misbehave, with nothing to say
why.

## Names

- **`rev<n>`**: lowercase, the number in decimal, no leading zeros. The same
  name is used everywhere:

  | Where | rev1 |
  |---|---|
  | the `.chgame` device | `rev1` |
  | the board package's FQBN | `CHGame:ch32v:rev1` |
  | the IDE's board menu | CHGame Rev1 |
  | the variant | `variants/CH32X035/CHGame_Rev1` |
  | the board macro | `ARDUINO_CHGAME_REV1` |
  | the target id (CHG files, `HELLO`, bootloader image) | `CGR1`, `0x31524743` |
  | the bootloader build | `CHBOOT_BOARD_TARGET=0x31524743` (until `build.sh` gets `--board=rev1`) |

- **Numbers go up in release order and are never given twice.** A revision
  abandoned before release keeps its number, unused. Board-level fixes that
  keep binaries compatible do not take a number.
- **Target ids:** `CGR<n>` for rev1-rev9, `CG<nn>` for rev10-rev99. They
  are four ASCII bytes read as a little-endian u32: `CGR1` is
  `0x31524743` (bytes `43 47 52 31`).
- **A board that is not a handheld revision** (another product, a
  development board) gets a name of its own (`a-z`, `0-9`, `-`, starting
  with a letter) and four characters of its own (letters and digits, not
  starting `CG` or `CX`, not one of the formats' magic numbers).
- **Never** reuse a name or an id, give one to another board, or change
  rev0's `CX35`. Do not put the board in a game's title, a cart's title or
  a file name (`CHGAME-REV0`): the device says it, and every tool reads the
  device.

## What already keeps things on the right board

Done 2026-10-07, ahead of rev1, so that the boards already in use stay as
they are and the tools that read carts today keep working once a rev1
exists:

| Path | What happens with the wrong board |
|---|---|
| **A `.chgame` cart** | Each binary names its device; a reader picks by name and never falls back to another board's. A reader made before a board exists warns (`unknown-device`), ignores that binary, keeps it when it rewrites the cart, and uses the rest. |
| **The SD menu** | A CHG file carries its board's target id. A bootloader installs only its own board's: another board's files show greyed under their 8.3 names, and starting one shows ERROR 5, with nothing erased. A card is prepared for one board (`chgame cart prepare --device`). |
| **A sketch over USB** | `chgame-upload flash -device rev0` (and `chgame cart flash` / `deploy`) refuse a board whose `HELLO` names another device. A rev0 bootloader sends no board field, which means rev0. Arduino Upload and the CHG post-build hook pass the selected board's `-device`. Upload then refuses a different board; Rev0 remains the default. |
| **A bootloader over USB** (`selfupdate`, Burn Bootloader with *CHGame USB*) | Refused when the image's board word (offset 0x14; 0 means rev0) names another board than the one running. |
| **A bootloader through the factory ISP** | **Not checked**: the chip's ROM knows nothing of boards. A rev0 bootloader on a rev1 board leaves it dark (the bootloader drives the wrong pins), and the ISP with the right bootloader brings it back ([recovery](../platform/board/docs/recovery.md)). The IDE's board menu is the only guard. |
| **The web emulator** | Emulates rev0 and takes `rev0` binaries (spec/README.md, "Board revisions"). |

In this fork the board package, shared input/audio paths, bootloader and
repository build/export/card tools support both `rev0` and `weact-f8u6`.
Rev0 remains the default. The reserved `rev1` is not a supported build target
until its hardware definition is settled.

## Adding a board: the checklist

In this order. rev1 is the example. Every step is checkable without the new
board except 8.

1. **Register it in the spec.** Fill in the reserved row of spec/chgame.md's
   device table: board name and FQBN, MCU, load address, largest image,
   save-safe size, layout id. If the memory map changes, note it in
   spec/chg.md ("The target") too. Tell the web emulator project
   (spec/README.md).
2. **The reference tools.** Move `rev1` from `RESERVED_DEVICES` to
   `DEVICES` in `tools/chcart/model.py`, and from `RESERVED` to `BOARDS` in
   `tools/chgpack.py`, with the same values. `--device rev1` then works for
   `chgame cart prepare`, `flash` and `deploy`, and for `chgpack.py pack`.
   Run `python tools/chcart/fixtures.py`: `good/devices` has a `rev1`
   binary, which now counts as a known device. It is a valid 64 B image, and
   `other-board` keeps the `unknown-device` warning, so the expected results
   should not change. Review the diff, then run chcart's tests.
3. **The uploaders.** Move `rev1` into `chgDevices` (`host/go/chg.go`) and
   `DEVICES` (`host/py/chgame_upload/chg.py`). Regenerate the shared vectors
   (`python -m chgame_upload.vectors --write ...`, see its docstring) and run
   both suites. **Bump the uploader's version** (`host/go/main.go` and
   `host/py/chgame_upload/__init__.py`, which must agree): step 4 makes
   `platform.txt` pass `-device`, which older uploaders do not know, and
   Arduino keeps an installed tool whose version has not changed.
4. **The board package** (`platform/board`; it takes effect at the next
   release, see CLAUDE.md rule 6):
   - `boards.txt`: a `rev1.*` entry, "CHGame Rev1", `build.variant=CH32X035/CHGame_Rev1`,
     `build.board=CHGAME_REV1`, its own bootloader files in the Bootloader
     menu, and `rev1.build.chgame_device=rev1`;
   - `platform.txt`: a default `build.chgame_device=rev0` (so a board entry
     without one, rev0's included, still builds), and `-device
     {build.chgame_device}` on the `pack` hook and the `flash` upload
     recipe. From then on, Upload with the wrong board selected is refused;
   - the variant `variants/CH32X035/CHGame_Rev1/`, a copy of Rev0's with the
     new pins (`PIN_LCD_*`, `PIN_SD_CS`, `PIN_BTN_*`, `PIN_BUZZER`,
     `PIN_LED`);
   - the CHANGELOG.
5. **The libraries' pins.** CHGfx (`CHGFX_*_PORT/PIN` in `CHGfx.h`), CHSd
   (`SdSpi.cpp`), the CHGame library's `Input.cpp` and `Audio.cpp`, and the
   core's START-hold in `pollButtons()` address GPIO ports directly. Take
   them from the variant (or `#if defined(ARDUINO_CHGAME_REV1)`) so one
   source builds for both boards. Rebuild all 20 games for rev0 and check
   that their sizes and sim frames are unchanged (rules 2, 3 and 6).
6. **The bootloader.** Its pins are in `src/hal.h` (the `HAL_GPIO*_CFG*`
   words and the button bits). Give `build.sh` a `--board=rev1` that selects
   them and sets `CHBOOT_BOARD_TARGET` to rev1's id. The board word in the
   image and `HELLO`'s board field come with that switch (16 B, measured on
   2026-10-07: 11,936 B list, 12,076 B visual). `dist.sh` builds each board's
   files under their own names, and SIZES.md records them. The PC suite
   already builds `core_rev1` (`HELLO` with the board field, CHG files for
   the other board refused). Rev0's binaries must stay byte-identical.
7. **The build tools.** `tools/device.py` (its FQBN), `chgame build`,
   `export`, `card` and `gif` build for rev0. Give them a device option, and
   make `chgame export` put one binary per device in the cart
   (`sources.from_sketch(d, image, device)`), so one `.chgame` serves both
   boards. The casino card (`chgame card`) is then prepared per board.
8. **On the boards.** Upload with the wrong board selected and check that it
   is refused; install a card for each board on the other and check for
   ERROR 5; burn the other board's bootloader over USB and check that it is
   refused.
9. **The docs.** This registry, CLAUDE.md (Limits, the FQBN), the README's
   board section, `platform/README.md`.

## Why not a target name in the title

The question came up for rev0: call games `CHGAME-REV0` and leave the
format alone. A name does not stop anything. The bootloader would install
any file whatever its title, an uploader would flash any image, and a cart
could not carry both builds of a game. The device field already gives each
binary a board. The target id puts the same fact where the hardware and the
uploaders check it.
