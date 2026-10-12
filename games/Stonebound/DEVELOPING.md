# Develop Stonebound

Use the game as a standalone PlatformIO project inside the maintained CHGame
fork. The integration tested for 0.1.0 is based on
c008a001a51f1eaa5087634193a04bbd4f0657c0.

## Native rules

The engine has no Arduino, display, filesystem or heap dependency:

~~~sh
mkdir -p games/Stonebound/out
g++ -std=c++14 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer \
  games/Stonebound/Game.cpp games/Stonebound/tools/tests/test_game.cpp \
  -o games/Stonebound/out/rules
games/Stonebound/out/rules
~~~

Tests check 300 seeds across all six floors, connected geometry, deterministic
regeneration, valid enemy/loot placement, melee, doors, spells, food, death,
the final exit, save restoration and invalid-save rejection. Random action
runs exercise invariants and periodic save roundtrips under the sanitizers.

## Simulator and firmware

~~~sh
.venv/bin/python -m pip install "pillow>=10,<12" "pyelftools>=0.30,<1"
CHSIM_CXX=g++ CHSIM_FLAGS="-include chrono" \
  .venv/bin/python games/Stonebound/tools/sim_check.py
.venv/bin/pio run -d games/Stonebound -e stonebound -e stonebound-debug
.venv/bin/python games/Stonebound/tools/package.py
~~~

The simulator script builds and drives the actual sketch. It checks the story,
hero choice, inventory/spell menus, save reload after clearing volatile game
RAM, and the five-second START threshold. It captures screens and an input
demonstration GIF. These are simulator captures, not physical-device footage.

The shared simulator models drawing and DMA sequencing; it does not establish
electrical behaviour, CPU frame rate or physical flash endurance.

## Compact memory model

One 24x24 tile array holds the current floor. A one-bit map tracks visibility,
and another tracks remembered exploration. At most 12 enemies and 16 loot
placements exist. Geometry and initial placements regenerate deterministically
from the run seed and floor number.

The saved State records:
- Run seed and gameplay random-generator state.
- Hero position, facing, health, mana, experience, gold and supplies.
- Current-floor enemies, including moved positions, HP and stun.
- Collected-loot bits, opened-door bits and fog of war.
- Floor number and alive/dead/won mode.

The game uses the CHGame CRC-protected alternating flash records.
Changing State's layout requires changing the save version in Stonebound.ino.
Keep sizeof(State) at most 244 bytes, static RAM at most 18,416 bytes and
the application at most 50,432 bytes so both save pages remain available.
The linker reserves an additional 2 KB stack and the bootloader's retained block.

The release has no debug commands. The debug profile supports the shared
CHGame protocol and **u**, which reports the game state. In the simulator,
**v** clears volatile game state and rereads the save pages for regression tests. The simulator-only **f 1/2/3** commands set up explicit
boss, victory and defeat render fixtures. These are used for screen QA; the
native rules tests independently exercise the final-exit and death mechanics.

## Maintaining the game

Update and test against the shared CHGame core rather than copying or editing
it. Rebuild both firmware profiles after core changes, then run the native and
simulator checks and regenerate packages. Keep board identity **WF86** when
packaging WeAct binaries. An original-console port needs an independently
built Rev0 binary and **CX35** identity.

This first release supports forward floor transitions. It has no branches,
religion system, arbitrary spell list, shop or full DCSS content compatibility.
New features must preserve map connectivity, save roundtrips and fixed memory
budgets.
