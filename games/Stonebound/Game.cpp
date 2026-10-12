// SPDX-License-Identifier: MIT
#include "Game.h"
#include <string.h>
namespace stone {
State game{};
World world{};
Event event=ENTER;
uint8_t detail=0;
static uint32_t next(uint32_t &s) { s^=s<<13; s^=s>>17; s^=s<<5; return s; }
static int rnd(int n) { return int(next(game.rng)%uint32_t(n)); }
static bool inside(int x,int y) { return x>=0 && x<W && y>=0 && y<H; }
static int index(int x,int y) { return y*W+x; }
static bool bit(const uint8_t *p,int i) { return (p[i>>3]>>(i&7))&1; }
static void setbit(uint8_t *p,int i) { p[i>>3]|=uint8_t(1u<<(i&7)); }
static int absn(int n) { return n<0?-n:n; }
static int dist(int x,int y,int xx,int yy) { return absn(x-xx)+absn(y-yy); }
static bool line(int x,int y,int xx,int yy) {
  int dx=absn(xx-x), dy=-absn(yy-y), sx=x<xx?1:-1, sy=y<yy?1:-1, e=dx+dy;
  while(x!=xx || y!=yy) {
    int e2=e*2;
    if(e2>=dy) {e+=dy; x+=sx;}
    if(e2<=dx) {e+=dx; y+=sy;}
    if(x==xx && y==yy) return true;
    if(!inside(x,y) || world.tiles[index(x,y)]==WALL || world.tiles[index(x,y)]==DOOR) return false;
  }
  return true;
}
bool passable(int x,int y) {
  return inside(x,y) && world.tiles[index(x,y)]!=WALL && world.tiles[index(x,y)]!=DOOR;
}
bool visible(int x,int y) { return inside(x,y) && bit(world.visible,index(x,y)); }
bool seen(int x,int y) { return inside(x,y) && bit(game.seen,index(x,y)); }
void see() {
  memset(world.visible,0,sizeof world.visible);
  for(int y=int(game.y)-5;y<=int(game.y)+5;y++) for(int x=int(game.x)-5;x<=int(game.x)+5;x++)
    if(inside(x,y) && (x-game.x)*(x-game.x)+(y-game.y)*(y-game.y)<=29 &&
       line(game.x,game.y,x,y)) {setbit(world.visible,index(x,y)); setbit(game.seen,index(x,y));}
}
int enemyAt(int x,int y) {
  for(int i=0;i<game.enemyCount;i++) if(game.enemies[i].hp && game.enemies[i].x==x && game.enemies[i].y==y) return i;
  return -1;
}
int lootAt(int x,int y) {
  for(int i=0;i<world.lootCount;i++) if(!(game.collected&(1u<<i)) && world.loot[i].x==x && world.loot[i].y==y) return i;
  return -1;
}
int attack() { return 4+(game.hero==0?2:0)+game.weapon+game.level; }
int defence() { return (game.hero==0?1:0)+game.armour; }
const char *enemyName(uint8_t k) {
  static const char *const names[]={"CAVE RAT","ASH BAT","BONE GUARD","WRAITH","STONE WARDEN"};
  return names[k<=WARDEN?k:0];
}
static uint8_t maxEnemyHP(uint8_t k,uint8_t floor) {
  return uint8_t((k==WARDEN?28:4+int(k)*2)+floor);
}
static void carve(int x,int y) {
  if(x>0&&x<W-1&&y>0&&y<H-1) world.tiles[index(x,y)]=FLOOR;
}
static bool freeCell(int x,int y) {
  if(!passable(x,y) || (x==world.startX&&y==world.startY) ||
     (x==world.exitX&&y==world.exitY) || enemyAt(x,y)>=0) return false;
  for(int i=0;i<world.lootCount;i++) if(world.loot[i].x==x&&world.loot[i].y==y) return false;
  return true;
}
static Point place(uint32_t &r,int minDist) {
  for(int n=0;n<600;n++) {
    int x=1+int(next(r)%(W-2)),y=1+int(next(r)%(H-2));
    if(freeCell(x,y)&&dist(x,y,world.startX,world.startY)>=minDist) return Point{uint8_t(x),uint8_t(y)};
  }
  for(int y=1;y<H-1;y++) for(int x=1;x<W-1;x++) if(freeCell(x,y)) return Point{uint8_t(x),uint8_t(y)};
  return Point{world.startX,world.startY}; // unreachable for the six-room layout
}
static void generate(bool spawn) {
  // Build-time fixed arrays never grow with floor count. No heap allocations.
  memset(&world,0,sizeof world);
  uint32_t r=game.seed^(0x9e3779b9u*game.floor);
  if(!r) r=0x31415927u;
  Point centres[6];
  const uint8_t cx[6]={4,12,19,4,12,19}, cy[6]={4,4,4,13,13,19};
  for(int i=0;i<6;i++) {
    int x=cx[i]+int(next(r)%3)-1,y=cy[i]+int(next(r)%3)-1;
    centres[i]=Point{uint8_t(x),uint8_t(y)};
    int w=2+int(next(r)%2),h=2+int(next(r)%2);
    for(int yy=y-h;yy<=y+h;yy++) for(int xx=x-w;xx<=x+w;xx++) carve(xx,yy);
    if(i) {
      int ax=centres[i-1].x,ay=centres[i-1].y;
      if(next(r)&1) {
        while(ax!=x) {carve(ax,ay); ax+=ax<x?1:-1;}
        while(ay!=y) {carve(ax,ay); ay+=ay<y?1:-1;}
      } else {
        while(ay!=y) {carve(ax,ay); ay+=ay<y?1:-1;}
        while(ax!=x) {carve(ax,ay); ax+=ax<x?1:-1;}
      }
      carve(x,y);
    }
  }
  world.startX=centres[0].x; world.startY=centres[0].y;
  world.exitX=centres[5].x; world.exitY=centres[5].y;
  world.tiles[index(world.exitX,world.exitY)]=STAIRS;
  // Optional corridor doors: no random draws here, and their indices are stable.
  for(int y=2;y<H-2&&world.doorCount<MAX_DOORS;y++) for(int x=2;x<W-2&&world.doorCount<MAX_DOORS;x++) {
    if(world.tiles[index(x,y)]!=FLOOR||dist(x,y,world.startX,world.startY)<5) continue;
    bool ns=world.tiles[index(x-1,y)]==WALL&&world.tiles[index(x+1,y)]==WALL&&
      world.tiles[index(x,y-1)]==FLOOR&&world.tiles[index(x,y+1)]==FLOOR;
    bool ew=world.tiles[index(x,y-1)]==WALL&&world.tiles[index(x,y+1)]==WALL&&
      world.tiles[index(x-1,y)]==FLOOR&&world.tiles[index(x+1,y)]==FLOOR;
    if((ns||ew)&&((x+y*3)%5==0)) {
      world.doors[world.doorCount++]=Point{uint8_t(x),uint8_t(y)};
      world.tiles[index(x,y)]=DOOR;
    }
  }
  // Always spawn from the same pristine state, even when rebuilding a save.
  // Use a temporary enemy array so preserved moving enemies cannot alter placement.
  Enemy saved[MAX_ENEMIES];
  memcpy(saved,game.enemies,sizeof saved);
  uint8_t oldCount=game.enemyCount;
  game.enemyCount=0;
  memset(game.enemies,0,sizeof game.enemies);
  for(int i=0;i<MAX_LOOT;i++) {
    Point p=place(r,i<3?0:4);
    uint8_t kind=i<3?uint8_t(POTION+i):uint8_t(next(r)%7);
    uint8_t value=uint8_t(kind==GOLD?5+game.floor*3+int(next(r)%12):
      (kind==WEAPON||kind==ARMOUR)?1+(game.floor+1)/2:1);
    world.loot[world.lootCount++]=Loot{p.x,p.y,kind,value};
  }
  uint8_t count=uint8_t(4+game.floor);
  for(uint8_t i=0;i<count;i++) {
    Point p=place(r,6);
    uint8_t kind=uint8_t(next(r)%(game.floor<3?3:4));
    if(game.floor==LAST_FLOOR && i==count-1) kind=WARDEN;
    game.enemies[game.enemyCount++]=Enemy{p.x,p.y,maxEnemyHP(kind,game.floor),kind,0};
  }
  if(!spawn) {memcpy(game.enemies,saved,sizeof saved); game.enemyCount=oldCount;}
  for(int i=0;i<world.doorCount;i++) if(game.opened&(1u<<i))
    world.tiles[index(world.doors[i].x,world.doors[i].y)]=FLOOR;
}
void rebuild() {generate(false); see();}
static void enterFloor() {
  game.opened=0; game.collected=0;
  memset(game.seen,0,sizeof game.seen);
  generate(true);
  game.x=world.startX;game.y=world.startY;game.facing=3;
  see(); event=ENTER;
}
void newGame(uint32_t seed,uint8_t hero) {
  memset(&game,0,sizeof game);
  game.seed=seed?seed:0x53424e44u;game.rng=game.seed^0xa511e9b3u;
  if(!game.rng) game.rng=1;
  game.hero=hero?1:0;game.floor=1;game.level=1;
  game.hp=game.maxhp=uint8_t(hero?26:34);
  game.mp=game.maxmp=uint8_t(hero?16:10);
  game.potions=2;game.tonics=1;game.rations=2;game.food=160;game.mode=ALIVE;
  enterFloor();
}
bool valid(const State &s) {
  if(!s.seed||!s.rng||s.floor<1||s.floor>LAST_FLOOR||s.hero>1||s.mode>WON||
     s.x>=W||s.y>=H||s.facing>3||!s.level||s.level>30||
     !s.maxhp||s.maxhp>120||s.hp>s.maxhp||s.maxmp>60||s.mp>s.maxmp||
     s.weapon>8||s.armour>8||s.potions>99||s.tonics>99||s.rations>99||
     s.enemyCount>MAX_ENEMIES||(s.mode==ALIVE&&!s.hp)||(s.mode==DEAD&&s.hp)) return false;
  for(int i=0;i<s.enemyCount;i++) {
    const Enemy &e=s.enemies[i];
    if(e.x>=W||e.y>=H||e.kind>WARDEN||e.hp>maxEnemyHP(e.kind,s.floor)||e.stun>2) return false;
    if(e.hp&&e.x==s.x&&e.y==s.y) return false;
    for(int j=0;j<i;j++) if(e.hp&&s.enemies[j].hp&&e.x==s.enemies[j].x&&e.y==s.enemies[j].y) return false;
  }
  return true;
}
bool restore(const State &s) {
  if(!valid(s)) return false;
  State old=game;
  game=s;rebuild();
  bool ok=passable(game.x,game.y)&&game.enemyCount==uint8_t(4+game.floor);
  for(int i=0;i<game.enemyCount;i++) if(game.enemies[i].hp&&!passable(game.enemies[i].x,game.enemies[i].y)) ok=false;
  if(!ok) {game=old;rebuild();return false;}
  return true;
}
static void addGold(uint16_t n) {game.gold=uint16_t(game.gold>65535u-n?65535u:game.gold+n);}
static void reward(Enemy &e) {
  uint16_t n=uint16_t(3+e.kind+game.floor/2);
  game.xp=uint16_t(game.xp+n);
  addGold(uint16_t(2+e.kind+game.floor));
  event=KILLED;detail=e.kind;
  while(game.xp>=uint16_t(12+game.level*5) && game.level<30) {
    game.xp=uint16_t(game.xp-(12+game.level*5));game.level++;
    game.maxhp=uint8_t(game.maxhp<=117?game.maxhp+3:120);
    game.maxmp=uint8_t(game.maxmp<60?game.maxmp+1:60);
    game.hp=uint8_t(game.hp+6>game.maxhp?game.maxhp:game.hp+6);
    game.mp=uint8_t(game.mp+3>game.maxmp?game.maxmp:game.mp+3);
    event=LEVEL_UP;
  }
}
static void damageEnemy(int i,int amount) {
  Enemy &e=game.enemies[i];
  if(amount>=e.hp) {e.hp=0;reward(e);}
  else {e.hp=uint8_t(e.hp-amount);event=HIT;detail=uint8_t(amount);}
}
static void hurt(int n) {
  if(n>=game.hp) {game.hp=0;game.mode=DEAD;event=DEFEAT;}
  else {game.hp=uint8_t(game.hp-n);event=HURT;detail=uint8_t(n);}
}
static void enemiesTurn() {
  for(int i=0;i<game.enemyCount&&game.mode==ALIVE;i++) {
    Enemy &e=game.enemies[i]; if(!e.hp) continue;
    if(e.stun) {e.stun--;continue;}
    int d=dist(e.x,e.y,game.x,game.y);
    if(d==1) {
      if(rnd(8)==0) continue;
      int hit=2+e.kind+game.floor/3-defence();
      hurt(hit>0?hit:1);continue;
    }
    if(d>8||!line(e.x,e.y,game.x,game.y)) continue;
    // Wraiths telegraph a ranged attack; shields and wall cover still work.
    if(e.kind==WRAITH && d<=4 && (game.turns%3)==0) {hurt(2);continue;}
    int dx=game.x>e.x?1:-1,dy=game.y>e.y?1:-1;
    bool xfirst=absn(game.x-e.x)>=absn(game.y-e.y);
    for(int attempt=0;attempt<2;attempt++) {
      bool xaxis=attempt==0?xfirst:!xfirst;
      int nx=e.x+(xaxis&&e.x!=game.x?dx:0),ny=e.y+(!xaxis&&e.y!=game.y?dy:0);
      if((nx!=e.x||ny!=e.y)&&passable(nx,ny)&&enemyAt(nx,ny)<0&&
        !(nx==game.x&&ny==game.y)) {e.x=uint8_t(nx);e.y=uint8_t(ny);break;}
    }
  }
}
static void finishTurn() {
  game.turns++;
  if(game.food) game.food--;
  else if(game.turns%8==0) {hurt(1);if(game.mode==ALIVE) event=HUNGRY;}
  if(game.mode==ALIVE) enemiesTurn();
  if(game.turns%5==0&&game.mp<game.maxmp) game.mp++;
  see();
}
static void collect() {
  int i=lootAt(game.x,game.y);if(i<0) return;
  const Loot &l=world.loot[i];game.collected|=uint16_t(1u<<i);
  switch(l.kind) {
    case GOLD:addGold(l.value);event=FOUND_GOLD;break;
    case POTION:if(game.potions<99)game.potions++;event=FOUND_POTION;break;
    case TONIC:if(game.tonics<99)game.tonics++;event=FOUND_TONIC;break;
    case RATION:if(game.rations<99)game.rations++;event=FOUND_FOOD;break;
    case WEAPON:if(l.value>game.weapon)game.weapon=l.value;else addGold(8);event=FOUND_WEAPON;break;
    case ARMOUR:if(l.value>game.armour)game.armour=l.value;else addGold(8);event=FOUND_ARMOUR;break;
    case TRAP:hurt(3+game.floor);if(game.mode==ALIVE)event=TRAPPED;break;
  }
}
bool move(int dx,int dy) {
  if(game.mode!=ALIVE||absn(dx)+absn(dy)!=1) return false;
  game.facing=uint8_t(dx<0?2:dx>0?3:dy<0?0:1);
  int x=game.x+dx,y=game.y+dy;
  if(!inside(x,y)||world.tiles[index(x,y)]==WALL) {event=BLOCKED;return false;}
  if(world.tiles[index(x,y)]==DOOR) {
    for(int i=0;i<world.doorCount;i++) if(world.doors[i].x==x&&world.doors[i].y==y) game.opened|=uint8_t(1u<<i);
    world.tiles[index(x,y)]=FLOOR;event=OPENED;finishTurn();return true;
  }
  int i=enemyAt(x,y);
  if(i>=0) {damageEnemy(i,attack()+rnd(3));finishTurn();return true;}
  game.x=uint8_t(x);game.y=uint8_t(y);event=STEP;
  collect();finishTurn();return true;
}
bool waitOrDescend() {
  if(game.mode!=ALIVE) return false;
  if(world.tiles[index(game.x,game.y)]==STAIRS) {
    if(game.floor==LAST_FLOOR) {
      for(int i=0;i<game.enemyCount;i++) if(game.enemies[i].kind==WARDEN&&game.enemies[i].hp) {event=BOSS_BLOCKS;return false;}
      game.mode=WON;event=VICTORY;return true;
    }
    game.floor++;game.food=uint8_t(game.food>210?240:game.food+30);
    enterFloor();event=DESCENDED;return true;
  }
  event=STEP;finishTurn();return true;
}
bool consume(uint8_t item) {
  if(game.mode!=ALIVE||item>2)return false;
  uint8_t *count=item==0?&game.potions:item==1?&game.tonics:&game.rations;
  if(!*count) {event=NO_ITEM;return false;}
  if((item==0&&game.hp==game.maxhp)||(item==1&&game.mp==game.maxmp)||(item==2&&game.food>=220)) {event=NO_ITEM;return false;}
  --*count;
  if(item==0) {game.hp=uint8_t(game.hp+16>game.maxhp?game.maxhp:game.hp+16);event=HEALED;}
  else if(item==1) {game.mp=game.maxmp;event=MANA;}
  else {game.food=uint8_t(game.food>150?240:game.food+90);event=FED;}
  finishTurn();return true;
}
bool cast(uint8_t spell) {
  if(game.mode!=ALIVE||spell>2)return false;
  const uint8_t costs[]={3,4,6};
  if(game.mp<costs[spell]) {event=NO_MANA;return false;}
  if(spell==1) {
    if(game.hp==game.maxhp) {event=NO_TARGET;return false;}
    game.mp=uint8_t(game.mp-4);game.hp=uint8_t(game.hp+12>game.maxhp?game.maxhp:game.hp+12);
    event=CAST_HEAL;finishTurn();return true;
  }
  if(spell==0) {
    const int8_t dx[]={0,0,-1,1},dy[]={-1,1,0,0};
    int target=-1;
    for(int n=1;n<=6;n++) {
      int x=game.x+dx[game.facing]*n,y=game.y+dy[game.facing]*n;
      if(!passable(x,y))break;target=enemyAt(x,y);if(target>=0)break;
    }
    if(target<0) {event=NO_TARGET;return false;}
    game.mp=uint8_t(game.mp-3);damageEnemy(target,7+game.level+(game.hero?2:0));
    if(game.enemies[target].hp)game.enemies[target].stun=1;
    if(event==HIT)event=CAST_BOLT;finishTurn();return true;
  }
  bool target=false;
  for(int i=0;i<game.enemyCount;i++) if(game.enemies[i].hp&&dist(game.x,game.y,game.enemies[i].x,game.enemies[i].y)<=2&&
    line(game.x,game.y,game.enemies[i].x,game.enemies[i].y))target=true;
  if(!target) {event=NO_TARGET;return false;}
  game.mp=uint8_t(game.mp-6);
  for(int i=0;i<game.enemyCount;i++) if(game.enemies[i].hp&&dist(game.x,game.y,game.enemies[i].x,game.enemies[i].y)<=2&&
    line(game.x,game.y,game.enemies[i].x,game.enemies[i].y)) {damageEnemy(i,6+game.level);game.enemies[i].stun=1;}
  if(event==HIT)event=CAST_NOVA;finishTurn();return true;
}
const char *message() {
  static const char *const words[]={
    "ENTER THE UNDERHOLLOW","FOOTSTEPS IN THE DARK","STONE BLOCKS THE WAY",
    "THE DOOR OPENS","YOUR BLADE STRIKES","FOE DEFEATED","YOU TAKE A HIT","MISSED",
    "GOLD FOUND","HEALING POTION FOUND","MANA TONIC FOUND","RATION FOUND",
    "WEAPON FOUND","ARMOUR FOUND","A HIDDEN TRAP!","LEVEL UP!","DEEPER INTO THE STONE",
    "DEFEAT THE WARDEN FIRST","POTION RESTORES HEALTH","MANA RESTORED","HUNGER EASED",
    "NOT NEEDED / EMPTY","NOT ENOUGH MANA","NO VALID TARGET","SPARK BOLT!",
    "MENDING LIGHT","STONE NOVA!","STARVING!","YOUR JOURNEY ENDS","THE RUNE IS YOURS"
  };
  return words[event<=VICTORY?event:ENTER];
}
} // namespace stone
