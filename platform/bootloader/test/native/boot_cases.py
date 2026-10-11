"""Whole-boot scenarios (test_boot.c): builds the package zoo and a FAT32
card, writes the manifest in the menu's display order, runs the scenarios,
converts the panel dumps to PNG and checks them against frames.json.

frames.json pins the SHA-256 of every frame. A change that alters a frame
fails until the new hashes are recorded (python3 run_tests.py --pin-frames),
CLAUDE.md rule 2: the pictures are the test."""
import hashlib
import json
import pathlib
import struct
import zlib

import chgpack
import fatimg

HERE = pathlib.Path(__file__).resolve().parent
TITLE_COLS = 19


def display_title(name, data):
    """What the menu shows: the header title, or the 8.3 name for a bad header."""
    try:
        chgpack.parse(data, check_payload=False)
        t = data[0x20:0x40].split(b"\0", 1)[0].decode("ascii")[:TITLE_COLS]
    except chgpack.ChgError:
        t = name.split(".")[0][:8]
    return t.ljust(TITLE_COLS)


def zoo(pk):
    z = {n: d for n, (d, _) in pk.items()}
    alpha = z["ALPHA.CHG"]
    # a bootloader image disguised as a game: valid header and CRCs, signature at payload offset 8
    p = bytearray(alpha[512:])
    struct.pack_into("<I", p, 8, chgpack.BOOT_SIG)
    h = bytearray(alpha[:512])
    h[0x20:0x40] = b"BOOT IMAGE".ljust(32, b"\0")
    struct.pack_into("<I", h, 0x14, zlib.crc32(bytes(p)) & 0xFFFFFFFF)
    struct.pack_into("<I", h, 0x1FC, zlib.crc32(bytes(h[:0x1FC])) & 0xFFFFFFFF)
    z["BOOTIMG.CHG"] = bytes(h) + bytes(p)
    # header fine, payload damaged
    h = bytearray(alpha[:512])
    h[0x20:0x40] = b"BAD PAYLOAD".ljust(32, b"\0")
    struct.pack_into("<I", h, 0x1FC, zlib.crc32(bytes(h[:0x1FC])) & 0xFFFFFFFF)
    p = bytearray(alpha[512:])
    p[5000] ^= 0x40
    z["BADPCRC.CHG"] = bytes(h) + bytes(p)
    return z


def cart_background_png():
    """The cart cards' picture: dark blue, a band in the rainbow colour at the
    top, a green block, a yellow strip at the foot."""
    import io
    from PIL import Image, ImageDraw
    im = Image.new("RGB", (128, 128), (0, 0, 64))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, 127, 15], fill=(255, 0, 255))
    d.rectangle([100, 30, 127, 127], fill=(0, 96, 0))
    d.rectangle([0, 120, 60, 127], fill=(200, 200, 0))
    buf = io.BytesIO()
    im.save(buf, "PNG")
    return buf.getvalue()


def cart_cards(build_dir, pk):
    """Three cards made by the reference implementation (tools/chcart), as
    test_boot.c's t_cart_* expect them:

      cart        GAMES/ in MENU.IDX's order: ZULU (BRAVO's payload), the
                  folder FOLDER ONE (ALPHA GAME, then the folder INNER with
                  BRAVO), MIKE; then EXTRA.CHG ("AAA EXTRA"), copied in by
                  hand and not in the index, and an empty folder EMPTY (by
                  hand too); the index also names GHOST.CHG, which is not on
                  the card. A background of four colours with a band in the
                  rainbow colour.
      cartlaunch  the same cart launching ALPHA GAME (inside FOLDER ONE)
      cartbadbg   cart with its MENU.BG cut short: the menu's own look

    Returns ({name: image path}, {pkg name: CHG bytes}) for the extra
    payloads (MIKE, EXTRA)."""
    from chcart import model, runtime
    def payload(n):
        return pk[n][0][512:512 + chgpack.parse(pk[n][0])["payload_bytes"]]
    alpha, bravo = payload("ALPHA.CHG"), payload("BRAVO.CHG")
    mike = bytes((i * 7 + 3) & 0xFF for i in range(6000))
    extra = bytes((i * 13 + 5) & 0xFF for i in range(3000))

    def cart(launch=""):
        G = model.Game
        return model.Cart(title="TEST CART", launch=launch, background=cart_background_png(), games=[
            G("zulu", "ZULU", {"rev0": bravo}),
            G("alpha", "ALPHA GAME", {"rev0": alpha}, folder="FOLDER ONE"),
            G("bravo", "BRAVO", {"rev0": bravo}, folder="FOLDER ONE/INNER"),
            G("mike", "MIKE", {"rev0": mike})])

    out, extras = {}, {}
    for name, c in (("cart", cart()), ("cartlaunch", cart("alpha")), ("cartbadbg", cart())):
        files = runtime.prepare(c)
        files["GAMES/EXTRA.CHG"] = chgpack.pack(extra, "AAA EXTRA")
        files["GAMES/EMPTY/"] = None
        files["GAMES/MENU.IDX"] += b"GHOST   CHG".ljust(32, b"\0")
        if name == "cartbadbg":
            files["GAMES/MENU.BG"] = files["GAMES/MENU.BG"][:-512]
        out[name] = build_dir / f"{name}.img"
        fatimg.build_image(str(out[name]), files, fs="fat32", lfn=True, fragment={"GAMES/MENU.BG": 3})
    extras["MIKE.CHG"] = chgpack.pack(mike, "MIKE")
    extras["EXTRA.CHG"] = chgpack.pack(extra, "AAA EXTRA")
    # cartbig: 250 games in GAMES/, more than a folder lists (MENU_MAX_GAMES,
    # 240): the menu keeps the first 240 the directory holds, GAME 000-239
    big = {f"GAMES/G{i:03d}.CHG": chgpack.pack(bytes((j * 7 + i * 13) & 0xFF for j in range(64 + i)), f"GAME {i:03d}")
           for i in range(250)}
    out["cartbig"] = build_dir / "cartbig.img"
    fatimg.build_image(str(out["cartbig"]), big, fs="fat32")
    extras["G239.CHG"] = big["GAMES/G239.CHG"]
    return out, extras


# ---- the visual menu (test_visual.c) -------------------------------------------------

PICTURE_COLOURS = {          # test pictures: a colour each, so every frame is told apart
    "cover": ((0, 0, 96), (255, 255, 255)), "about": ((96, 0, 96), (255, 255, 0)),
    "f1": ((0, 96, 0), (255, 255, 255)), "inner": ((0, 96, 96), (255, 200, 0)),
    "zulu": ((160, 0, 0), (255, 255, 255)), "alpha": ((0, 0, 200), (255, 128, 0)),
    "bravo": ((200, 100, 0), (0, 0, 0)), "oscar": ((100, 100, 100), (0, 255, 0)),
    "mike": ((0, 140, 140), (255, 0, 0)), "romeo": ((140, 0, 140), (0, 255, 255)),
}


def test_picture(name, k=0):
    """A 128x128 PNG that follows the picture rule: a colour of its own behind,
    a second colour in bands, a square whose place says which picture it is;
    the cover also has a bar in #FF00FF (the rainbow colour)."""
    import io
    from PIL import Image, ImageDraw
    if name.startswith("sys"):
        back, band = ((20 + k * 25) % 256, 40, (200 - k * 20) % 256), (255, 255, 255)
    else:
        back, band = PICTURE_COLOURS[name]
        k = list(PICTURE_COLOURS).index(name)
    im = Image.new("RGB", (128, 128), back)
    d = ImageDraw.Draw(im)
    for y in range(8, 128, 24):
        d.rectangle([0, y, 127, y + 3], fill=band)
    d.rectangle([8 + 10 * k, 40, 17 + 10 * k, 49], fill=(255, 244, 214))
    if name == "cover":
        d.rectangle([0, 112, 127, 119], fill=(255, 0, 255))
    b = io.BytesIO()
    im.save(b, "PNG")
    return b.getvalue()


def system_test_pic():
    """SYSTEM.PIC with a test picture for each of its nine screens."""
    from chcart import runtime
    return b"".join(runtime.menu_picture(test_picture("sys", k)) for k in range(9))


def visual_cards(build_dir, pk):
    """The cards test_visual.c reads (its header says what is where):
    vcart, vcartlaunch (the same, launching ALPHA GAME), vcartbad (pictures
    broken, no SYSTEM.PIC). Returns ({name: image path}, {pkg name: CHG
    bytes})."""
    from chcart import model, runtime

    def payload(n):
        return pk[n][0][512:512 + chgpack.parse(pk[n][0])["payload_bytes"]]
    alpha, bravo = payload("ALPHA.CHG"), payload("BRAVO.CHG")
    mike = bytes((i * 7 + 3) & 0xFF for i in range(6000))
    extra = bytes((i * 13 + 5) & 0xFF for i in range(3000))
    oscar = bytes((i * 11 + 1) & 0xFF for i in range(4000))
    romeo = bytes((i * 5 + 9) & 0xFF for i in range(5000))
    stray = bytes((i * 3 + 7) & 0xFF for i in range(2000))
    P = test_picture

    def cart(launch=""):
        G = model.Game
        return model.Cart(title="VISUAL CART", launch=launch, cover=P("cover"), about=P("about"),
                          folder_covers={"FOLDER ONE": P("f1"), "FOLDER ONE/INNER": P("inner")}, games=[
            G("zulu", "ZULU", {"rev0": bravo}, cart_image=P("zulu")),
            G("alpha", "ALPHA GAME", {"rev0": alpha}, folder="FOLDER ONE", cart_image=P("alpha")),
            G("bravo", "BRAVO", {"rev0": bravo}, folder="FOLDER ONE/INNER", cart_image=P("bravo")),
            G("oscar", "OSCAR", {"rev0": oscar}, folder="FOLDER ONE/OTHER", cart_image=P("oscar")),
            G("mike", "MIKE", {"rev0": mike}, cart_image=P("mike")),
            G("romeo", "ROMEO", {"rev0": romeo}, folder="TWO", cart_image=P("romeo"))])

    out, extras = {}, {}
    sysp = system_test_pic()
    for name, c in (("vcart", cart()), ("vcartlaunch", cart("alpha")), ("vcartbad", cart())):
        files = runtime.prepare(c)
        files["GAMES/EXTRA.CHG"] = chgpack.pack(extra, "AAA EXTRA")
        files["GAMES/EMPTY/"] = None
        files["GAMES/SYSTEM.PIC"] = sysp
        if name == "vcartbad":
            m = bytearray(files["GAMES/MIKE.CHG"])          # its picture's offset past the file
            struct.pack_into("<I", m, chgpack.IMAGE_OFF, 0x1F000)
            struct.pack_into("<I", m, 0x1FC, zlib.crc32(bytes(m[:0x1FC])) & 0xFFFFFFFF)
            files["GAMES/MIKE.CHG"] = bytes(m)
            files["GAMES/ZULU.CHG"] = files["GAMES/ZULU.CHG"][:-4000]     # cut in its picture
            files["GAMES/FOLDERON/COVER.PIC"] = files["GAMES/FOLDERON/COVER.PIC"][:8000]
            del files["GAMES/SYSTEM.PIC"]
        out[name] = build_dir / f"{name}.img"
        fatimg.build_image(str(out[name]), files, fs="fat32", lfn=True, fragment={"GAMES/MIKE.CHG": 3})
    for n, d in (("MIKE.CHG", mike), ("EXTRA.CHG", extra), ("OSCAR.CHG", oscar), ("ROMEO.CHG", romeo),
                 ("STRAY.CHG", stray)):
        extras[n] = chgpack.pack(d, n[:-4])
    return out, extras


def run_visual(build_dir, build, run, pk, z, order, extras, card, fdir):
    """The visual menu's suites, both styles, on the cards above and on the
    zoo card with SYSTEM.PIC added (fat32sys)."""
    from run_tests import CORE, VISUAL, hostpath   # noqa: E402
    vimgs, vextras = visual_cards(build_dir, pk)
    files = {"GAMES/" + n: d for n, d in z.items()}
    files["WORDS.DIC"] = bytes(5000)
    files["GAMES/SYSTEM.PIC"] = system_test_pic()
    sysimg = build_dir / "boot_fat32sys.img"
    fatimg.build_image(str(sysimg), files, fs="fat32", lfn=True, fragment={"GAMES/ALPHA.CHG": 7})
    pkdir = build_dir / "pk"
    man = [f"img fat32 {hostpath(card)}", f"img fat32sys {hostpath(sysimg)}",
           f"img cartbig {hostpath(build_dir / 'cartbig.img')}"]
    man += [f"img {k} {hostpath(v)}" for k, v in vimgs.items()]
    allx = dict(extras)
    allx.update(vextras)
    for n in order + sorted(set(allx) - set(order)):   # (the zoo first: menu_index() is its position)
        d = z.get(n) or allx[n]
        (pkdir / n).write_bytes(d)
        try:
            info = chgpack.parse(d, check_payload=False)
            ln, crc = info["payload_bytes"], info["payload_crc32"]
        except chgpack.ChgError:
            ln, crc = 0, 0
        man.append(f"pkg {n} 0 {ln} {crc:x} {hostpath(pkdir / n)}")
    mpath = build_dir / "visual_manifest.txt"
    mpath.write_text("\n".join(man) + "\n")
    defs = ["-DCHBOOT_MENU=1", "-DCHGAME_ALLOW_SELFUPDATE=1", "-DMENU_UI=MENU_UI_VISUAL"]
    exe = build("visual", "test_visual.c", defs, CORE + VISUAL)
    ok = run("visual", exe, mpath, fdir)
    real = HERE.parents[3] / "out" / "sdcard.img"
    if real.exists():
        ok &= run("visual_real", exe, real_manifest(build_dir, real), fdir, "real")
    ok &= run("visual_static", build("visual_static", "test_visual.c", defs + ["-DMENU_STYLE=MENU_STYLE_STATIC"],
                                     CORE + VISUAL), mpath, fdir, "style", "static_")
    return ok


def check_pictures(fdir):
    """The visual menu shows a picture as the tools made it: the frame equals
    the picture, converted (chcart's menu_picture) and expanded from RGB565 as
    the panel model does. Static: every pixel; rainbow: all but those in
    colour 15."""
    from PIL import Image
    from chcart import runtime
    checks = {"v_row_zulu": "zulu", "v_flip_one": "f1", "static_v_flip_one": "f1", "v_splash": "cover",
              "static_v_splash": "cover", "v_inner_entry": "inner", "v_about": "about"}   # (about: SYSTEM.PIC's first)
    diff = {}
    for frame, name in checks.items():
        f = fdir / f"{frame}.png"
        if not f.exists():
            diff[frame] = "missing"
            continue
        pic = runtime.menu_picture(test_picture("sys", 0) if name == "about" else test_picture(name))
        pal = [int.from_bytes(pic[8 + 2 * i:10 + 2 * i], "little") for i in range(16)]
        exp = [((c >> 11) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31) for c in pal]
        real = Image.open(f).convert("RGB").load()
        n = 0
        for y in range(128):
            for x in range(128):
                b = pic[512 + y * 64 + x // 2]
                i = b >> 4 if x % 2 == 0 else b & 15
                if i == 15 and not frame.startswith("static_"):
                    continue
                n += real[x, y] != exp[i]
        if n:
            diff[frame] = n
    print(f"{'pictures':12s} {'ok' if not diff else 'FAILED':6s} the visual menu's frames vs the pictures the tools made"
          + (f": pixels differ {diff}" if diff else ""))
    return not diff


def run_all(build_dir, build, run, imgs, lay, pk, quick, pin=False):
    z = zoo(pk)
    pkdir = build_dir / "pk"
    pkdir.mkdir(exist_ok=True)
    files = {}
    for n, d in z.items():
        (pkdir / n).write_bytes(d)
        files["GAMES/" + n] = d
    files["WORDS.DIC"] = bytes(5000)
    card = build_dir / "boot_fat32.img"
    fatimg.build_image(str(card), files, fs="fat32", lfn=True, decoys=True,
                       fragment={"GAMES/ALPHA.CHG": 7, "GAMES/BRAVO.CHG": 3}, dir_pieces={"GAMES": 2})
    order = sorted(z, key=lambda n: display_title(n, z[n]))
    from run_tests import hostpath   # noqa: E402  (paths as the test program sees them)
    carts, extras = cart_cards(build_dir, pk)
    man = [f"img fat32 {hostpath(card)}", f"img fat16 {hostpath(imgs['fat16'])}",
           f"img nogames {hostpath(imgs['nogames'])}"]
    man += [f"img {k} {hostpath(v)}" for k, v in carts.items()]
    for n in order + sorted(extras):             # (the zoo first: menu_index() is its position)
        d = z.get(n) or extras[n]
        if n in extras:
            (pkdir / n).write_bytes(d)
        try:
            info = chgpack.parse(d, check_payload=False)
            ln, crc = info["payload_bytes"], info["payload_crc32"]
        except chgpack.ChgError:
            ln, crc = 0, 0
        man.append(f"pkg {n} 0 {ln} {crc:x} {hostpath(pkdir / n)}")
    mpath = build_dir / "boot_manifest.txt"
    mpath.write_text("\n".join(man) + "\n")
    fdir = build_dir / "frames"
    fdir.mkdir(exist_ok=True)
    for old in fdir.glob("*"):
        old.unlink()
    from run_tests import CORE, MENU   # noqa: E402
    exe = build("boot", "test_boot.c", ["-DCHBOOT_MENU=1", "-DCHGAME_ALLOW_SELFUPDATE=1"], CORE + MENU)
    ok = run("boot", exe, mpath, fdir)
    wexe = build("boot_static", "test_boot.c", ["-DCHBOOT_MENU=1", "-DCHGAME_ALLOW_SELFUPDATE=1",
                                               "-DMENU_STYLE=MENU_STYLE_STATIC"], CORE + MENU)
    ok &= run("boot_static", wexe, mpath, fdir, "style", "static_")   # the static style still works and draws
    real = HERE.parents[3] / "out" / "sdcard.img"
    if real.exists():
        ok &= run_real(build_dir, exe, real, fdir)
    else:
        print(f"{'boot_real':12s} {'skip':6s} no out/sdcard.img (python tools/sdcard/mkcard.py --image out/sdcard.img)")
    ok &= run_visual(build_dir, build, run, pk, z, order, extras, card, fdir)
    ok &= check_frames(fdir, pin)
    ok &= check_preview(fdir)
    ok &= check_pictures(fdir)
    return ok


def check_preview(fdir):
    """`chgame background --preview` (tools/chcart/background.py) draws the
    menu as the bootloader does: the cart card's frame against its preview.
    The rainbow style: every pixel but colour 15's (its hue depends on the
    time). The static style: every pixel."""
    from PIL import Image
    from chcart import background, runtime
    png = cart_background_png()
    ui = dict(background.model.UI_COLORS)
    idx = runtime.menu_background(png, ui)
    diff = {}
    for style, frame in (("rainbow", "cart_menu.png"), ("static", "static_cart_menu.png")):
        pv = background.preview(png, ui, titles=["ZULU", "FOLDER ONE", "MIKE", "AAA EXTRA", "EMPTY"], scale=1,
                                installed=(), folders=(1, 4), style=style)
        real = Image.open(fdir / frame).convert("RGB").load()
        mine = pv.load()
        n = 0
        for y in range(128):
            for x in range(128):
                b = idx[512 + y * 64 + x // 2]
                if style == "rainbow" and ((b >> 4 if x % 2 == 0 else b & 15) == 15 or 20 <= y < 30):
                    continue
                n += real[x, y] != mine[x, y]
        diff[style] = n
    bad = {k: v for k, v in diff.items() if v}
    print(f"{'preview':12s} {'ok' if not bad else 'FAILED':6s} chcart's menu preview vs the bootloader's frames"
          " (rainbow, static)" + (f": pixels differ {bad}" if bad else ""))
    return not bad


def real_manifest(build_dir, img):
    """The manifest for the real card (out/sdcard.img, `chgame card`): every
    CHG file in the menu's order, folder by folder as MENU.IDX lists them.
    A package's code is its place: folder row << 8 | row in that folder
    (folder row 255: a game in GAMES/ itself)."""
    from run_tests import hostpath   # noqa: E402
    vol = fatimg.FatVolume(str(img))

    def index(path):
        data = vol.read_file(path + "/MENU.IDX")
        return [data[k:k + 11] for k in range(32, len(data), 32)]
    pkgs = []
    for frow, raw in enumerate(index("GAMES")):
        name = raw[:8].decode().rstrip()
        if raw[8:11] == b"CHG":
            pkgs.append((255 << 8 | frow, name + ".CHG", "GAMES/" + name + ".CHG"))
            continue
        for row, g in enumerate(index("GAMES/" + name)):
            if g[8:11] == b"CHG":
                n = g[:8].decode().rstrip() + ".CHG"
                pkgs.append((frow << 8 | row, n, f"GAMES/{name}/{n}"))
    pkdir = build_dir / "realpk"
    pkdir.mkdir(exist_ok=True)
    man = [f"img fat32 {hostpath(img)}"]
    for code, n, path in pkgs:
        data = vol.read_file(path)
        (pkdir / n).write_bytes(data)
        info = chgpack.parse(data)
        man.append(f"pkg {n} {code} {info['payload_bytes']} {info['payload_crc32']:x} {hostpath(pkdir / n)}")
    mpath = build_dir / "real_manifest.txt"
    mpath.write_text("\n".join(man) + "\n")
    return mpath


def run_real(build_dir, exe, img, fdir):
    from run_tests import run   # noqa: E402
    return run("boot_real", exe, real_manifest(build_dir, img), fdir, "real")


def check_frames(fdir, pin):
    from PIL import Image
    pinned_path = HERE / "frames.json"
    pinned = json.loads(pinned_path.read_text()) if pinned_path.exists() else {}
    now = {}
    for ppm in sorted(fdir.glob("*.ppm")):
        im = Image.open(ppm).convert("RGB")
        im.save(ppm.with_suffix(".png"))
        now[ppm.stem] = hashlib.sha256(im.tobytes()).hexdigest()
    if pin or not pinned:
        if not pin:
            pinned.clear()
        pinned.update(now)
        pinned_path.write_text(json.dumps(pinned, indent=1, sort_keys=True) + "\n", newline="\n")
        print(f"{'frames':12s} {'pinned':6s} {len(now)} frames recorded in frames.json")
        return True
    # Both real_* and v_real_* need the optional generated SD card fixture.
    keys = set(now) | {k for k in pinned if not k.startswith(("real_", "v_real_"))}
    diff = sorted(k for k in keys if now.get(k) != pinned.get(k))
    print(f"{'frames':12s} {'ok' if not diff else 'FAILED':6s} {len(now)} frames vs frames.json" + (f": changed {diff}" if diff else ""))
    return not diff
