#!/usr/bin/env python3
"""Check the source/profile contracts only. Does not compile or access hardware."""
import argparse
from pathlib import Path
import sys

sys.dont_write_bytecode = True

project = Path(__file__).resolve().parent
sys.path.insert(0, str(project / "platform/builder"))
from chgame_profile import application, boot_sources, find_repo, validate_sources


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sketch", help="also inspect a game's board settings")
    args = parser.parse_args()
    repo = find_repo(project)
    validate_sources(repo)
    for mode in ("release", "locked", "nomenu"):
        for menu in ("list", "visual"):
            boot_sources(repo, mode, menu)
    options = {"custom_chgame_sketch": args.sketch or "", "custom_chgame_save_pages": "auto" if args.sketch else "0"}
    app = application(repo, project, lambda key, default=None: options.get(key, default), environ={})
    print("Source contracts OK. Sketch:", app["name"], "Options:", app["options"],
          "Save pages:", app["save_pages"], "Defines:", app["defines"])
    print("No builds or hardware actions performed.")


if __name__ == "__main__":
    main()
