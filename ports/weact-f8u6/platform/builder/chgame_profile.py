"""Read the current checkout's board/sketch settings without building anything."""
import importlib.util
import os
from pathlib import Path
import re
import shlex
import sys

TARGET = "weact-f8u6"
TARGET_ID = 0x36384657
BOARD_PATH = Path("platform/board/arduino/CHGame")


def find_repo(project):
    for candidate in (Path(project).resolve(), *Path(project).resolve().parents):
        if (candidate / BOARD_PATH / "boards.txt").is_file():
            return candidate
    raise ValueError("Run this platform inside a CHGame repository checkout")


def properties(path):
    result = {}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            key, value = line.split("=", 1)
            result[key.strip()] = value.strip()
    return result


def board_settings(repo, options):
    all_props = properties(repo / BOARD_PATH / "boards.txt")
    prefix = TARGET + "."
    props = {k[len(prefix):]: v for k, v in all_props.items() if k.startswith(prefix)}
    if props.get("build.board") != "CHGAME_WEACT_F8U6":
        raise ValueError("WeAct Arduino board definition is absent or has changed")
    for menu, choice in options.items():
        key = "menu." + menu + "." + choice
        if key not in props:
            raise ValueError("Unknown WeAct board option: " + menu + "=" + choice)
        p = key + "."
        props.update({k[len(p):]: v for k, v in list(props.items()) if k.startswith(p)})
    if props.get("build.variant") != "CH32X035/CHGame_WeActF8U6":
        raise ValueError("Unexpected WeAct variant path")
    return props


def find_sketch(repo, project, name):
    if not name:
        return repo / "ports/weact-f8u6/src"
    direct = Path(name).expanduser()
    candidates = [direct] if direct.is_absolute() else [Path(project) / direct, repo / direct]
    examples = repo / BOARD_PATH / "libraries/CHGame/examples"
    candidates.append(examples / name)
    candidates += [examples / kind / name for kind in ("Games", "Apps", "Graphics")]
    for path in candidates:
        path = path.resolve()
        if path.is_file() and path.suffix == ".ino":
            path = path.parent
        if path.is_dir() and (path / (path.name + ".ino")).is_file():
            return path
    raise ValueError("CHGame sketch not found: " + name)


def game_config(repo, sketch):
    sys.path.insert(0, str(repo / "tools"))
    spec = importlib.util.spec_from_file_location("weact_gamecfg", repo / "tools/gamecfg.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.load(sketch)


def application(repo, project, get_option, environ=None):
    environ = os.environ if environ is None else environ
    name = environ.get("CHGAME_SKETCH", get_option("custom_chgame_sketch", ""))
    sketch = find_sketch(repo, project, name)
    cfg = game_config(repo, sketch)
    opts = {"opt": "oslto", "rtlib": "nano", "periph": "game", "usb": "uploadonly" if name else "serial"}
    if cfg.FQBN:
        # A sketch's FQBN uses Arduino's menu defaults when it omits an option.
        opts["usb"] = "serial"
        parts = cfg.FQBN.split(":", 3)
        if len(parts) > 3:
            opts.update(dict(pair.split("=", 1) for pair in parts[3].split(",") if pair))
    for menu in ("opt", "rtlib", "usb"):
        choice = get_option("custom_chgame_" + menu, "auto")
        if choice != "auto":
            opts[menu] = choice
    if opts.get("periph") != "game":
        raise ValueError("This wiring reserves UART/PWM pins; periph=full is incompatible")
    pages = get_option("custom_chgame_save_pages", "auto")
    pages = int(cfg.SAVE_PAGES if pages == "auto" else pages)
    if pages not in (0, 1, 2):
        raise ValueError("custom_chgame_save_pages must be 0, 1 or 2")
    return {"sketch": sketch, "name": sketch.name if name else "WeActBringup",
            "board": board_settings(repo, opts), "defines": list(cfg.DEFINES),
            "save_pages": pages, "external_sketch": bool(name), "options": opts}


def words(value):
    return shlex.split(value)


def boot_sources(repo, mode, menu):
    """Use build.sh's source arrays, including additions in future releases."""
    boot = repo / "platform/bootloader"
    text = (boot / "build.sh").read_text(encoding="utf-8")
    mapping = {"SRC": boot / "src", "SPL": boot / "vendor/spl",
               "USB": boot / "vendor/usbcdc"}
    face = boot / "src" / ("visual.c" if menu == "visual" else "menu.c")

    def expand(body):
        result = []
        for word in words(body):
            if word == "$FACE":
                result.append(face)
                continue
            match = re.fullmatch(r"\$(SRC|SPL|USB)/(.+)", word)
            if not match:
                raise ValueError("Bootloader source expression changed: " + word)
            result.append(mapping[match[1]] / match[2])
        return result

    initial = re.search(r"\bCSRC=\((.*?)\)", text, re.S)
    additions = re.findall(r"CSRC\+=\((.*?)\)", text, re.S)
    assembly = re.search(r"\bASRC=\((.*?)\)", text, re.S)
    if not initial or not assembly or len(additions) != 2:
        raise ValueError("Bootloader build.sh source selection changed; review PlatformIO integration")
    # Both release/locked/nomenu include USB; nomenu omits only the menu array.
    result = expand(initial[1]) + expand(additions[0]) + expand(assembly[1])
    if mode != "nomenu":
        result += expand(additions[1])
    for source in result:
        if not source.is_file():
            raise ValueError("Missing bootloader source: " + str(source))
    return result


def validate_sources(repo, read_bytes=None):
    """Reject a changed memory map or missing integration before writes/builds."""
    def read(relative):
        data = read_bytes(relative) if read_bytes else (repo / relative).read_bytes()
        return data.decode("utf-8", errors="replace")

    core = str(BOARD_PATH / "cores/arduino/ch32")
    ld = read(str(BOARD_PATH / "system/CH32X035/SRC/Ld/link_chgame_app.ld"))
    if not re.search(r"FLASH\s*\(rx\)\s*:\s*ORIGIN\s*=\s*0x00003000\s*,\s*LENGTH\s*=\s*50944", ld):
        raise ValueError("Application flash layout changed; review the WeAct port before applying")
    if not re.search(r"RAM\s*\(xrw\)\s*:\s*ORIGIN\s*=\s*0x20000010\s*,\s*LENGTH\s*=\s*20464", ld):
        raise ValueError("Application SRAM layout changed; review the WeAct port")
    for relative, token in (
        (str(BOARD_PATH / "variants/CH32X035/PinAF_CH32X035.h"), "CH32V_LOCK_DEBUG"),
        (core + "/chgame_boot.c", "ARDUINO_CHGAME_WEACT_F8U6"),
        (str(BOARD_PATH / "libraries/CHGame/src/chgame/Input.cpp"), "ARDUINO_CHGAME_WEACT_F8U6"),
        (str(BOARD_PATH / "libraries/CHGame/src/chgame/Audio.cpp"), "ARDUINO_CHGAME_WEACT_F8U6"),
        ("platform/bootloader/src/hal.h", "CHBOOT_WEACT_F8U6"),
        ("platform/bootloader/shared/chg_format.h", "CHG_TARGET_WEACT_F8U6"),
    ):
        if token not in read(relative):
            raise ValueError("Missing WeAct source contract: " + relative + " / " + token)
    registries = [
        "platform/bootloader/host/py/chgame_upload/chg.py", "tools/chgpack.py", "tools/chcart/model.py",
    ]
    for relative in registries:
        text = read(relative)
        if TARGET not in text or "0x36384657" not in text:
            raise ValueError("WeAct device registry changed: " + relative)
    audio = str(BOARD_PATH / "libraries/CHGame/src/chgame/Audio")
    header = read(audio + ".h")
    source = read(audio + ".cpp")
    start = source.index("#elif defined(ARDUINO_CHGAME_WEACT_F8U6)")
    quiet = source[start:source.index("\n#else", start)]

    def signatures(text):
        return sorted((result, name, 0 if not args.strip() else args.count(",") + 1)
                      for result, name, args in re.findall(
                          r"^(void|bool|u?int\w*)\s+(\w+)\s*\(([^)]*)\)", text, re.M))

    if signatures(header) != signatures(quiet):
        raise ValueError("Upstream audio API changed; extend the WeAct silent implementation")
    return True
