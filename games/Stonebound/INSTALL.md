# Install Stonebound on WeAct F8U6

Target: WeAct CH32X035F8U6 Core Board V1.0, the existing CHGame WeAct wiring,
128x128 ST7735S display, eight buttons, and a working WF86 SD-menu bootloader.

## Run the ready-built game

1. Download **[Stonebound-WeAct-0.1.0.zip](https://github.com/veluv01/CHGame/raw/refs/heads/codex/stonebound-weact/packages/Stonebound-WeAct-0.1.0.zip)**.
2. Extract the package.
3. Copy **sdcard/GAMES/STONE.CHG** to **GAMES/STONE.CHG** on your FAT32 card.
4. Put the card into the console and select **STONEBOUND**.

The game has its artwork in firmware and requires no external asset file.
The SD menu installs the program into internal flash. Saves use the two
reserved flash pages; they are not files on the card.

This download targets **WF86**. Original CHGame Rev0 requires its own build.
Installing applications uses your existing bootloader; this project contains
no bootloader replacement or factory-provisioning command.

## Build with PlatformIO

Use Python 3.10+ and the maintained CHGame fork. Get this development branch:

~~~sh
git clone --branch codex/stonebound-weact https://github.com/veluv01/CHGame.git
cd CHGame
~~~

Then, from its repository root:

~~~sh
python3 -m venv .venv
.venv/bin/python -m pip install -r ports/weact-f8u6/requirements.txt
.venv/bin/pio run -d games/Stonebound -e stonebound
~~~

In VS Code, open **games/Stonebound** as the PlatformIO project. Choose
**stonebound** for release or **stonebound-debug** for USB debug commands.

A downloaded source-only folder belongs at **CHGame/games/Stonebound**.
The platform reads the core and pin mapping from the surrounding CHGame fork.
The tested base revision is **c008a001a51f1eaa5087634193a04bbd4f0657c0**.

Upload the application over the console's CHGame USB bootloader:

~~~sh
.venv/bin/pio run -d games/Stonebound -e stonebound -t upload
~~~

If automatic port detection fails, add **--upload-port COM5** on Windows, or
the actual serial-device path reported on your computer.

## Save and exit

START opens pause when tapped and released. Choose **SAVE JOURNEY** to
checkpoint, or **SAVE + EXIT** to save and return to the console menu.
Holding START for **five seconds** saves and returns directly.
A failed save keeps the game running and displays an error.

A new journey and each floor transition also create checkpoints.
Powering off before saving loses progress since the last checkpoint.
Death ends the run and replaces its checkpoint, as in a permadeath roguelike.
Different CHGame applications share the physical save pages; another game's
save may replace Stonebound's record.

The 0.1.0 release has software build and simulator validation. Physical
display operation, SD installation and flash persistence still need a check
on your assembled console.
