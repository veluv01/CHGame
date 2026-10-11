#!/usr/bin/env python3
# Modified 2026-10-11: WeAct F8U6 hardware variant; see docs/weact-f8u6.md.
"""Python upload/CHG pack host for the WeAct F8U6 board target."""
import sys
from pathlib import Path

if sys.version_info < (3, 10):
    raise SystemExit("WeAct uploader needs Python 3.10 or newer")
location = Path(__file__).resolve()
checkout_host = location.parents[5] / "platform/bootloader/host/py"
# A repository checkout always uses its current upstream host. The bundled
# source copy is only for Arduino IDE's separately installed platform.
host = checkout_host if (checkout_host / "chgame_upload/cli.py").is_file() else location.parent / "weact_python"
if not (host / "chgame_upload/cli.py").is_file():
    raise SystemExit("Uploader source is missing. Use a CHGame checkout or a board archive built with tools/release/make_package.py.")
sys.path.insert(0, str(host))
try:
    from chgame_upload.cli import main
except ModuleNotFoundError as exc:
    if exc.name == "serial":
        raise SystemExit("Install pyserial for this Python: python3 -m pip install pyserial") from None
    raise

if __name__ == "__main__":
    raise SystemExit(main(default_device="weact-f8u6"))
