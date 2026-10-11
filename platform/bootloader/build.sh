#!/usr/bin/env bash
# Modified 2026-10-11: WeAct F8U6 hardware variant; see docs/weact-f8u6.md.
# Build the CHGame bootloader (Linux, macOS, or Git Bash on Windows).
#
#   ./build.sh [MODE] [--ui=list|visual] [--style=rainbow|static] [--nolto]
#
# MODE
#   release   the SD game menu bootloader, with the developer self-update
#             (DEV_UNLOCK/DEV_WRITE_BOOT) that lets a later bootloader be
#             installed over USB, as the 0.2.4 bootloader allows (default)
#   locked    release without self-update: bootloader changes then need the
#             BOOT button and the factory ISP (production option)
#   nomenu    no SD menu: the old boot decision on the new update path (HW2a)
#   app       the menu as an ordinary program at 0x3000 (dry run, no USB and no
#             flash writes) for testing the card and the panel under ANY
#             bootloader
# --ui        the menu's face: list (the default), the text list over the
#             card's MENU.BG (src/menu.c); or visual, one picture at a time and
#             no text (src/visual.c, docs/visual-menu.md)
# --style     the menu's colour 15 (the selection bar, the boxes, #FF00FF in
#             the card's pictures): rainbow (the default), one colour turning
#             through the colour wheel; or static
# --board     rev0 (default), or weact-f8u6 (its pins and WF86 target id)
# --nolto     build without LTO, for a per-object size breakdown
# --roomy     link against a 16 KB copy of the script, to measure a build that
#             does not fit (never flash it)
#
# CHBOOT_BOARD_TARGET (environment): the board's target id for a board after
# rev0 (shared/chg_format.h), e.g. 0x31524743 for rev1. Unset is rev0, and
# gives the binaries rev0 has always had. Until a later board's pins are in
# the source this only measures what its id costs: never flash such a build
# on a rev0 (docs/hardware-revisions.md).
#
# Output: build/<MODE>[-visual][-static][-board<id>]/chgame_boot.{elf,bin,map,lst}
# and a size report.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODE=release
LTO=-flto
STYLE=rainbow
UI=list
BOARD_NAME=rev0
ROOMY=
for a in "$@"; do
  case "$a" in
    release|locked|nomenu|app) MODE="$a" ;;
    --nolto) LTO= ;;
    --roomy) ROOMY=1 ;;
    --style=rainbow|--style=static) STYLE="${a#--style=}" ;;
    --ui=list|--ui=visual) UI="${a#--ui=}" ;;
    --board=rev0|--board=weact-f8u6) BOARD_NAME="${a#--board=}" ;;
    *) echo "unknown argument: $a" >&2; exit 1 ;;
  esac
done
BOARD="${CHBOOT_BOARD_TARGET:-}"
BOARD_DEFS=
if [ "$BOARD_NAME" = weact-f8u6 ]; then
  if [ -n "$BOARD" ] && [ "$BOARD" != 0x36384657 ]; then
    echo "weact-f8u6 requires CHBOOT_BOARD_TARGET=0x36384657 (or unset it)" >&2
    exit 1
  fi
  BOARD=0x36384657
  BOARD_DEFS="-DCHBOOT_WEACT_F8U6=1 -DCH32V_LOCK_DEBUG"
  OUT="$HERE/build/weact-f8u6/$MODE"
else
  if [ -n "$BOARD" ] && [ "$((BOARD))" -eq "$((0x36384657))" ]; then
    echo "WF86 requires --board=weact-f8u6 so its pin map is selected too" >&2
    exit 1
  fi
  OUT="$HERE/build/$MODE"
fi
[ "$UI" != list ] && OUT="$OUT-$UI"
[ "$STYLE" != rainbow ] && OUT="$OUT-$STYLE"
[ -z "$LTO" ] && OUT="$OUT-nolto"
[ -n "$ROOMY" ] && OUT="$OUT-roomy"
[ "$BOARD_NAME" = rev0 ] && [ -n "$BOARD" ] && OUT="$OUT-board$BOARD"

# The toolchain ships with the CHGame board package (arduino-cli core install
# CHGame:ch32v); it is found in the usual Arduino data folders. Override with
# CHGAME_TOOLCHAIN=<dir containing riscv-none-embed-gcc>.
TC="${CHGAME_TOOLCHAIN:-}"
if [ -z "$TC" ]; then
  for d in "$HOME/.arduino15" "$HOME/Library/Arduino15" "${LOCALAPPDATA:-/nonexistent}/Arduino15"; do
    for t in "$d"/packages/CHGame/tools/riscv-none-embed-gcc/*/bin "$d"/packages/CH32_Arduino/tools/riscv-none-embed-gcc/*/bin; do
      [ -d "$t" ] && TC="$t"
    done
  done
fi
CC="$TC/riscv-none-embed-gcc"
OBJCOPY="$TC/riscv-none-embed-objcopy"
OBJDUMP="$TC/riscv-none-embed-objdump"
SIZE="$TC/riscv-none-embed-size"
[ -x "$CC" ] || [ -x "$CC.exe" ] || { echo "toolchain not found (set CHGAME_TOOLCHAIN)" >&2; exit 1; }

SPL="$HERE/vendor/spl"
SRC="$HERE/src"
USB="$HERE/vendor/usbcdc"
SHARED="$HERE/shared"

SELFUPDATE=1; MENU=1; APPDEF=; LD="$HERE/ld/link_boot.ld"
case "$MODE" in
  locked) SELFUPDATE=0 ;;
  nomenu) MENU=0 ;;
  app)    APPDEF="-DCHBOOT_APP=1"; LD="$HERE/ld/link_app.ld" ;;
esac

ARCH="-march=rv32imacxw -mabi=ilp32"
IMAGE_ID=1; [ "$MODE" = app ] && IMAGE_ID=2   # the fault blink tells whose handler caught it
DEFS="-DCH32X035 -DSYSCLK_FREQ_48MHz_HSI=48000000 -DF_CPU=48000000 -DCHGAME_IMAGE_ID=$IMAGE_ID"
DEFS="$DEFS -DCHGAME_ALLOW_SELFUPDATE=$SELFUPDATE -DCHBOOT_MENU=$MENU $APPDEF -DMENU_STYLE=MENU_STYLE_$(echo "$STYLE" | tr a-z A-Z)"
DEFS="$DEFS -DMENU_UI=MENU_UI_$(echo "$UI" | tr a-z A-Z) $BOARD_DEFS"
# Defines reach the startup file too, which puts the board id in the image.
[ -n "$BOARD" ] && DEFS="$DEFS -DCHGAME_BOARD_TARGET=$BOARD"
INC="-I$SHARED -I$SRC -I$USB -I$SPL -I$SPL/Core -I$SPL/Peripheral/inc"
WARN="-Wall -Wextra -Wundef -Werror=implicit-function-declaration"
# The visual menu fits gate A with five more size flags (SIZES.md, "Visual:
# sideways slides, faster card reads", "paced animations", "the border"):
# together about 144 B; the pixel and card loops compile the same with or
# without them.
UIOPT=""
[ "$UI" = visual ] && UIOPT="-fno-guess-branch-probability -fno-shrink-wrap -fno-tree-scev-cprop -fno-caller-saves -fno-tree-vrp"
OPT="-Os $LTO -ffunction-sections -fdata-sections -fno-common -msmall-data-limit=8 -msave-restore -fno-jump-tables $UIOPT ${CHBOOT_EXTRA_CFLAGS:-}"
CFLAGS="$ARCH $DEFS $INC $WARN $OPT -std=gnu11 -g"

CSRC=(
  "$SRC/main.c" "$SRC/boot.c" "$SRC/bootreq.c" "$SRC/appmeta.c" "$SRC/crc32.c"
  "$SRC/sys.c" "$SRC/jump.c" "$SRC/startup_glue.c" "$SRC/flash.c"
  "$SPL/system_ch32x035.c" "$SPL/Peripheral/src/ch32x035_misc.c"
)
if [ "$MODE" != app ]; then
  CSRC+=( "$SRC/update.c" "$SRC/crc16.c" "$SRC/proto.c" "$SRC/usb.c"
          "$USB/wch_usbcdc_cdc.c" "$USB/wch_usbcdc_descr.c" "$USB/wch_usbcdc_handler.c" )
fi
if [ "$MENU" = 1 ]; then
  FACE="$SRC/menu.c"; [ "$UI" = visual ] && FACE="$SRC/visual.c"
  CSRC+=( "$SRC/sd.c" "$SRC/fat.c" "$SRC/chg.c" "$SRC/install.c" "$SRC/lcd.c" "$SRC/card.c" "$FACE" )
fi
ASRC=( "$SRC/startup_chgame_boot.S" )

rm -rf "$OUT"; mkdir -p "$OUT/obj"
if [ -z "$LTO" ] || [ -n "$ROOMY" ]; then
  # Size analysis only: without LTO the image does not fit, so link against a
  # roomier copy of the script. Never flash a --nolto or --roomy build.
  sed -e 's/LENGTH = 12288/LENGTH = 16384/' -e 's/_etext <= 0x3000/_etext <= 0x4000/' "$LD" > "$OUT/analysis.ld"
  LD="$OUT/analysis.ld"
fi
OBJS=()
for f in "${ASRC[@]}"; do
  o="$OUT/obj/$(basename "$f").o"; OBJS+=("$o")
  "$CC" $ARCH $DEFS -I"$SPL/Startup" -x assembler-with-cpp -c "$f" -o "$o"
done
for f in "${CSRC[@]}"; do
  o="$OUT/obj/$(basename "$f").o"; OBJS+=("$o")
  "$CC" $CFLAGS -c "$f" -o "$o"
done

"$CC" $ARCH $OPT -T "$LD" -nostartfiles -Xlinker --gc-sections \
      --specs=nano.specs --specs=nosys.specs \
      -Wl,-Map,"$OUT/chgame_boot.map" -o "$OUT/chgame_boot.elf" "${OBJS[@]}"
"$OBJCOPY" -O binary "$OUT/chgame_boot.elf" "$OUT/chgame_boot.bin"
"$OBJDUMP" -d -S "$OUT/chgame_boot.elf" > "$OUT/chgame_boot.lst"

# Gate A keeps 256 B spare; the visual menu, by the owner's choice
# (2026-10-03, its key map), 192 B (SIZES.md).
echo "== $MODE$([ "$UI" != list ] && echo " $UI")$([ "$STYLE" != rainbow ] && echo " $STYLE")$([ -z "$LTO" ] && echo " (no LTO)")"
python3 "$HERE/tools/size_report.py" "$OUT/chgame_boot.elf" --size-tool "$SIZE" \
        ${LTO:+} $([ -z "$LTO" ] && echo --objects) \
        $([ "$MODE" = app ] && echo --margin -999999)         $([ "$MODE" != app ] && [ "$UI" = visual ] && echo --margin 192)
