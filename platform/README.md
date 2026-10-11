# platform/

This checkout adds the **WeAct CH32X035F8U6** variant and a local PlatformIO
project using these master sources. The build matrix covers all 20 games,
three apps, bringup, Hello, a debug game and five WeAct bootloaders.
[Wiring/setup](../docs/weact-f8u6.md) and
[build results](../docs/weact-f8u6-builds.md) document the variant.
The WeAct branch omits sound/status LED writes and preserves SDI debug pins;
Rev0 retains its existing pin map and sound APIs.
See [variant maintenance](../docs/weact-f8u6-maintenance.md) for upstream
merges, source checks and Arduino release packaging.

The CHGame platform: the board package, the bootloader, the libraries and
the hardware files. **These are the master copies.** Each piece began in a
repository of its own; those are frozen, and every fix and feature is made
here. The table's "Came from" column is history, not something to sync with.

| Folder | What | Version | Came from | Licence |
|---|---|---|---|---|
| `board/arduino/CHGame/` | The CHGame Arduino board package: core, variant, linker scripts, bootloader binary, `boards.txt` / `platform.txt` | 0.3.0 (released 2026-10-07) | CH32SerialBoot tag `v0.2.4` (5de3006), folder `arduino/CHGame` | MIT (`board/LICENSE`, `board/THIRD-PARTY.md`) |
| `board/docs/` | The board's docs: hardware pin map, flash/RAM map, boot flow, upload protocol, recovery, CH32X035 gotchas, building the bootloader | 0.2.4 | same tag, folder `docs` | MIT |
| `board/arduino/CHGame/libraries/CHGame/` | The CHGame library: `CHGame.h`, the one include of a sketch (buttons, pacing, palette, drawing, sound, saving, the debug protocol) | 0.1.0 | built here (2026-10-02) from the code the twenty games shared | Apache-2.0 (`LICENSE`, `NOTICE`) |
| `board/arduino/CHGame/libraries/CHGfx/` | The graphics library | 1.3.2 | CHGfx tag `1.3.0` (838bbb0) | MIT (+ font notices in its `LICENSE`) |
| `board/arduino/CHGame/libraries/CHSd/` | Read-only SD card + FAT16/32 library | 1.0.0 | never had a repository of its own | MIT |
| `bootloader/` | The bootloader with the SD game menu: sources, PC test suite, built binaries, and the uploader in Go (`host/go`, the executable the board package installs) and in Python (`host/py`, what the repository's tools use) | 0.2.4 + the SD menu (BOOT_VERSION 2) | CH32SerialBoot tag `v0.2.4` (5de3006): `bootloader/`, `shared/`, `host/py/`, `test/` | MIT (+ BSD font, `bootloader/NOTICE`) |
| `hardware/` | Rev 0 schematic (PDF) and netlist (EasyEDA `.tel`) | 2026-08-21 | | |

The third-party code inside these (the WCH core and SPL, the USB CDC stack,
fonts) keeps its own notices: `board/THIRD-PARTY.md`,
`bootloader/THIRD-PARTY.md`, `bootloader/vendor/usbcdc/VENDORED.md` and
CHGfx's `LICENSE`.

## What the board package carries

Everything: the core, the menu bootloader, the `CHGame` library (with CHGfx
and CHSd beside it) and the casino games as its examples, so that
installing or updating "CHGame" in the Boards Manager brings all of it at
once. That is 0.3.0, released on 2026-10-07, built from here and tested
as a new user would get it (`python tools/release/stage.py`);
[../docs/roadmap.md](../docs/roadmap.md) has what is left. Between
releases, the pieces are used from this repository as described below,
or from a staged install
([board/docs/trying-a-release.md](board/docs/trying-a-release.md)).

## The board package (`board/`)

**Installing.** Install the board package through the Arduino Boards
Manager. That also installs the RISC-V GCC 8.2 toolchain, `chgame-upload`
and `wchisp`:

```bash
arduino-cli config add board_manager.additional_urls https://github.com/bateske/CHGame/releases/latest/download/package_chgame_index.json
arduino-cli core update-index
arduino-cli core install CHGame:ch32v
```

0.3.0 (2026-10-07) is the first release cut from this repository. The
CH32SerialBoot repository's URL offers 0.2.4 only. To try the next version
before it is published, stage it and install it from this machine:
[board/docs/trying-a-release.md](board/docs/trying-a-release.md).

**What the copy here is:**
- the source of the next release. A change made here does not reach a build
  until a release is installed (or the installed package is patched by
  hand), because arduino-cli compiles against the installed package, not
  this folder;
- the place to read the core, variant and linker script (the pin names are
  in `variants/CH32X035/CHGame_Rev0/variant_CHGame_Rev0.h`; the menus and their flags
  in `boards.txt`);
- 0.3.0: what the next release installs. What changed from 0.2.4 is in
  [board/CHANGELOG.md](board/CHANGELOG.md).

**Gaps:**
- The board docs sometimes refer to `bootloader/`, `host/` or `tools/`
  paths as they were in CH32SerialBoot. `bootloader/` and `host/py` are now
  under [bootloader/](bootloader). The release scripts are
  `tools/release/` (Python), described in `board/docs/building.md`.
- `board/docs/hardware-pinmap.md` leaves some ports as "—". The variant
  header has them all; the summary is in
  [../docs/platform.md](../docs/platform.md).
- The core has no README of its own here. The Tools menus, `Serial`
  behaviour and memory report are described in
  [../README.md](../README.md) and [../CLAUDE.md](../CLAUDE.md).

**Fixed here, released in 0.3.0:**
- **Linux builds failed.** `cores/arduino/ch32/lib/ch32yyxx.h` included
  `core_riscv_cH32yyxx.h`; the file is `core_riscv_ch32yyxx.h`, so it only
  resolved on case-insensitive file systems (Windows, default macOS). Fixed
  in this copy (ships with 0.3.0); CLAUDE.md gives the symlink workaround
  for an installed 0.2.4.
- **Stale comments** saying the bootloader is 8 KB and the app starts at
  0x2000 (`link_chgame_app.ld`, `chgame_map.h`), and one in `boards.txt`
  naming a `tools/gen_ld.py` that does not exist (2026-10-02).

**Changed here, released in 0.3.0** (2026-10-02; all 20 games built from the
staged package, sizes unchanged):
- **Smallest + LTO is the default** *Optimize* option. FQBNs without `opt=`
  now build with `-flto`; with plain `-Os` the larger games do not fit.
  The release FQBN names `oslto` already, so the games are unchanged.
  CHSDtoUSB's `tools/game.py` pins it to `opt=osstd` (`FQBN`), what it was
  tested with; CHSDtoSerial's pins `usb=serial` (its port is the website's
  protocol) with `GFX_CHUNK_ROWS=1` (`DEFINES`).
- **Tools > Bootloader** offers the SD Text Menu (Rainbow or Static) and
  USB Only (2026-10-03: the Plain and Casino colour themes went with menu
  v2, whose look comes from the card; the two styles differ only in colour
  15, turning or as painted: the Static style was White until 2026-10-03,
  when the tools began writing the picture's `#FF00FF` into palette entry 15
  instead of white, and the menu without a picture did the same); the 0.2.4 bootloader is no longer in the package (it is
  kept in `bootloader/release/0.2.4/` for rollback).
  The programmers are named for what they need.
- **Examples** are under *CHGame > Games* and *CHGame > Apps* (the folders
  were `games/` and `apps/`).
- **Every build writes `<sketch>.ino.chg`**, the SD menu package, with
  `chgame-upload pack` (`recipe.hooks.objcopy.postobjcopy.1`), so *Export
  Compiled Binary* leaves one by the sketch. It needs the uploader 0.2.0.

## CHGame (`board/arduino/CHGame/libraries/CHGame/`)

The library every game is built on; [its README](board/arduino/CHGame/libraries/CHGame/README.md)
is the reference and [../docs/chgame-library.md](../docs/chgame-library.md)
the record of its decisions. The board package carries it from 0.3.0:
- `tools/device.py` passes `--library <repo>/platform/board/arduino/CHGame/libraries/CHGame`,
  which wins over the installed package's copy;
- the simulator compiles its `src/` unless `CHSIM_CHGAME` points elsewhere;
- with 0.2.4, Arduino IDE users copy this folder into their sketchbook's
  `libraries/`.

A change to it is a change to every game: rebuild all twenty (size), and
compare their simulator frames (and, for `chgame/Audio`, the sound preview
hashes: `tools/audio/preview.py`).

## CHGfx (`board/arduino/CHGame/libraries/CHGfx/`)

The games compile against this copy:
- `tools/device.py` passes `--library <repo>/platform/board/arduino/CHGame/libraries/CHGfx` to
  arduino-cli. That takes priority over a CHGfx installed in the sketchbook.
- The simulator (`tools/chsim/chsim.py`) uses its `src/` unless
  `CHSIM_CHGFX` points elsewhere.
- The board package carries it from 0.3.0; with 0.2.4, Arduino IDE users
  copy this folder into their sketchbook's `libraries/`.

**Documentation:**
- Its README documents the API, draw modes and configuration macros.
- Its README links `../PERFORMANCE.md`. Here that document is
  [../docs/performance.md](../docs/performance.md).
- `extras/tests` is the library's own test suite (about 20,000 checks). It
  runs on the repository's simulator, `tools/chsim`, which took over CHGfx's
  own `extras/sim` on 2026-10-02 together with its panel model: `python
  tools/chsim/chsim.py test`, or `chgame --sketch CHGfx test`.

## CHSd (`board/arduino/CHGame/libraries/CHSd/`)

The SD reader of CHWords, CHCrossword and CHWordWheel. They include it as
a library (`<Fat.h>`, `<SdSpi.h>`); there are no copies in the games.

To change it:

```bash
cd platform/board/arduino/CHGame/libraries/CHSd
python tests/run_tests.py          # FAT16/FAT32 images, every failure mode
```

Then run `chgame check` in each of the three games. Its `tools/fatimg.py`
builds and reads FAT16/FAT32 card images; it is useful for any SD work.

## The bootloader (`bootloader/`)

The board's permanent bootloader with a game menu that installs games from
the SD card ([../docs/sd-menu.md](../docs/sd-menu.md)).
[bootloader/README.md](bootloader/README.md) has the design, the
differences from 0.2.4, building, testing and installing.

**It is in `platform/board`, not in a released package yet** (2026-10-02).
- `board/arduino/CHGame/bootloaders/CHGame/` carries the menu bootloader,
  the no-menu build and the 0.2.4 bootloader. *Tools > Bootloader* chooses
  one, and the programmer **CHGame USB** writes it through the bootloader
  already on the board (`chgame-upload burn`, tool version 0.2.0,
  `bootloader/host/go`); **WCH factory ISP** remains for recovery.
- Tried on a board through `arduino-cli burn-bootloader`: every change
  between the three, 0.2.4 to the menu included, then an upload.
- The installed package 0.2.4 has none of this: it offers the factory ISP
  only and writes the 0.2.4 bootloader.
- Games build against core 0.2.4 unchanged; the menu bootloader is
  compatible with it.
- To ship it through the Boards Manager, the release must also publish the
  `chgame-upload` 0.2.0 archives and name them in the package index.

**Menu v2** (2026-10-03, BOOT_VERSION 3): folders, the card's order and
background (`GAMES/MENU.IDX`, `MENU.BG`: [../spec/card.md](../spec/card.md)),
a game started at power-on (START held: the menu), the turning rainbow as a
colour of the picture. Errors are shown as numbers; `fault.c`, the STATUS
and READ commands, the colour themes and the code-drawn title went for
flash (11,920 B, 368 B free; a folder lists 240 entries, which fills the RAM: [bootloader/SIZES.md](bootloader/SIZES.md)).
Two styles: Rainbow (the default) and Static. It passes the PC suite, which
boots it on cards made by `tools/chcart`, and it ran on the board on
2026-10-03 ([bootloader/test/hil/RESULTS-2026-10-03.md](bootloader/test/hil/RESULTS-2026-10-03.md)):
`bootloader/release/` and `board/.../bootloaders/CHGame/` carry the images
that ran.

**The visual menu** (2026-10-03, `build.sh --ui=visual`): the same card
shown one picture at a time, no text, as the Arduboy FX does
([../docs/visual-menu.md](../docs/visual-menu.md)). The card's cover at
power-on, the folders' covers (LEFT/RIGHT), the games' pictures (UP/DOWN;
each in its CHG file), an about page, its own screens from the card
(`SYSTEM.PIC`) and built-in icons when the card cannot give one. 12,060 B
(228 B free), Rainbow and Static; Burn Bootloader offers both beside the
list menu. The list and visual menus share `bootloader/src/card.c`; the
list builds stayed byte-identical. Not yet run on a board.

## Changing a platform piece

1. Make the change here and say what it is for in the commit. For the
   bootloader, also list it in its README.
2. Rebuild every game. `chgame build` in each game must still
   fit, and the simulator frames must be unchanged or deliberately changed
   (CLAUDE.md rules 2 and 3).
3. `bootloader/src/sd.c` and `fat.c` are a C fork of CHSd: a fix to one
   belongs in the other too.
4. When a version number changes, update this table, `CLAUDE.md` and the
   root README.

## Changes since the copies were taken

- 2026-10-08: CHSd clocks one idle byte before each command (`cmd()` in
  `SdSpi.cpp`), the SD spec's N_RC: at least 8 clocks between a response
  and the next command. Identification sent its commands back to back
  (`read()` and `stream()` already waited first). A SanDisk 32 GB card
  ("SK32G") took them misaligned and stopped answering, so CHStlView and
  the word games found no card where the menu, whose `sd.c` always sent the
  byte, read it. Contributed with a board run on that card (PR #21).
  `tests/test_spi.cpp`'s card model now counts a command sent with no gap
  as a failure (11 or 12 per init without the fix). +8 B in each of the
  four sketches on CHSd (CHWords, CHWordWheel, CHCrossword, CHStlView),
  RAM unchanged; the simulator does not run `SdSpi.cpp`.

- 2026-10-08: the three libraries link from archives (`dot_a_linkage=true`
  in CHGfx's, CHGame's and CHSd's `library.properties`). A developer using
  only `audio::` and the buttons, with their own display code, found every
  sketch that includes `CHGame.h` carrying CHGfx's framebuffer: Arduino
  links a library's objects whole, and the DMA interrupt handler in
  `CHGfx.cpp` (a strong `DMA1_Channel3_IRQHandler` over the core's weak
  one) keeps `gfx_fb`, the chunk buffers and the SRAM converters with it.
  From an archive an object is linked only when something calls into it,
  so that sketch is 352 B of RAM instead of 9,712 B, and the two hooks the
  libraries override (that handler, and `osSystickHandler` in `Audio.cpp`)
  sit in the same objects as `gfx_begin()` and `audio::begin()`, so a
  sketch that uses the feature always gets them. Plain `riscv-none-embed-ar`
  indexes GCC 8's slim LTO objects, so the `-Os -flto` builds need no
  `gcc-ar`. All 23 examples' release images are byte-identical
  (board/CHANGELOG.md, "Unreleased"). The simulator does not read the flag.

- 2026-10-07: an API reference for every library the board package ships.
  The public headers of CHGame, CHGfx, CHSd, SPI, Wire and EEPROM carry
  Doxygen comments (a brief, every parameter, the return value, and the
  details that matter, after the Arduboy2 library's reference), and
  `docs/api/` builds them into one site, published to
  <https://bateske.github.io/CHGame/> by `.github/workflows/docs.yml`
  ([../docs/api/README.md](../docs/api/README.md)). The headers' code is
  unchanged (EEPROM.h aside, below) and all 20 games' release images are
  byte-identical. Save.h's example said a record holds 248 bytes; it holds
  244 (`save::MAX_DATA`). Writing the comments turned up three things,
  fixed with it (board/CHANGELOG.md, "Unreleased"):
  - **EEPROM**: the CH32V003's option-byte emulation erased and rewrote the
    CH32X035's option bytes in a way WCH's library for this chip never
    does, and a failed write could leave the chip read-protected. On the
    CH32X035 its `commit()` now writes nothing and returns `false`, and
    including it prints a compiler message.
  - **Wire**: `endTransmission()` returned 4 after a timeout for a device
    that did not answer; it now returns Arduino's 2 or 3 at once and sends
    a STOP, and reads of an absent device release the bus. Compiled, not
    yet run on hardware.
  - **CHGfx**: `gfx_setSpiDiv()` now programs all of SPI1, so
    `gfx_setSpiDiv(gfx_spiDiv())` hands the bus back after the Arduino SPI
    class (which resets SPI1). No game calls it.

- 2026-10-07: the dealer redrawn, and a spotlight behind him.
  `tools/art/common/dealer.png` and `faces.png` are the owner's new dealer
  and his seven expressions. The eight games with a dealer use them:
  CHBlackjack, CHCraps and CHYacht now read `faces.png` too, instead of
  rebuilding Press Play On Tape's expressions, so all eight carry the same
  arrays (the sibling checks agree). The CHGame library gains
  `spotlight()` (`chgame/Draw.h`): a disc of 25% dots, the checker on every
  other row. Seven games draw a white one, radius 24, centred on the
  dealer's head (x `DEALER_X + 23`, y 17), where the wood rectangle was
  (CHYacht has no backdrop). The dealer is no longer drawn twice: the games
  painted `FACE_NORMAL` over `DEALER`, which already has that face
  (identical frames, about 125 B each). The assets writer of six games
  could cut a 16-bit value across two lines (CHSlots' output re-wrapped, no
  value changed). CHCraps drops the angry expression it never shows (62 B).
  Images (B) with these changes: Bingo 36,964, Blackjack 46,388, Craps
  50,384, Four 36,736, Roulette 49,892, Tic Tac Toe 50,248, Word Wheel
  50,112, Yacht 44,652, all with both save pages; the other twelve
  unchanged. Checked: `chgame check --quick --no-device` in the eight,
  Blackjack's frames identical without the second face, their README GIFs
  re-recorded.

- 2026-10-07: what two outside reviews led to (the rest of their
  suggestions, and why they were set aside, are in
  [../docs/performance.md](../docs/performance.md), "Dead ends").
  - **CHGfx 1.3.1: a 12 bpp-only start.** `gfx_begin()` is inline: a
    constant `GFX_12BPP` calls `gfx__begin12()`, anything else
    `gfx__begin()`. The 16 and 18 bpp converters and LUT builders are
    reached through two pointers that only `gfx__begin()` and
    `gfx_setColorMode()` set, so a sketch that only runs 12 bpp (every game
    and app) links neither: 168-460 B less image and 176 B less RAM in every
    sketch, from SRAM code mostly. The 12 bpp path is the same code. The 16
    bpp examples moved by -160 to +52 B; the simulator's stand-in has the
    two starts too. Frames: the simulator does not run `CHGfx.cpp`, so a
    board run of a 12 bpp game and of `Benchmark` (16 bpp) is still owed.
  - **The board package links LTO as one partition**
    (`-flto-partition=one`, a new `build.flags.ltolink` that the Optimize
    option's `oslto` sets; `platform.txt`'s link recipe). gcc split ten of
    the sketches in two; nine are 40-376 B smaller, none bigger.
  - **CHSd turns the card's CRC checking off** (`CMD59`, in `ident()`).
    CHSDtoUSB turns it on, the card stays powered across a reset, and an
    upload from CHSDtoUSB starts the game without the menu (whose `sd.c`
    already turns it off), so the word games could find no card. Its
    argument's stuff bits make the frame's CRC the usual 0x95 (+12 B, not
    +24). `tests/test_spi.cpp` runs `SdSpi.cpp`'s init and read against a
    card model that keeps CRC on across CMD0 (fails without the fix).
  - **Two save pages are a build requirement.** `SAVE_PAGES` in a game's
    `tools/game.py` (two by default); `tools/check_size.py --save-pages`,
    which `chgame build` passes for a release build, fails below it.
    Nothing checked it before: since the title art (2026-10-06) CHCraps was
    50,764 B, past both pages (saving off), and CHTicTacToe 50,508 B (one
    page). With the two changes above they are 50,428 and 50,232 B.
- 2026-10-07: board revisions, ahead of a rev1 whose pins will differ
  ([../docs/hardware-revisions.md](../docs/hardware-revisions.md); the rules
  in [../spec/chgame.md](../spec/chgame.md), "Devices and revisions"). Each
  board is a device `rev<n>` with a four-character target id: rev0 keeps
  `CX35`, and rev1 is reserved as `CGR1`. `bootloader/shared/chg_format.h`
  names the ids (`CHG_TARGET_REV0`, `CHG_TARGET_REV1`), and
  `CHG_TARGET_ID` is now the board the build is for: rev0 unless
  `build.sh` gets `CHBOOT_BOARD_TARGET`. A later board's build also sends
  its id in `HELLO` (offset 30) and writes it at offset 0x14 of its image
  ([board/docs/protocol.md](board/docs/protocol.md), "Which board").
  Measured with rev1's id: +16 B (list 11,936, visual 12,076). Every rev0
  build is byte-identical (all seven of `dist.sh`'s compared). The
  uploaders (Go and Python) read the board from `HELLO`, refuse a
  bootloader image for another board, and take `-device` on `flash`
  (refusing another board) and `pack`. `platform.txt` does not pass
  `-device` yet (the checklist's step 4 does that, with an uploader version
  bump). chcart writes each device's id into its CHG files (until now
  `chgpack.pack` always wrote `CX35`), backs a card up under the board its
  CHG files name, and takes `--device` on `cart prepare`, `flash` and
  `deploy`. The format change: a binary for an unknown or reserved device
  is now the warning `unknown-device` (not used, kept), not the error
  `bad-device`, so readers made before rev1 still use a cart's rev0
  binaries. Checked: the bootloader's PC suite (a new `core_rev1` suite and
  a board test in every menu build), the uploaders' suites and shared
  vectors (additions only), chcart's tests and the schema test, the
  fixtures (`bad/bad-device` remade, `good/devices` new, nothing else
  changed), and the casino card and cart rebuilt byte for byte.

- 2026-10-04: box art for every picture the menus show, painted with the
  new `tools/artkit` ([../docs/cover-art.md](../docs/cover-art.md)): each
  example's `docs/cart.png` and `tools/cart.py`, the casino card's cover and
  folders, and the spec's default pictures (`spec/assets/`; the fixtures
  regenerated: only COVER.PIC's and SYSTEM.PIC's hashes changed). The
  visual menu's four built-in icons (`bootloader/art/icons/`) are redrawn as
  solid silhouettes, same 12x12 bitmaps, 0 B: `chgame_sdvisual*.bin` and
  the dry run rebuilt (12,080 and 11,896 B, as before), every list build
  byte-identical. Checked: no library change; the bootloader's PC suite
  (45 frames re-pinned: the icon screens and the real card's pictures),
  chcart's tests, the fixtures and `chgame boxart --check` pass.

- 2026-10-06: the bootloader after the owner's review of the menus
  (`bootloader/README.md`, "Changes of 2026-10-06"): the visual menu stays
  on the cover at power-on (A there shows the installed game); a launch
  game starts straight in if installed, else the menu opens on it and A
  installs it, so a power-on never writes flash; the Static text menu
  draws colour 15 in the text colour; LEFT/RIGHT do nothing without
  folders; the USB-only bootloader touches only the LED and USB, and every
  build leaves its unused pins high-Z (the buzzer included). Sizes: list
  11,920 B, static 11,716, visual 12,060 (36 B under its margin), USB-only
  5,340. With it the `.chgame` format gains `menu.systemImages` (a cart's
  own versions of the visual menu's screens, the web tool's `x-chgame-web`
  read as an alias; [../spec/chgame.md](../spec/chgame.md)) and `.elf`
  input for carts and CHG files. Checked: the bootloader's PC suite (112
  frames re-pinned), chcart's tests, the fixtures (two new).
  Later that day, CHG files gained the game's record (`spec/chg.md`, field
  0x06C; `shared/chg_format.h` names it, no bootloader code reads it, no
  binary changed) and `chgame cart backup` makes a card into a cart again
  from it, SD files included (`spec/card.md`, "Backing up a card"). Every
  prepared CHG file is about 21 KB longer. Checked: the bootloader's PC suite
  (no frame changed), chcart's tests, the fixtures (prepared CHG bytes
  re-pinned; five backup cards new), the casino card backed up into its own
  cart and prepared again byte for byte.
  Later still, the title screens' titles: thirteen games (Bingo, Blackjack,
  Boardwalk, Craps, Dominoes, Mahjong, Poker, Roulette, Slots, Snakes,
  Solitaire, Tic Tac Toe, Yacht) draw their cover's lettering, extruded and
  outlined as there, in the house gold as a smooth gradient: a hint of
  white at the top, a wide GOLD middle, a WOOD foot, the steps blended by an
  ordered dither baked into each row of the title's data (no bevel, no
  glints: the owner's choices after seeing the cover's chrome bands, which
  looked washed out and busy) ([../docs/cover-art.md](../docs/cover-art.md),
  "The same title on the title screen"). The seven font-drawn titles are
  untouched. The CHGame library gains `maskTitle()` and `titleArt()` in
  `chgame/Mask` (about 400 B of code in a game that calls them, 0 B in one
  that doesn't, 56 B of RAM for the row shifter it runs from SRAM);
  `tools/titleart.py` packs a recipe's `title_lines()`. Sizes (B): Bingo
  36,935 (+881), Blackjack 45,896 (+654), Boardwalk 50,051 (+790), Craps
  50,387 (+725, 45 B left), Dominoes 43,485 (+942), Mahjong 48,626 (+487),
  Poker 49,000 (+844), Roulette 49,974 (+566), Slots 48,116 (+831), Snakes
  37,342 (+386), Solitaire 31,357 (+764), Tic Tac Toe 50,115 (+934, with its title screen's falling X and O pieces in place of the demo board), Yacht
  44,783 (+983); the other seven unchanged to the byte. Boardwalk's and
  Snakes' title screens draw the title every frame over a moving scene:
  about 50 and 40 fps there (simulator estimates). Checked: every cover
  byte-identical (`docs/cart.png`), the seven untouched games' images
  unchanged, `chgame check --quick --no-device` in the thirteen, their
  README GIFs re-recorded (Mahjong's title clip half a second shorter to
  stay under 1 MB), `chgame boxart --check`.

- 2026-10-03: the visual menu (above): `bootloader/src/card.c` split out of
  `menu.c` (every list build byte-identical), `visual.c`, `icons.h` from
  `art/icons/`; Burn Bootloader offers SD Graphic Menu (Rainbow, Static)
  (`boards.txt`, `bootloaders/CHGame/`); `release.py` lists the five
  shipped binaries (its stale `_plain`/`_casino` names fixed). The card
  gains `COVER.PIC`, `SYSTEM.PIC` and a picture in each CHG file
  ([../spec/card.md](../spec/card.md)); each example has `docs/cart.png`
  from its `tools/cart.py`, and the casino card is in genre folders.
  Checked: no library change (the 22 release images unchanged); the
  bootloader's PC suite (both faces, the real card included), chcart's tests
  and the conformance fixtures pass.

- 2026-10-03: the bootloader's menu v2 (above). Both uploaders skip
  `-verify`'s readback on BOOT_VERSION 3, which has no READ
  (`host/py/chgame_upload/upload.py`, `host/go/upload.go`); their parity
  tests pass. Burn Bootloader offers SD Text Menu (Rainbow, Static) and USB Only
  (`boards.txt`; the plain and casino binaries are gone). The examples each
  have a `chgame.json` (the `.chgame` format, [../spec/chgame.md](../spec/chgame.md)).
  Checked: the 22 release images unchanged (no library change); the
  bootloader's PC suite, chcart's tests and the conformance fixtures pass.

- 2026-10-02: CHSd gained `fat::root()`, `fat::list()` (any folder's files
  and folders, through a callback) and `sd::stream()` (a CMD18 run of blocks
  at 24 MHz by DMA, each block handed over while the next arrives), for the
  new app CHStlView; the simulator's card (`host/sd_host.cpp`) has
  `stream()` too. `tests/run_tests.py` checks `list()` against every image.
  Checked: CHWords, CHWordWheel and CHCrossword release images byte for byte
  the same (they call none of it).
- 2026-10-02: `chgame/Sizzle.inl`: the pop-in banner's rainbow outline is
  behind `SIZZLE_IS_RAINBOW()`, so a sketch whose `SIZZLE_STYLES` leaves
  the rainbow out compiles (it named `B_RAINBOW` unconditionally; every game
  has the style, the two apps do not). Checked: all twenty release images
  byte for byte the same.

- 2026-10-02: `chgame/Sizzle` (`libraries/CHGame/src/chgame/Sizzle.h` and
  `Sizzle.inl`): the particle pool, the banners and the floating texts the
  twenty games each carried in `src/fx/Fx.cpp` are one body in the
  library, configured per game with `SIZZLE_*` switches in its `src/fx/Fx.h`
  and compiled in its `src/fx/Fx.cpp` under its own size pragma (an
  implementation header, since the library is compiled apart from the
  sketch). Checked: all twenty release images byte for byte the same as
  before, static RAM unchanged, every script's frames, every README GIF,
  check, redraw and save test the same. (CHSlots' device debug image is 4 B
  smaller: LTO partitions the unchanged code differently; the release image
  is identical.)
- 2026-10-02: one simulator. `tools/chsim` took over CHGfx's `extras/sim`: its
  panel model (the measured wire rate scaled by the SPI divider, 45 us of
  setup, rows landing on a simulated panel as they convert, per-column
  tearing detection, the board's palette) and its free-running mode; CHGfx's
  tests moved to `extras/tests` and run on it. A full 12 bpp flush is 28 us
  shorter than the old model's; games that take a seed or an animation phase
  from the clock take another branch from there on, so 11 of 201 script runs
  and CHCheckers' README GIF were re-recorded (CHBoardwalk `save`, CHCheckers
  `gameplay`, CHFour `play1` and `showcase`, CHPoker `monkey` and `showcase`,
  CHSlots `save`, CHSolitaire `cascade`, `save` and `screens`, CHWords
  `card_words`); every other run is frame for frame the same, with no `BUG:`
  line anywhere, and every check, redraw and save test unchanged.
- `board/`: the core is as released; `arduino/CHGame/libraries/` gained CHGame, CHGfx and CHSd.
- `board/arduino/CHGame/libraries/CHGfx/`: `library.properties` gives this repository's URL.
- `board/arduino/CHGame/libraries/CHGame/`: new.
- `board/arduino/CHGame/libraries/CHSd/`: used as a library by the three SD
  games since 2026-10-02 (`tools/vendor.py` and the games' copies are gone);
  `architectures=ch32v`; the simulator's card is `$CHSD_CARD`.
- 2026-10-02: the three libraries moved from `platform/libraries/` into the
  board package's `libraries/` folder. Checked: 17 release images byte for
  byte the same, the three SD games 4 to 56 B smaller, every game's README
  reel frame for frame the same from the simulator.
- `bootloader/`: the SD menu work, listed in its README.
- 2026-10-02: the core's crash handler (`cores/arduino/ch32/chgame_boot.c`
  `chgame_fault()`, reached from `HardFault_Handler` and `while1_handler`)
  silences the piezo and lights the LED; debug builds also keep the crash
  for the CHGame library's `!` command (CHANGELOG). Release images grow
  64-72 B (CHCrossword 50,416 B, still under 50,432). Builds see it only
  through a patched installed 0.2.4 until the next release.
