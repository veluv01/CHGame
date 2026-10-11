# F8U6 build verification

Validated on **2026-10-11** against upstream `e1f8d05940ddb7bbef89c13326da75c003f02904`.

**31/31 F8U6 firmware builds passed:** all 20 shared games, three SD applications, bringup, Hello, a CHFour debug build and five bootloader configurations. All images fit their configured flash, save-page and static RAM budgets.

Built on Linux x86_64 with Python 3.12, PlatformIO 6.1.19 and WCH GCC 8.2.0 (`rv32imacxw/ilp32`). The Linux compiler package is pinned to `e8e7ba93c9c1ea30366d98f28318d4ad09b369e2`.

| Image | Mode | Flash bytes | Flash budget | Static RAM bytes |
|---|---|---:|---:|---:|
| bringup | release | 9,152 | 50,944 | 11,620 |
| hello | release | 9,880 | 50,944 | 11,672 |
| game | release | 35,164 | 50,432 | 16,260 |
| game-debug | debug | 43,632 | 50,432 | 17,132 |
| sdreader | release | 42,016 | 50,432 | 16,868 |
| sdserial | release | 44,876 | 50,432 | 17,148 |
| stlviewer | release | 30,736 | 50,432 | 16,116 |
| bootloader | release | 11,912 | 12,032 | 18,384 |
| bootloader-visual | release | 12,076 | 12,096 | 18,192 |
| bootloader-usb | release | 5,272 | 12,032 | 1,656 |
| bootloader-static | release | 11,708 | 12,032 | 18,244 |
| bootloader-visual-static | release | 11,884 | 12,096 | 18,180 |
| CHBackgammon | release | 47,680 | 50,432 | 16,492 |
| CHBingo | release | 35,460 | 50,432 | 14,768 |
| CHBlackjack | release | 42,960 | 50,432 | 15,412 |
| CHBoardwalk | release | 47,948 | 50,432 | 14,892 |
| CHCheckers | release | 39,528 | 50,432 | 16,868 |
| CHChess | release | 46,712 | 50,432 | 17,320 |
| CHCraps | release | 48,308 | 50,432 | 15,424 |
| CHCrossword | release | 48,220 | 50,432 | 17,076 |
| CHDominoes | release | 41,612 | 50,432 | 16,712 |
| CHMahjong | release | 46,984 | 50,432 | 17,668 |
| CHPoker | release | 47,004 | 50,432 | 15,352 |
| CHRoulette | release | 46,980 | 50,432 | 15,912 |
| CHSlots | release | 45,584 | 50,432 | 14,700 |
| CHSnakes | release | 35,352 | 50,432 | 15,200 |
| CHSolitaire | release | 29,708 | 50,432 | 16,540 |
| CHTicTacToe | release | 48,520 | 50,432 | 14,508 |
| CHWordWheel | release | 47,292 | 50,432 | 14,848 |
| CHWords | release | 48,340 | 50,432 | 15,588 |
| CHYacht | release | 42,688 | 50,432 | 15,076 |

The static RAM budget is **18,416 bytes**, excluding the 16-byte retained boot block and 2,048-byte stack. Applications link at `0x3000`; flash budgets account for each sketch's save-page requirement. Bootloader flash budgets retain the existing 256-byte list/USB margin or 192-byte visual-menu margin.

## Compatibility and host checks

| Check | Result |
|---|---|
| Root and standalone PlatformIO entry points | Bringup, CHFour and list bootloader images match byte for byte; all carry WF86. |
| Application packages | 26 WF86 headers/payloads verified; all reject a Rev0 target check. |
| Bootloader images | Five carry WF86 and meet their flash/RAM gates. |
| Rev0 game regression | All 20 game images are byte-identical to the upstream baseline using a temporary Rev0 application profile derived from the same adapter. This comparison did not use Arduino CLI. |
| Game simulator | 20/20 deterministic samples match upstream: 24 frames per run, every fourth frame sampled, A/START events; 120 matching frame pairs. |
| Source-profile tests | Six passed, including changed-memory-layout rejection and SD-app configuration preservation. |
| Python uploader | 22 protocol, CLI and layout tests passed. |
| Cart tools | 33 passed; one optional JSON-schema test skipped. |
| Native bootloader | 1,304 checks passed; 78 existing pinned frames match, with preview/picture parity checks also passing. |
| SD-card export | 23 WF86 programs and 50 files; FAT32 image and individual CHFour cart generated successfully. |
| Arduino archive | Current shared Python host and five WeAct bootloaders included; installed-platform wrapper fallback loads. |
| Rev0 bootloader builds | List 11,928 bytes; visual 12,068 bytes. Both fit their existing margins. |

Simulator comparisons used `CHSIM_FLAGS="-include chrono"` on both source trees to isolate the upstream Arduino `min`/`max` macro issue under the host GNU C++ compiler. These samples are not exhaustive gameplay tests.

## Related fixes

- The simulator excludes PlatformIO-generated `.ino.cpp` translation units to avoid compiling a sketch twice.
- The bootloader stops reading a short title at NUL; the native sanitizer suite exposed an over-read in the upstream baseline. Existing pinned menu frames remain unchanged.
- The frame harness treats visual `v_real_*` frames as dependent on the same optional generated card fixture as `real_*`; other missing frames still fail.
- Unique local-platform aliases prevent multiple checkouts from sharing an incorrect PlatformIO link. Target identity and independent entry-point comparisons guard the resulting images.

## Reproduce

```sh
python -m pip install -r ports/weact-f8u6/requirements.txt
python -m pip install -e .
python ports/weact-f8u6/check.py
python -m unittest discover -s ports/weact-f8u6/tests
python ports/weact-f8u6/build_all.py --install-bootloaders
pio run -d ports/weact-f8u6 -e bringup -e game -e bootloader
python -m unittest discover -s platform/bootloader/test/protocol
python -m unittest discover -s tools/chcart/tests
python platform/bootloader/test/native/run_tests.py --quick
python tools/chgame.py card --device weact-f8u6 --no-build
```

Firmware, logs and the build runner's JSON results go under `ports/weact-f8u6/build/verification/`. The [detailed report](weact-f8u6-builds.json) records image hashes, settings, memory sizes and regression checks. GitHub Actions repeats the configured firmware/host checks and retains artifacts for 30 days when workflows are enabled.

## Verification limits

No physical hardware was connected, flashed or measured during this update. Other host operating systems/toolchain packages, Go compiler tests and complete Arduino CLI release staging were not exercised. The optional generated real-card fixture was absent, and the optional JSON-schema dependency was not installed. Electrical wiring, display initialization, SD signal integrity and USB operation require checks on the assembled hardware.
