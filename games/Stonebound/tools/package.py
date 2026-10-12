#!/usr/bin/env python3
"""Package the actual WF86 release and enforce flash/RAM/save-page budgets."""
import hashlib
import json
from pathlib import Path
import shutil
import sys
import zipfile
from elftools.elf.elffile import ELFFile
GAME=Path(__file__).resolve().parents[1]
ROOT=GAME.parents[1]
sys.path.insert(0,str(ROOT/"tools"))
import chgpack
VERSION="0.1.0"
OUT=GAME/"out"/"release"
OUT.mkdir(parents=True,exist_ok=True)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

profiles={}
for name in ("stonebound","stonebound-debug"):
    folder=GAME/".pio"/"build"/name
    metadata=json.loads((folder/"chgame-build.json").read_text())
    payload=(folder/"firmware.bin").read_bytes()
    if metadata["target_id"]!=0x36384657 or metadata["board"]!="weact-f8u6":
        raise SystemExit("wrong board identity")
    if metadata["app_origin"]!=0x3000 or metadata["save_pages"]!=2:
        raise SystemExit("application/save layout changed")
    if len(payload)>50432:
        raise SystemExit("firmware overlaps the two save pages")
    with (folder/"firmware.elf").open("rb") as stream:
        elf=ELFFile(stream)
        ram=sum(section["sh_size"] for section in elf.iter_sections()
                if section["sh_flags"]&2 and 0x20000010<=section["sh_addr"]<0x20005000
                and section.name!=".stack")
    if ram>18416:
        raise SystemExit("static RAM exceeds the 18416-byte budget")
    # Independently parse the builder's package, including CRC and target.
    raw=(folder/"firmware.chg").read_bytes()
    parsed=chgpack.parse(raw,target=chgpack.TARGET_WEACT_F8U6)
    if parsed["payload_bytes"]!=len(payload) and parsed["payload_bytes"]!=(len(payload)+3)//4*4:
        raise SystemExit("builder package length mismatch")
    profiles[name]=dict(flash_bytes=len(payload),flash_budget=50432,static_ram_bytes=ram,
                       static_ram_budget=18416,save_pages=2,
                       sha256=sha(folder/"firmware.bin"))
release=GAME/".pio"/"build"/"stonebound"/"firmware.bin"
picture=chgpack.picture_file(str(GAME/"docs"/"cart.png"))
package=chgpack.pack(release.read_bytes(),"STONEBOUND",version=VERSION,
                    app_version=0x000100,picture=picture,target=chgpack.TARGET_WEACT_F8U6)
info=chgpack.parse(package,target=chgpack.TARGET_WEACT_F8U6)
try:
    chgpack.parse(package,target=chgpack.TARGET_REV0)
except chgpack.ChgError:
    pass
else:
    raise SystemExit("WF86 package accepted as Rev0")
dest=OUT/"sdcard"/"GAMES"
dest.mkdir(parents=True,exist_ok=True)
(dest/"STONE.CHG").write_bytes(package)
shutil.copy2(release,OUT/"Stonebound-WeAct.bin")
report=dict(game="Stonebound",version=VERSION,device="weact-f8u6",target="WF86",
            hardware_tested=False,profiles=profiles,chg_sha256=sha(dest/"STONE.CHG"),
            package=info)
(OUT/"BUILD_REPORT.json").write_text(json.dumps(report,indent=2)+"\n")
(OUT/"BUILD_REPORT.md").write_text(
    "# Stonebound build verification\n\n"
    "Device: WeAct CH32X035F8U6 / WF86. Application begins at 0x3000.\n\n"
    "| Profile | Flash bytes | Static RAM bytes |\n|---|---:|---:|\n"+
    "".join("| "+k+" | "+str(v["flash_bytes"])+" | "+str(v["static_ram_bytes"])+" |\n"
            for k,v in profiles.items())+
    "\nBoth profiles fit 50,432 bytes of flash and 18,416 bytes of static RAM, "
    "with two flash save pages and a 2,048-byte stack reserved. "
    "The packager validates CRCs, target IDs and rejection by the Rev0 target. "
    "Physical hardware was not flashed or measured.\n")
src=OUT/"source"/"Stonebound"
for path in GAME.rglob("*"):
    if not path.is_file():
        continue
    rel=path.relative_to(GAME)
    if any(part in (".pio","out","build","__pycache__") for part in rel.parts):
        continue
    target=src/rel
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(path,target)
shutil.copy2(GAME/"INSTALL.md",OUT/"INSTALL.md")
shutil.copy2(GAME/"LICENSE",OUT/"LICENSE")
archive=GAME/"out"/("Stonebound-WeAct-"+VERSION+".zip")
with zipfile.ZipFile(archive,"w",compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for path in sorted(OUT.rglob("*")):
        if path.is_file():
            z.write(path,path.relative_to(OUT).as_posix())
print("stonebound PACKAGE "+json.dumps(report,separators=(",",":")))
print("stonebound release "+str(archive))
