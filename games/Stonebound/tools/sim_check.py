#!/usr/bin/env python3
"""Exercise the actual sketch and capture simulator screenshots, not mockups."""
import base64
import io
import os
from pathlib import Path
import re
import sys
GAME = Path(__file__).resolve().parents[1]
ROOT = GAME.parents[1]
sys.path.insert(0, str(ROOT / "tools" / "chsim"))
from chsim import build
from chdrivelib import Driver, SimTransport, mask_of
from fbimage import to_image, save_gif

os.environ.setdefault("CHSIM_CXX", "g++")
os.environ.setdefault("CHSIM_FLAGS", "-include chrono")
os.environ["STONEBOUND_SEED"] = "20261012"
OUT = GAME / "out" / "sim"
DOCS = GAME / "docs"
OUT.mkdir(parents=True, exist_ok=True)
DOCS.mkdir(parents=True, exist_ok=True)
exe = build(GAME)
d = Driver(SimTransport(exe), "STBN")
d.handshake()
d.cmd("L1")
frames = []
checks = 0

def check(ok, why):
    global checks
    checks += 1
    if not ok:
        raise AssertionError(why)
    print("stonebound UI: " + why, flush=True)

def state():
    row = d.query("u", "STATE ")
    return {k:int(v) for k,v in re.findall(r"(\w+)=(\d+)", row)}

def tap(button, hold=3):
    d.buttons(mask_of(button))
    d.frames(hold)
    d.buttons(0)
    d.frames(2)
    frames.append(to_image(d.shot(), 3))

def capture(name):
    image = to_image(d.shot(), 3)
    image.save(DOCS / (name + ".png"))
    buf = io.BytesIO()
    image.save(buf, format="PNG", optimize=True)
    print("STONEBOUND_IMAGE " + name + " " + base64.b64encode(buf.getvalue()).decode())
    return image

d.frames(2)
capture("title")
check(state()["screen"] == 0, "boots to title")
tap("SELECT")
check(state()["hero"] == 1, "hero selection uses SELECT")
tap("A")
capture("story")
check(state()["screen"] == 1, "new journey shows story")
tap("A")
check(state()["screen"] == 2 and state()["save"] == 1, "new run begins and stores its checkpoint")
capture("dungeon")
turn = state()["turns"]
tap("SELECT")
capture("satchel")
check(state()["screen"] == 3 and state()["turns"] == turn, "inventory opens without spending a turn")
tap("B")
tap("B")
capture("spells")
check(state()["screen"] == 4 and state()["turns"] == turn, "spell menu opens without spending a turn")
tap("B")
for button in ["RIGHT","RIGHT","DOWN","DOWN","RIGHT","UP","RIGHT","RIGHT"]:
    tap(button)
capture("exploration")
before = state()
tap("START")
capture("pause")
check(state()["screen"] == 5, "short START tap opens pause")
tap("DOWN")
tap("A")
check(state()["save"] == 1, "pause menu writes a save")
d.cmd("v")
d.frames(2)
check(state()["screen"] == 0 and state()["save"] == 1, "save record survives a reset of game RAM")
tap("A")
after = state()
for key in ["floor","hp","mp","turns","x","y"]:
    check(after[key] == before[key], "continue restores " + key)
# Three seconds must remain below this game's five-second quit threshold.
d.buttons(mask_of("START")); d.frames(90)
check(state()["screen"] == 2, "holding START for three seconds keeps the game running")
d.buttons(0); d.frames(2)
check(state()["screen"] == 5, "releasing START cancels quit and opens pause")
tap("START")
check(state()["screen"] == 2, "short START resumes")
d.buttons(mask_of("START")); d.frames(148)
check(state()["screen"] == 2, "still running immediately before five seconds")
try:
    d.frames(6)
except EOFError:
    pass
else:
    raise AssertionError("five-second START hold did not exit")
check(d.t.p.wait(timeout=10) == 0, "five-second hold exits successfully")
check(not any("BUG" in s for s in d.t.bugs), "simulator reports no drawing or DMA bugs")
save_gif(frames, DOCS / "gameplay.gif", 220)
capture_image = DOCS / "title.png"
from PIL import Image
Image.open(capture_image).resize((128,128),Image.Resampling.NEAREST).save(DOCS/"cart.png")
print("stonebound UI: " + str(checks) + " checks passed")
