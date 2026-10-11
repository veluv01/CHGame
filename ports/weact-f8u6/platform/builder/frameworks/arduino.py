"""Compile current CHGame sources; no duplicate framework or game sources."""
from pathlib import Path
from SCons.Script import Import

Import("env")

if env["CHGAME_IMAGE_KIND"] == "app":
    # Direct core objects have the same reachability as Arduino's whole-archive
    # core.a: preserve startup, USB interrupts and constructors, then gc-sections.
    env.BuildSources("$BUILD_DIR/FrameworkArduino", env["CHGAME_CORE"])
    env.BuildSources("$BUILD_DIR/Variant", env["CHGAME_VARIANT"])
else:
    root = Path(env["CHGAME_REPO"]) / "platform/bootloader"
    groups = {}
    for source in env["CHGAME_BOOT_SOURCES"]:
        path = Path(source)
        groups.setdefault(path.parent, []).append(path.name)
    for folder, names in groups.items():
        relative = folder.relative_to(root)
        filters = " ".join("+<" + name + ">" for name in names)
        env.BuildSources(str(Path(env.subst("$BUILD_DIR")) / "Bootloader" / relative), str(folder), filters)
