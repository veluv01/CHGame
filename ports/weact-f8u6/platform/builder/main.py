"""PlatformIO/SCons integration for the current CHGame checkout.

This file defines build/upload actions. Creating the source package does
not execute them. See the project README before provisioning a blank board.
"""
import json
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys

from SCons.Script import Builder, Default, DefaultEnvironment
from SCons.Tool import createObjBuilders

env = DefaultEnvironment()

builder = Path(env.PioPlatform().get_dir()) / "builder"
sys.path.insert(0, str(builder))
from chgame_profile import (BOARD_PATH, TARGET, TARGET_ID, application, boot_sources,
                     find_repo, properties, validate_sources, words)

if env.GetProjectOption("board") != TARGET:
    raise ValueError("Loaded platform profile does not match the selected board; check the local platform link")

project = Path(env.subst("$PROJECT_DIR"))
if sys.version_info < (3, 10):
    raise ValueError("Run PlatformIO with Python 3.10 or newer for the CHGame Python host")
repo = find_repo(project)
validate_sources(repo)
board_root = repo / BOARD_PATH
core = board_root / "cores/arduino"
system = board_root / "system/CH32X035"
image_kind = env.GetProjectOption("custom_chgame_image", "app")
if image_kind not in ("app", "bootloader"):
    raise ValueError("custom_chgame_image must be app or bootloader")

toolchain = Path(env.PioPlatform().get_package_dir("toolchain-riscv"))
prefix = "riscv-none-embed-"
compiler_bin = toolchain / "bin"
if not any((compiler_bin / (prefix + "gcc" + ext)).is_file() for ext in ("", ".exe")):
    raise ValueError("This port requires the pinned WCH GCC 8 riscv-none-embed toolchain")
env.PrependENVPath("PATH", str(compiler_bin))
env.Replace(
    CC=prefix + "gcc", CXX=prefix + "g++", AS=prefix + "gcc",
    AR=prefix + "gcc-ar", RANLIB=prefix + "gcc-ranlib",
    OBJCOPY=prefix + "objcopy", SIZE=prefix + "size", GDB=prefix + "gdb",
    PROGNAME="firmware", PROGSUFFIX=".elf", ARFLAGS=["rcs"],
    SIZEPROGREGEXP=r"^(?:\.init|\.vector|\.text|\.fini|\.rodata|\.data|\.ramfunc|\.preinit_array|\.init_array|\.fini_array)\s+([0-9]+).*",
    SIZEDATAREGEXP=r"^(?:\.data|\.bss|\.noinit|\.ramfunc)\s+([0-9]+).*",
    SIZECHECKCMD="$SIZE -A $SOURCES", SIZEPRINTCMD="$SIZE -B -d $SOURCES",
)
common = ["-march=rv32imacxw", "-mabi=ilp32", "-msmall-data-limit=8", "-msave-restore",
          "-ffunction-sections", "-fdata-sections", "-fno-common"]
env.Append(CCFLAGS=common, ASFLAGS=common,
           LINKFLAGS=common + ["-nostartfiles", "-Wl,--gc-sections", "-Wl,-Map,$BUILD_DIR/firmware.map"])
env.Replace(ASCOM="$AS -x assembler-with-cpp $ASFLAGS $_CPPDEFFLAGS $_CPPINCFLAGS -c -o $TARGET $SOURCE")

if image_kind == "app":
    app = application(repo, project, env.GetProjectOption)
    props = app["board"]
    variant = board_root / "variants" / props["build.variant"]
    sketch_key = hashlib.sha256(str(app["sketch"].resolve()).encode()).hexdigest()[:16]
    includes = [variant, variant.parent, core, core / "ch32/usb", core / "avr", core / "ch32", core / "ch32/lib",
                system / "USER", system / "SRC/Core", system / "SRC/Debug",
                system / "SRC/Startup", system / "SRC/Peripheral/inc", system / "SRC/Peripheral/src"]
    env.Replace(PROJECT_SRC_DIR=str(app["sketch"]), PROJECTSRC_DIR=str(app["sketch"]),
                BUILD_SRC_DIR="$BUILD_DIR/sketch-" + sketch_key,
                LDSCRIPT_PATH=str(system / "SRC/Ld" / props["build.ldscript"]), LINK="$CXX")
    if app["external_sketch"]:
        # Match Arduino: root sketch units and src/, without PC tests or shims.
        env.Replace(SRC_FILTER=["+<*.ino>", "+<*.c>", "+<*.cpp>", "+<*.cc>",
                               "+<*.cxx>", "+<*.S>", "+<*.s>", "+<src/>"])
    env.BoardConfig().update("upload.maximum_size", int(props["upload.maximum_size"]))
    env.BoardConfig().update("upload.maximum_ram_size", int(props["upload.maximum_data_size"]))
    env.Append(CPPPATH=[str(p) for p in includes],
               CPPDEFINES=[("ARDUINO", 10819), "ARDUINO_ARCH_CH32", "CH32X035", "CH32X035F8U",
                           ("VARIANT_H", env.StringifyMacro(props["build.variant_h"]))],
               CFLAGS=["-std=gnu99"],
               CXXFLAGS=["-std=gnu++14", "-fno-threadsafe-statics", "-fno-rtti", "-fno-exceptions",
                         "-fno-use-cxa-atexit", "-fpermissive"],
               CCFLAGS=words(props["build.flags.optimize"]) + ["-g", "-fsigned-char"],
               ASFLAGS=words(props["build.flags.optimize"]),
               LINKFLAGS=["-fsigned-char"] + words(props["build.flags.optimize"])
                         + words(props.get("build.flags.ltolink", ""))
                         + words(props["build.flags.ldflags"]),
               LIBS=words(props.get("build.ch_extra_lib", "").replace("-l", ""))
                    + words(props.get("build.math_lib_gcc", "").replace("-l", "")) + ["c"])
    for key in ("build.flags.clock", "build.flags.debug", "build.flags.periph", "build.usb_flags", "build.flags.board"):
        env.MergeFlags(props.get(key, ""))
    for define in app["defines"]:
        env.MergeFlags("-D" + define)
    env["CHGAME_APP"] = app
    env["CHGAME_CORE"] = str(core)
    env["CHGAME_VARIANT"] = str(variant)
else:
    boot = repo / "platform/bootloader"
    mode = env.GetProjectOption("custom_chgame_boot_mode", "release")
    menu = env.GetProjectOption("custom_chgame_menu", "list")
    style = env.GetProjectOption("custom_chgame_style", "rainbow")
    if mode not in ("release", "locked", "nomenu") or menu not in ("list", "visual") or style not in ("rainbow", "static"):
        raise ValueError("Unsupported bootloader mode/menu/style")
    env["CHGAME_BOOT_SOURCES"] = [str(p) for p in boot_sources(repo, mode, menu)]
    env["CHGAME_BOOT_MARGIN"] = 192 if menu == "visual" and mode != "nomenu" else 256
    env.Replace(PROJECT_SRC_DIR=str(repo / "ports/weact-f8u6/bootloader-src"),
                PROJECTSRC_DIR=str(repo / "ports/weact-f8u6/bootloader-src"), SRC_FILTER=["-<*>"],
                LDSCRIPT_PATH=str(boot / "ld/link_boot.ld"), LINK="$CC")
    env.BoardConfig().update("upload.maximum_size", 12288)
    env.Append(CPPPATH=[str(boot / p) for p in ("shared", "src", "vendor/usbcdc", "vendor/spl",
                                              "vendor/spl/Core", "vendor/spl/Peripheral/inc", "vendor/spl/Startup")],
               CPPDEFINES=["CH32X035", "CHBOOT_WEACT_F8U6", "CH32V_LOCK_DEBUG",
                           ("SYSCLK_FREQ_48MHz_HSI", 48000000), ("F_CPU", 48000000),
                           ("CHGAME_BOARD_TARGET", "0x36384657"), ("CHGAME_IMAGE_ID", 1),
                           ("CHGAME_ALLOW_SELFUPDATE", 0 if mode == "locked" else 1),
                           ("CHBOOT_MENU", 0 if mode == "nomenu" else 1),
                           ("MENU_UI", "MENU_UI_" + menu.upper()),
                           ("MENU_STYLE", "MENU_STYLE_" + style.upper())],
               CFLAGS=["-std=gnu11", "-Werror=implicit-function-declaration"],
               CCFLAGS=["-Os", "-flto", "-fno-jump-tables", "-Wall", "-Wextra", "-Wundef", "-g"],
               ASFLAGS=["-Os", "-flto"],
               LINKFLAGS=["-Os", "-flto", "--specs=nano.specs", "--specs=nosys.specs"])
    if menu == "visual":
        flags = ["-fno-guess-branch-probability", "-fno-shrink-wrap", "-fno-tree-scev-cprop",
                 "-fno-caller-saves", "-fno-tree-vrp"]
        env.Append(CCFLAGS=flags, LINKFLAGS=flags)

env["CHGAME_IMAGE_KIND"] = image_kind
env["CHGAME_REPO"] = str(repo)

# Use one object for .S; the lower-case suffix is not a GNU assembly source.
static_obj, shared_obj = createObjBuilders(env)
for obj in (static_obj, shared_obj):
    obj.add_action(".S", "$ASCOM")

env.Append(BUILDERS={"ElfToBin": Builder(action=env.VerboseAction(
    "$OBJCOPY -O binary $SOURCES $TARGET", "Creating $TARGET"), suffix=".bin")})


def upstream_host():
    host = repo / "platform/bootloader/host/py"
    sys.path.insert(0, str(host))
    return host


def prepare_image(source, target, env):
    # This runs only after a user-requested build, never during apply/check.
    binary = Path(str(target[0]))
    data = binary.read_bytes()
    upstream_host()
    build = {"board": TARGET, "target_id": TARGET_ID, "image_kind": image_kind,
             "flash_bytes": len(data), "hardware_tested": False,
             "build_type": env.GetBuildType()}
    if image_kind == "bootloader":
        from chgame_upload.image import boot_image_board
        if boot_image_board(data) != TARGET_ID:
            raise ValueError("Bootloader does not carry the WeAct WF86 board ID")
        if len(data) > 12288 - env["CHGAME_BOOT_MARGIN"]:
            raise ValueError("WeAct bootloader fails upstream Gate A size margin")
        build.update(name=env["PIOENV"], flash_budget=12288 - env["CHGAME_BOOT_MARGIN"],
                     gate_margin=env["CHGAME_BOOT_MARGIN"], app_origin=0)
        binary.with_name("chgame-build.json").write_text(json.dumps(build, indent=2) + "\n")
        return
    from chgame_upload import chg
    limit = int(app["board"]["upload.maximum_size"]) - app["save_pages"] * 256
    if (len(data) + 3) // 4 * 4 > limit:
        raise ValueError("Application exceeds its " + str(limit) + " byte budget with save pages")
    package = binary.with_suffix(".chg")
    package.write_bytes(chg.pack(data, chg.default_title(app["name"]), device=TARGET))
    build.update(name=app["name"], flash_budget=limit, save_pages=app["save_pages"],
                 options=app["options"], defines=app["defines"], app_origin=0x3000)
    binary.with_name("chgame-build.json").write_text(json.dumps(build, indent=2) + "\n")
    if app["external_sketch"] and env.GetProjectOption("custom_chgame_export", "yes") == "yes":
        kind = "debug" if "debug" in env.GetBuildType() else "release"
        out = app["sketch"] / "build" / TARGET / kind
        out.mkdir(parents=True, exist_ok=True)
        stem = app["name"] + ".ino"
        for artifact, suffix in ((binary, ".bin"), (package, ".chg"),
                                 (binary.with_suffix(".elf"), ".elf")):
            shutil.copy2(artifact, out / (stem + suffix))


def upload(source, target, env):
    host = upstream_host()
    child_env = dict(os.environ)
    child_env["PYTHONPATH"] = str(host) + os.pathsep + child_env.get("PYTHONPATH", "")
    command = [sys.executable, "-m", "chgame_upload"]
    if image_kind == "app":
        command += ["flash", str(source[0]), "--device", TARGET, "--run"]
    else:
        # Explicit bootloader environment upload provisions through factory ISP.
        # Whole code flash is erased by upstream's provisioning routine.
        command += ["burn", "--method", "isp", "--bootloader", str(source[0])]
    if env.get("UPLOAD_PORT") and image_kind == "app":
        command += ["--port", env.subst("$UPLOAD_PORT")]
    return subprocess.call(command, env=child_env)


program = env.BuildProgram()
binary = env.ElfToBin("$BUILD_DIR/firmware", program)
env.AddPostAction(binary, prepare_image)
env.Depends(binary, "checkprogsize")
env.AddPlatformTarget("size", program, env.VerboseAction("$SIZEPRINTCMD", "Calculating size $SOURCE"),
                      title="Size", description="Display application or bootloader size")
env.AddPlatformTarget("upload", binary, env.VerboseAction(upload, "Uploading WeAct firmware"),
                      title="Upload", description="CHGame USB application upload, or explicit factory ISP bootloader provisioning")
Default(binary)
