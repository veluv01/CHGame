<!-- Modified 2026-10-11: WeAct F8U6 hardware variant; see docs/weact-f8u6.md. -->
# The .chgame format, version 1

A `.chgame` file is how CHGame games are shared and distributed: one file
holding one or more compiled games, what they are called and who made them,
the files they read from the SD card, and pictures of them. Uploaders,
card builders and emulators take it as their input. Like the Arduboy's
`.arduboy`, from which it borrows, it is a ZIP file with a manifest,
`info.json`, at its root.

This document is the format's definition. The reference implementation is
[tools/chcart](../tools/chcart) (`chgame export`, `chgame cart ...`), and
[fixtures/](fixtures) holds conformance cases with the results an
implementation must give. What a CHGame does with a cart, its menu and SD
card, is [card.md](card.md): a separate step, so that this format stays the
same when the card's layout changes.

**Words.** A *cart* is a `.chgame` file; it holds one *game* or several.
"Must" and "must not" are requirements; a file that breaks one is not a
`.chgame`, and a reader refuses it.

## The container

- **A ZIP file** (PKWARE APPNOTE), named `*.chgame`. Each entry is stored
  (method 0) or deflated (method 8). Entries must not be encrypted. ZIP64 is
  not needed and need not be read.
- **Paths:**
  - printable ASCII, with `/` between folders;
  - relative, so no leading `/`, no drive letter or `:`;
  - no empty, `.` or `..` part, and no `\`;
  - two entries must not have the same path, even ignoring case;
  - an entry whose name ends in `/` is a folder and is ignored.
- **The manifest** is `info.json` at the root: UTF-8 JSON, one object.
- **Every other file is named by the manifest.** A file it does not name is
  ignored, with the warning `unused-file`. Where files sit inside the ZIP is
  up to the writer: readers go by the manifest's paths.
- **Limits** a reader may enforce: 4,096 files, 512 MiB uncompressed in all.
- **Writing.** chcart writes the same bytes for the same cart: `info.json`
  first, then the paths in sorted order, every entry dated 1980-01-01 00:00,
  deflate level 9, no extra attributes. Its layout is `<id>/<device>.bin`,
  `<id>/LICENSE`, `<id>/cart.png`, `<id>/sdcard/...` and `menu/...`. Readers must not depend on that layout.

## info.json

### The cart

| Key | | Value |
|---|---|---|
| `schemaVersion` | required | `1` |
| `title` | required | the cart's name: any text, not blank |
| `version`, `author`, `description`, `license`, `url`, `sourceUrl` | | text. `license` is an SPDX expression |
| `date` | | ISO 8601: `2026-10-03`, or with a time |
| `launch` | | the `id` of the game a CHGame starts at power-on instead of showing its menu (card.md) |
| `menu` | | how the cart's menu looks: below |
| `games` | required | the games, one or more, in menu order |

### `menu`

| Key | Value |
|---|---|
| `background` | path of a PNG, 128x128, opaque: the whole picture behind the menu, its title or logo included. The menu draws over rows 20-119 only (card.md). Pixels of exactly `#FF00FF` are drawn in the menu's colour 15: one colour turning through the rainbow, or as painted on a bootloader in the Static style. Besides `#FF00FF` and the four menu colours below, at most 11 colours (card.md says why, and how it is converted). `chgame background` makes any image into one (docs/menu-image.md) |
| `colors` | the menu's own colours, `#RRGGBB`: `text` (titles, default `#FFF4D6`), `disabled` (a file the menu cannot install, `#808080`), `selectedText` (the title on the selection bar, `#000000`), `mark` (the installed game's chip, `#D62020`) |
| `folders` | a list of `{"name": "CARD GAMES", "background": "path.png", "cover": "path.png"}`: a folder's own background, the same kind of PNG (folders without one show their parent's); and its cover for the visual menu, a picture (below; folders without one show the menu's no-cover screen) |
| `cover` | path of a picture (below): the cart's cover, which the visual menu shows at power-on and as the top level's first row |
| `about` | path of a picture: the visual menu's about page (B at the top level), how the menu works; the default explains the keys |
| `systemImages` | the visual menu's own screens, any of them, each the path of a picture: `installed` (the program in flash, when no game on the card holds it), `game` (a game without a picture), `folder` (a folder without a cover), `error-1` to `error-5` (the install errors, numbered as in docs/sd-menu.md). A key that is absent takes the default that comes with the tools ([assets/system/](assets/system)); one that is given must follow the picture rule (`bad-picture`). card.md, step 7, says how they reach the card (`SYSTEM.PIC`). The bootloader keeps its own built-in screens for a card it cannot read: these pictures never remove that |

Everything the bootloader reads from the card is therefore the cart's to
set: the list menu's `background` (and a folder's), the visual menu's
`cover` (and a folder's), `about` and `systemImages`, and each game's
`cartImage`. A cart that sets none of them is complete all the same: the
tools fill in the defaults when they prepare the card.

### A game

| Key | | Value |
|---|---|---|
| `id` | required | the game's handle within the cart: `a-z`, `0-9` and `-`, 1 to 32 characters, not starting with `-`; unique in the cart |
| `title` | required | what the menu shows: printable ASCII, 1 to 31 characters. The menu shows 19, in capitals (`a-z` are drawn as `A-Z`; characters after `_`, except `a-z`, as `?`) |
| `folder` | | the menu folder the game is in: `""` or absent for the top level, `"CARD GAMES"`, `"CARD GAMES/CLASSIC"` for one inside another. Each name is printable ASCII, 1 to 19 characters, no `/`, no space at either end; 4 levels at most |
| `version`, `author`, `description`, `genre`, `license`, `url`, `sourceUrl` | | text |
| `licenseFiles` | | paths of the game's licence and notice files (for example LICENSE and NOTICE), kept with its binary as its licence asks. Their last part is a plain file name |
| `buttons` | | `[{"control": "A", "action": "Lay a tile"}]`, as in `.arduboy` |
| `binaries` | required | `[{"device": "rev0", "filename": "path.bin"}]`: the game built for each board it runs on, at most one per device (below) |
| `sdcard` | | a folder in the ZIP, ending in `/`: every file under it goes onto the SD card's root at the same relative path (below) |
| `cartImage` | | path of a 128x128 PNG: the game's picture, its box art, which the visual menu shows while it is selected. A picture (below); one that breaks the picture rule is a warning (`bad-picture`) and the card gets no picture of it |

### Order and folders

- **The order of `games` is the menu's order.**
- **A folder lists at most 240 entries** (games and folders), and so does
  the top level (`full-folder`). Folders hold any number of games between
  them.
- **A folder** takes its place in its parent's list where its first game
  is, in that order.
- **`menu.folders` only adds a background and a cover.** It never orders anything.

### Pictures

`menu.cover`, `menu.about`, a folder's `cover` and a game's `cartImage` are
pictures for the visual menu: a 128x128 PNG, opaque, with at most 11 colours
besides `#FF00FF` and the menu's four at their default values (`#FFF4D6`,
`#808080`, `#000000`, `#D62020`), whatever `menu.colors` says, so a picture
means the same in any cart. `#FF00FF` turns through the rainbow on a Rainbow
bootloader. A `menu` picture that breaks the rule is an error
(`bad-picture`). `chgame picture` converts any image into one
(docs/visual-menu.md); card.md, step 7, says how a card stores them.
`chgame export` and the cart tools draw a picture for a game that has none
and a cover for a folder that has none (tools/boxart.py), outside this
format: the cart carries the PNG, so readers never have to draw one.

Example: games `A` (top level), `B` (folder `F`), `C` (top level), `D`
(folder `F`). The top level lists `A`, `F`, `C`, and `F` lists `B`, `D`.
To put a folder first, put its games first: `chgame cart order cart.chgame
F/` does that (an item ending in `/` is a folder's games).

### Binaries and devices

A binary is the raw program image (`.bin`), exactly what the board
package's build writes as `<sketch>.ino.bin`, linked to run at the device's
load address. It is never a `.hex`, an `.elf` or a `.chg`: tools convert
those on the way in (an `.elf`'s loadable segments at their load addresses,
as `objcopy -O binary` does), so readers only ever see one format.

| `device` | Board | Target id | MCU | Load address | Largest image | Notes |
|---|---|---|---|---|---|---|
| `rev0` | CHGame Rev0 (`CHGame:ch32v:rev0`) | `CX35` (`0x35335843`) | CH32X035G8U6 | 0x3000 | 50,944 B | saving needs the image at 50,432 B or less (above that a game's saves switch off: warning `save-pages`); CHG layout `0x003000F7` (chg.md) |
| `weact-f8u6` | CHGame on WeAct (`CHGame:ch32v:weact-f8u6`) | `WF86` (`0x36384657`) | CH32X035F8U6 | 0x3000 | 50,944 B | fork variant; save-safe 50,432 B; layout `0x003000F7`; no sound or external status LED; see [hardware guide](../docs/weact-f8u6.md) |
| `rev1` | CHGame Rev1, **reserved**: not defined yet | `CGR1` (`0x31524743`) | | | | the name and id are taken; until this row is filled in, a `rev1` binary is one no reader knows (`unknown-device`) |

- **The image's length**, padded with 0xFF to a multiple of 4, must not
  exceed the device's largest image.
- **A bootloader image is refused** (`bootloader-image`): one with the
  signature `0x4C424843` ("CHBL") as a 32-bit little-endian word at offset
  8.
- **A game runs on a board only if it has a binary for it.** A game need not
  have one for every device.

### Devices and revisions

A *device* is a board as a program sees it: its MCU and clock, its memory
map and bootloader, and the pins its screen, card, buttons, speaker and
lights are on. A binary is built for one device and runs only on that one:
nothing ever runs a binary on another device, or falls back to another
device's binary.

**When a board needs a new device.** A new board gets a new device when a
binary built for the board before it might not run correctly on it: a pin
moved, another MCU or clock, another flash or RAM map, another bootloader
reservation, another screen. A board that every existing binary runs on
unchanged (another PCB colour, battery or case, a part swapped for an
equivalent on the same pins) keeps its device. When in doubt it is a new
device: a needless one costs a rebuild, a missing one games that misbehave.

**Names.**
- **The CHGame handheld's revisions are `rev<n>`**: `rev`, then the
  revision number in decimal without leading zeros: `rev0`, `rev1`, ...,
  `rev10`. The same name is the board's ID in the board package (FQBN
  `CHGame:ch32v:rev1`), its variant (`CHGame_Rev1`) and its name in the
  Arduino IDE ("CHGame Rev1").
- **Numbers go up in release order.** A number is never given twice: a
  revision abandoned before release keeps its number, unused.
- **A board that is not a revision of the handheld** (another product, a
  development board) gets a name of its own, not a `rev` number:
  lowercase `a-z`, `0-9` and `-`, 1 to 32 characters, starting with a letter.
- **Every device name** follows that last rule. A name that breaks it is
  the error `bad-device`.

**Target ids.** Each device has a target id: a 32-bit value, stored
little-endian, whose four bytes are printable characters. It is how
everything below the cart says which board it is for: a CHG file's header
(chg.md), the bootloader's `HELLO` answer and the bootloader image itself
(platform/board/docs/protocol.md).
- **`rev0` is `CX35`** (`0x35335843`). It was named after rev0's MCU before
  there were other boards, and keeps that id for good: every rev0
  bootloader and CHG file in use carries it.
- **`rev1` to `rev9` are `CGR1` to `CGR9`** ("CHGame Rev" and the digit);
  **`rev10` to `rev99` are `CG10` to `CG99`**.
- **A board with a name of its own** gets four characters of its own when it
  is registered: letters and digits, not starting `CG` or `CX`, and not one
  of the formats' magic numbers (`CHG1`, `CHBL`, `CHB1`, `CHGM`, `CHGB`,
  `CHGD`).
- **An id is never reused or given to another board**, and is never 0: in a
  bootloader image's board word, 0 means rev0.

**Registering a device** is a new row in the table above, made with the
reference tools, the bootloader and the uploaders
([docs/hardware-revisions.md](../docs/hardware-revisions.md) is the
checklist). A row can be *reserved* first, naming a board and its id before
anything else about it is settled, so that nothing else takes them. Readers
treat a reserved device as one they do not know. A new device is not a new
`schemaVersion` (below, "Versions").

**What readers and writers do.**
- **A game has at most one binary per device** (`bad-device` otherwise).
- **A reader picks a binary by its `device`**, never by its place in the
  list.
- **A binary for a device the reader does not know** (one registered after
  the reader was made, or a reserved one) is the warning `unknown-device`.
  The reader does not use or check it, and uses the rest of the cart as
  usual. A writer that edits the cart keeps it as it is. So a cart that
  carries rev0 and rev1 binaries still works, for rev0, in a reader made
  before rev1.
- **Preparing a card or uploading a game is for one device** (card.md): a
  game without a binary for that device cannot be put on that board.

### SD card files

- **Paths.** Each part of a path under `sdcard` must be an 8.3 name in
  capitals: 1 to 8 characters, optionally `.` and 1 to 3 more, from `A-Z`,
  `0-9` and ``! # $ % & ' ( ) - @ ^ _ ` { } ~``. The CHGame's FAT readers
  know nothing else.
- **`GAMES/` belongs to the menu**: no game may put files there.
- **Two games may carry the same file** (same path, same bytes): it is
  written once.
- **The same path with different bytes is an error** (`sd-conflict`). A
  card holds one.

Example: `"sdcard": "chwords/sdcard/"` and the ZIP entry
`chwords/sdcard/WORDS.DIC` put `WORDS.DIC` in the card's root.

## Rules a reader checks

| Code | | When |
|---|---|---|
| `not-a-zip` | error | the file is not a ZIP |
| `bad-zip` | error | an encrypted entry, another method, an unsafe or clashing path, over the limits |
| `no-manifest` | error | no `info.json` at the root |
| `bad-json` | error | `info.json` is not UTF-8 JSON, or not an object |
| `schema-version` | error | `schemaVersion` missing, not a whole number, or newer than the reader knows |
| `missing-field` | error | a required key is missing; `games` or a game's `binaries` empty |
| `bad-field` | error | a value of the wrong kind or form (text, list, date, colour, file name, button) |
| `missing-file` | error | a path the manifest names is not in the ZIP |
| `bad-id` | error | an `id` breaks its rule |
| `duplicate-id` | error | two games share an `id` |
| `bad-title` | error | a title is blank, not printable ASCII, or over 31 characters |
| `bad-folder` | error | a folder name breaks its rule, or folders nest deeper than 4 |
| `bad-device` | error | a device name that breaks its rule, or two binaries for one device |
| `binary-size` | error | an image empty, or larger than the device takes |
| `bootloader-image` | error | a bootloader, not a program |
| `bad-sd-path` | error | an SD path that is not 8.3 capitals, or under `GAMES/` |
| `sd-conflict` | error | two games' SD files at one path with different bytes |
| `bad-image` | error | a picture of the wrong kind or size; a background with transparent pixels |
| `bad-background` | error | a background with too many colours |
| `bad-launch` | error | `launch` names no game |
| `full-folder` | error | a menu folder (or the top level) holds more than 240 entries, games and folders together: the menu could not list them all |
| `unknown-key` | warning | a key this version does not define: ignored (a key beginning with `x-` is an extension, not an unknown key) |
| `unused-file` | warning | a file the manifest does not name; an `sdcard` folder with nothing in it; a folder background for a folder no game is in |
| `unknown-device` | warning | a binary for a device the reader does not know (a board registered after it, or a reserved one): not used, kept |
| `extension-conflict` | warning | `x-chgame-web.systemImages` names a screen that `menu.systemImages` also names, with a different picture: the official field is used |
| `long-title` | warning | a title over 19 characters (the menu shows the first 19) |
| `title-chars` | warning | a title with characters the menu shows as `?` |
| `save-pages` | warning | an image too large to keep the save pages |

A reader that meets an error refuses the whole cart. Warnings change
nothing.

## Versions

- **`schemaVersion` changes when a reader that ignores the change would
  behave differently.** A reader refuses a version newer than it knows.
- **New optional keys that may safely be ignored** do not change it. Older
  readers warn (`unknown-key`) and go on. `menu.systemImages` is one: a
  reader from before it shows the default screens and every game still
  plays.
- **New devices** are additions to the table above, not a new version. A
  reader from before one warns (`unknown-device`), leaves that device's
  binaries alone and goes on. Until 2026-10-07 an unknown device was the
  error `bad-device`; no cart carried one then.

## Extensions

A top-level key beginning with `x-` belongs to another tool (`x-chgame-web`
is the web emulator's). A reader ignores the ones it does not know, without
a warning, and a writer that edits the cart keeps them as they are (it does
not carry files they may name: a tool that needs them names them in the
manifest proper). Two rules for the one extension that overlaps this
format:

- **`x-chgame-web` with `"version": 1`** may hold `systemImages`, the same
  eight keys as `menu.systemImages`, from before that field existed. A
  reader takes them as `menu.systemImages` for every screen the official
  field leaves unnamed; where both name a screen, the official field is
  used, and a different picture is the warning `extension-conflict`. A
  picture named there is checked like the official one (`missing-file`,
  `bad-picture`). A writer puts them in `menu.systemImages` and keeps the
  rest of the extension.
- **Any other version** of `x-chgame-web` is not read as version 1: it is
  kept as it is, and files only it names are `unused-file`.

## An example

```json
{
  "schemaVersion": 1,
  "title": "Word Night",
  "author": "bateske",
  "launch": "chwords",
  "menu": {"background": "menu/background.png", "colors": {"text": "#FFFFFF"}},
  "games": [
    {
      "id": "chwords",
      "title": "WORDS",
      "version": "0.1",
      "author": "bateske",
      "genre": "Word",
      "license": "Apache-2.0",
      "licenseFiles": ["chwords/LICENSE", "chwords/NOTICE"],
      "binaries": [{"device": "rev0", "filename": "chwords/rev0.bin"}],
      "sdcard": "chwords/sdcard/"
    },
    {
      "id": "chcrossword",
      "title": "CROSSWORD",
      "folder": "PUZZLES",
      "binaries": [{"device": "rev0", "filename": "chcrossword/rev0.bin"}],
      "sdcard": "chcrossword/sdcard/"
    }
  ]
}
```

## What came from .arduboy, and what changed

**Kept:**
- a ZIP renamed, with `info.json` at its root;
- `schemaVersion`;
- the cart's text fields (`title`, `author`, `version`, `description`,
  `date`, `genre`, `url`, `sourceUrl`, `license`);
- `binaries` as a list of `{device, filename}`, one per board;
- `buttons`, a cart image per game.

**Changed:**
- **No screenshots.** A cart holds what the device and the tools that
  prepare a card use: binaries, SD files, licences and the menu's pictures.
  Gameplay pictures and GIFs stay with the game's own project (a sketch's
  `docs/gameplay.gif`, its README), not in the cart. A `screenshots` key
  from an older cart is an unknown key (warning `unknown-key`, its files
  `unused-file`) and is dropped.
- **One file holds several games.** `games` is the list, in menu order,
  with folders. A single game is a list of one.
- **Binaries are raw `.bin`, never `.hex`.** The load address comes from
  the device.
- **SD card files** (`sdcard`) take the place of FX flash data. There are no
  per-game save files yet: every game shares the board's two save pages.
  Each game's CHG file on the card records which files it brought
  (chg.md), so a card can be backed up into a cart again.
- **Pictures are the colour screen's**: 128x128, and backgrounds have a
  palette rule.
- **The rules are exact, and their codes stable**, with fixtures, so that
  tools and emulators read a cart the same way. With `.arduboy`, tools that
  read the manifest differently, or not at all, made that hard.

## Making and reading carts

| | |
|---|---|
| A sketch as a cart | `chgame export` in its folder writes `build/<Name>.chgame`, described by the sketch's `chgame.json` (tools/chcart/sources.py: every key optional) |
| Carts from games and other carts | `chgame cart new OUT ITEM ...` (`.chgame`, `.bin`, `.hex`, `.elf`, `.chg`, sketch folders), `add`, `remove`, `order`, `set`, `launch`, `background`, `picture` (the cover, a game's, a folder's, the about page, `--system` one of the menu's own screens) |
| Checking | `chgame cart verify FILE ...` prints every issue and exits 1 on an error; `chgame cart info FILE --json` prints the manifest as chcart reads it |
| The card | `chgame cart prepare FILE DIR [--image IMG]`, `chgame cart deploy FILE --card E:\` (card.md); `--device` picks the board (default `rev0`), as it does for `chgame cart flash` |
| Back from a card | `chgame cart backup E:\ OUT [--game ID ...]`: the card's games as a cart, with their SD files, from the record in each CHG file (card.md, "Backing up a card"). A `.chg` given to `new` or `add` is read the same way: with a record, the game comes back as its cart had it |
