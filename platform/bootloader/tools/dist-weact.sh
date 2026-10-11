#!/usr/bin/env bash
# Modified 2026-10-11: WeAct F8U6 hardware variant; see docs/weact-f8u6.md.
# Optional future build step. Not run when preparing the source-only mod.
# Builds WeAct images; never copies or renames the existing Rev0 binaries.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="$HERE/../board/arduino/CHGame/bootloaders/CHGame"
mkdir -p "$DEST"
for ui in list visual; do
  for style in rainbow static; do
    "$HERE/build.sh" release --board=weact-f8u6 "--ui=$ui" "--style=$style"
    build_dir="$HERE/build/weact-f8u6/release"
    base=chgame_weact_sdboot
    [ "$ui" = visual ] && { build_dir="$build_dir-visual"; base=chgame_weact_sdvisual; }
    [ "$style" = static ] && { build_dir="$build_dir-static"; base="${base}_static"; }
    cp "$build_dir/chgame_boot.bin" "$DEST/$base.bin"
  done
done
"$HERE/build.sh" nomenu --board=weact-f8u6
cp "$HERE/build/weact-f8u6/nomenu/chgame_boot.bin" "$DEST/chgame_weact_boot_nomenu.bin"
echo "WeAct bootloader images copied to $DEST"
