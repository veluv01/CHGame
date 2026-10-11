#!/usr/bin/env python3
"""Build the WeAct environments and every current upstream game; never flash.

Run with PlatformIO's Python: python build_all.py [--smoke]
Logs, firmware and a machine-readable size report go in build/verification/.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from elftools.elf.elffile import ELFFile

PROJECT = Path(__file__).resolve().parent
REPO = PROJECT.parents[1]
GAMES = REPO / "platform/board/arduino/CHGame/libraries/CHGame/examples/Games"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--smoke", action="store_true", help="only the named environments")
    parser.add_argument("--project-dir", type=Path, default=REPO,
                        help="PlatformIO entry point (default: repository root)")
    parser.add_argument("--install-bootloaders", action="store_true",
                        help="refresh the five WeAct binaries in the Arduino board package")
    args = parser.parse_args()
    jobs = [(name, None) for name in ("bringup", "hello", "game", "game-debug", "sdreader", "sdserial", "stlviewer",
                                     "bootloader", "bootloader-visual", "bootloader-usb",
                                     "bootloader-static", "bootloader-visual-static")]
    if not args.smoke:
        jobs += [("game", game.name) for game in sorted(GAMES.iterdir())
                 if game.name != "CHFour" and (game / (game.name + ".ino")).is_file()]
    out = PROJECT / "build/verification"
    out.mkdir(parents=True, exist_ok=True)
    reports = []
    for environment, sketch in jobs:
        label = sketch or environment
        destination = out / label
        destination.mkdir(exist_ok=True)
        child_env = dict(os.environ)
        child_env.pop("CHGAME_SKETCH", None)
        if sketch:
            child_env["CHGAME_SKETCH"] = sketch
        command = [sys.executable, "-m", "platformio", "run", "-d", str(args.project_dir.resolve()), "-e", environment]
        with (destination / "build.log").open("w") as log:
            result = subprocess.run(command, env=child_env, stdout=log, stderr=subprocess.STDOUT)
        report = {"environment": environment, "selection": sketch, "passed": result.returncode == 0}
        if report["passed"]:
            build = args.project_dir.resolve() / ".pio/build" / environment
            profile = json.loads((build / "chgame-build.json").read_text())
            report.update(profile)
            with (build / "firmware.elf").open("rb") as handle:
                elf = ELFFile(handle)
                report["static_ram_bytes"] = sum(section["sh_size"] for section in elf.iter_sections()
                    if section["sh_flags"] & 2 and 0x20000010 <= section["sh_addr"] < 0x20005000
                    and section.name != ".stack")
            report["ram_budget"] = 18416
            report["ram_with_retained_and_stack"] = report["static_ram_bytes"] + 16 + 2048
            report["passed"] = (profile["target_id"] == 0x36384657
                and profile["flash_bytes"] <= profile["flash_budget"]
                and report["static_ram_bytes"] <= report["ram_budget"])
            report["sha256"] = {}
            extensions = ("bin", "elf", "map", "chg") if profile["image_kind"] == "app" else ("bin", "elf", "map")
            for extension in extensions:
                artifact = build / ("firmware." + extension)
                shutil.copy2(artifact, destination / artifact.name)
                report["sha256"][artifact.name] = hashlib.sha256(artifact.read_bytes()).hexdigest()
            shutil.copy2(build / "chgame-build.json", destination)
            if args.install_bootloaders and report["passed"] and profile["image_kind"] == "bootloader":
                filename = {"bootloader": "chgame_weact_sdboot.bin",
                            "bootloader-visual": "chgame_weact_sdvisual.bin",
                            "bootloader-usb": "chgame_weact_boot_nomenu.bin",
                            "bootloader-static": "chgame_weact_sdboot_static.bin",
                            "bootloader-visual-static": "chgame_weact_sdvisual_static.bin"}[environment]
                bootloaders = REPO / "platform/board/arduino/CHGame/bootloaders/CHGame"
                installed = bootloaders / filename
                shutil.copy2(build / "firmware.bin", installed)
                installed.chmod(0o644)
        reports.append(report)
        status = "PASS" if report["passed"] else "FAIL"
        print(f"{label:20} {status}" +
              (f" {report['flash_bytes']:5}/{report['flash_budget']} flash, "
               f"{report['static_ram_bytes']:5}/{report['ram_budget']} static RAM" if "flash_bytes" in report else ""),
              flush=True)
    report = {"utc": datetime.now(timezone.utc).isoformat(), "board": "weact-f8u6",
              "hardware_tested": False, "results": reports}
    (out / "results.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{sum(item['passed'] for item in reports)}/{len(reports)} builds passed; {out}")
    return 0 if all(item["passed"] for item in reports) else 1


if __name__ == "__main__":
    sys.exit(main())
