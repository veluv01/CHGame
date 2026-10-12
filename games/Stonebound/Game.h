// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
namespace stone {
constexpr int W=24, H=24, CELLS=W*H, MAX_ENEMIES=12, MAX_LOOT=16, MAX_DOORS=8;
constexpr uint8_t LAST_FLOOR=6;
enum Tile : uint8_t { WALL, FLOOR, STAIRS, DOOR };
enum Kind : uint8_t { RAT, BAT, SKELETON, WRAITH, WARDEN };
enum LootKind : uint8_t { GOLD, POTION, TONIC, RATION, WEAPON, ARMOUR, TRAP };
enum Mode : uint8_t { ALIVE, DEAD, WON };
enum Event : uint8_t { ENTER, STEP, BLOCKED, OPENED, HIT, KILLED, HURT, MISS,
  FOUND_GOLD, FOUND_POTION, FOUND_TONIC, FOUND_FOOD, FOUND_WEAPON, FOUND_ARMOUR,
  TRAPPED, LEVEL_UP, DESCENDED, BOSS_BLOCKS, HEALED, MANA, FED, NO_ITEM,
  NO_MANA, NO_TARGET, CAST_BOLT, CAST_HEAL, CAST_NOVA, HUNGRY, DEFEAT, VICTORY };
struct Enemy { uint8_t x,y,hp,kind,stun; };
struct Loot { uint8_t x,y,kind,value; };
struct Point { uint8_t x,y; };
// This is the complete persistent state. Geometry and loot positions regenerate
// from seed + floor; opened doors, collected loot, enemies and fog are retained.
struct State {
  uint32_t seed, rng;
  uint16_t turns, gold, xp, collected;
  uint8_t floor, hero, x,y, facing, hp,maxhp, mp,maxmp, level;
  uint8_t weapon,armour, potions,tonics,rations, food, mode, opened, enemyCount;
  uint8_t seen[(CELLS+7)/8];
  Enemy enemies[MAX_ENEMIES];
};
static_assert(sizeof(State)<=244, "State must fit the CHGame flash save record");
struct World {
  uint8_t tiles[CELLS], visible[(CELLS+7)/8];
  Loot loot[MAX_LOOT]; Point doors[MAX_DOORS];
  uint8_t lootCount, doorCount, startX,startY, exitX,exitY;
};
extern State game;
extern World world;
extern Event event;
extern uint8_t detail;
void newGame(uint32_t seed, uint8_t hero=0);
bool restore(const State &state);
bool valid(const State &state);
void rebuild();
bool move(int dx,int dy);
bool waitOrDescend();
bool consume(uint8_t item); // 0 potion, 1 tonic, 2 ration
bool cast(uint8_t spell); // 0 bolt, 1 heal, 2 nova
void see();
int enemyAt(int x,int y);
int lootAt(int x,int y);
bool visible(int x,int y);
bool seen(int x,int y);
bool passable(int x,int y);
int attack();
int defence();
const char *message();
const char *enemyName(uint8_t kind);
} // namespace stone
