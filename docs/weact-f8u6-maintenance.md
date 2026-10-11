# Maintaining the F8U6 variant

The variant is integrated into the repository. Merge upstream changes
through Git and rebuild from the merged sources. No overlay installer or
independent framework copy is required.

## Source ownership

| Component | Location | Maintenance rule |
|---|---|---|
| Board properties | `platform/board/arduino/CHGame/boards.txt` | Keep the separate `weact-f8u6` entry. |
| Physical pin map | `variants/CH32X035/CHGame_WeActF8U6/` under the Arduino platform | Review against the F8U6 package and board revision. |
| Input, silent audio and START exit | Shared core/CHGame library | Keep F8U6 behavior conditional on `ARDUINO_CHGAME_WEACT_F8U6`. |
| Bootloader pins and target | `platform/bootloader/src/hal.h`, `build.sh` | Select pins and WF86 identity together. |
| Device registries | `spec/chgame.md`, `tools/chcart/model.py`, `tools/chgpack.py`, `shared/chg_format.h`, Python/Go hosts | Names, IDs and memory limits must agree. |
| PlatformIO | `platformio.ini`, `ports/weact-f8u6/` | Consume the current core, libraries, board settings and bootloader source lists. |
| Arduino uploader | `tools/weact_upload.py` under the Arduino platform | Import the checkout's shared Python host; release packaging bundles that same source. |
| Generated bootloader binaries | Arduino platform `bootloaders/CHGame/chgame_weact_*.bin` | Rebuild from current source before distributing a board archive. |

Rev0 retains its target and defaults. Do not reuse reserved CHGame revision
IDs for development boards, rename a Rev0 binary to imply compatibility,
or copy older shared core/library files over an upstream update.

## Merge upstream

With local work committed and the working tree clean:

```sh
# Add the upstream remote once, if it is not already configured.
git remote add upstream https://github.com/bateske/CHGame.git
git fetch upstream
git merge upstream/main
python ports/weact-f8u6/check.py
python -m unittest discover -s ports/weact-f8u6/tests
python ports/weact-f8u6/build_all.py --install-bootloaders
```

Resolve Git conflicts before building. The checker rejects incompatible
changes to the linker layout, audio API, debug reservation or device
registration. The bootloader builder reads `build.sh` source lists and
rejects unrecognized list syntax instead of omitting new source files.

Ordinary game/library changes flow through the shared sources. A future
change to those contracts may need a variant update; compatibility with
unknown future releases is not guaranteed.

## Verification before merging or releasing

Run the project-wide rules in [CLAUDE.md](../CLAUDE.md) as well as:

```sh
python -m unittest discover -s platform/bootloader/test/protocol
python -m unittest discover -s tools/chcart/tests
python platform/bootloader/test/native/run_tests.py --quick
pio run -d ports/weact-f8u6 -e bringup -e game -e bootloader
```

`build_all.py` discovers shared games dynamically and verifies each
application's flash, static RAM, save-page budget and WF86 package. It
builds all twelve named environments plus the remaining shared games.
`--smoke` limits the run to named environments; it is not the full release
matrix. `--install-bootloaders` refreshes the five tracked WeAct binaries
without accessing hardware.

For shared library/core changes, also rebuild Rev0 games and compare
deterministic simulator frames. For bootloader changes, validate both menu
faces and preserve pinned frames unless the change intentionally affects
them. Keep the [verification report](weact-f8u6-builds.md) current and
separate measured compiler results from physical hardware checks.

The `WeAct F8U6 builds` GitHub Actions workflow runs firmware, source-profile,
uploader, cart and native bootloader checks and retains artifacts for 30
days. Forks may require workflows to be enabled in the repository's
**Actions** tab. CI never flashes hardware. The upstream API-documentation
workflow builds on forks; its Pages deployment is restricted to the
upstream repository.

## Arduino board archive

PlatformIO uses the checkout directly. Arduino IDE/CLI uses its installed
board package, which must be built from this fork to include F8U6.
The upstream Boards Manager package remains a Rev0 release.

Install the standard CHGame package first for the compiler/ISP dependencies,
then follow [local release staging](../platform/board/docs/trying-a-release.md).
Before staging:

```sh
python ports/weact-f8u6/build_all.py --install-bootloaders
python tools/release/stage.py --quick
```

The release packager adds the current Python uploader under
`tools/weact_python/` inside the archive. That generated copy is not checked
into the repository, avoiding a second host implementation that can drift.
The installed F8U6 board invokes `python3` on Linux/macOS or `py -3` on
Windows; install `pyserial` with the same Python interpreter. Select
**CHGame WeAct F8U6** in the board menu.

The archive includes five F8U6 bootloaders. A bootloader source change
requires rebuilding them; copying a raw checkout platform folder into an
IDE installation is insufficient because it omits the generated uploader.
Release URLs/versions must identify the fork's own release channel; do not
overwrite upstream release metadata. See [board-package release
documentation](../platform/board/docs/building.md) for distribution details.

## License and provenance

The integration retains upstream license and attribution files. The
PlatformIO platform follows the repository's Apache-2.0 license; the board
package, bootloader and included libraries retain the licenses documented
in [the root README](../README.md#-licences). The validated upstream base
is `e1f8d05940ddb7bbef89c13326da75c003f02904`; recorded compiler results are
in [the verification report](weact-f8u6-builds.md).
