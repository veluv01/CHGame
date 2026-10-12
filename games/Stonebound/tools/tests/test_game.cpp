// SPDX-License-Identifier: MIT
#include "../../Game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
using namespace stone;
static unsigned checks=0;
static void check(bool value,const char *why) {
  checks++;if(!value){fprintf(stderr,"FAIL: %s (seed=%u floor=%u turn=%u)\n",why,game.seed,game.floor,game.turns);abort();}
}
static void floorShape() {
  bool reached[CELLS]={};uint16_t queue[CELLS];int front=0,back=0;
  int start=world.startY*W+world.startX;reached[start]=true;queue[back++]=uint16_t(start);
  const int dx[]={0,0,-1,1},dy[]={-1,1,0,0};
  while(front<back) {
    int pos=queue[front++],x=pos%W,y=pos/W;
    for(int k=0;k<4;k++) {
      int xx=x+dx[k],yy=y+dy[k];
      if(xx<0||xx>=W||yy<0||yy>=H)continue;
      int n=yy*W+xx;
      if(!reached[n]&&world.tiles[n]!=WALL){reached[n]=true;queue[back++]=uint16_t(n);}
    }
  }
  check(reached[world.exitY*W+world.exitX],"stairs reachable through openable doors");
  for(int i=0;i<CELLS;i++)if(world.tiles[i]!=WALL)check(reached[i],"all rooms connected");
  for(int x=0;x<W;x++)check(world.tiles[x]==WALL&&world.tiles[(H-1)*W+x]==WALL,"horizontal border");
  for(int y=0;y<H;y++)check(world.tiles[y*W]==WALL&&world.tiles[y*W+W-1]==WALL,"vertical border");
  for(int i=0;i<game.enemyCount;i++) {
    check(passable(game.enemies[i].x,game.enemies[i].y),"enemy on floor");
    for(int j=0;j<i;j++)check(game.enemies[i].x!=game.enemies[j].x||game.enemies[i].y!=game.enemies[j].y,"enemy placement unique");
  }
  for(int i=0;i<world.lootCount;i++) {
    check(passable(world.loot[i].x,world.loot[i].y),"loot on floor");
    check(!(world.loot[i].x==world.exitX&&world.loot[i].y==world.exitY),"exit clear");
    for(int j=0;j<i;j++)check(world.loot[i].x!=world.loot[j].x||world.loot[i].y!=world.loot[j].y,"loot placement unique");
  }
}
static void openArena() {
  newGame(42);
  memset(world.tiles,FLOOR,sizeof world.tiles);world.lootCount=0;
  memset(game.enemies,0,sizeof game.enemies);game.enemyCount=0;
  game.x=10;game.y=10;game.hp=game.maxhp;game.food=200;see();
}
static void generation() {
  for(uint32_t seed=1;seed<=300;seed++)for(uint8_t floor=1;floor<=LAST_FLOOR;floor++) {
    // A real floor transition spawns the complete next-floor population.
    newGame(seed,seed&1);
    for(uint8_t n=1;n<floor;n++) {
      game.x=world.exitX;game.y=world.exitY;waitOrDescend();
    }
    check(valid(game),"generated state valid");floorShape();
    State s=game;World w=world;
    check(restore(s),"restore generated floor");
    check(memcmp(&s,&game,sizeof s)==0,"restore state identical");
    check(memcmp(&w,&world,sizeof w)==0,"restore world identical");
    newGame(seed,seed&1);
    for(uint8_t n=1;n<floor;n++){game.x=world.exitX;game.y=world.exitY;waitOrDescend();}
    check(memcmp(&s,&game,sizeof s)==0,"same seed same floor");
  }
}
static void rules() {
  openArena();uint16_t turn=game.turns;world.tiles[10*W+11]=WALL;
  check(!move(1,0)&&game.turns==turn,"wall bump costs no turn");
  check(!move(1,1)&&game.turns==turn,"diagonal rejected");
  world.tiles[10*W+11]=DOOR;world.doorCount=1;world.doors[0]=Point{11,10};
  check(move(1,0)&&game.x==10&&game.opened==1,"door opens before walking");
  check(game.turns==turn+1,"opening door spends one turn");
  check(move(1,0)&&game.x==11,"walk through opened door");
  openArena();game.enemyCount=1;game.enemies[0]=Enemy{11,10,1,RAT,0};
  uint16_t gold=game.gold;
  check(move(1,0)&&game.enemies[0].hp==0&&game.x==10,"bump attacks without stepping");
  check(game.gold>gold&&game.xp>0,"kill gives rewards");
  openArena();game.enemyCount=1;game.enemies[0]=Enemy{12,10,8,SKELETON,0};game.facing=3;
  check(cast(0)&&game.mp==game.maxmp-3&&game.enemies[0].hp==0,"bolt hits facing target");
  openArena();game.enemyCount=1;game.enemies[0]=Enemy{12,10,8,SKELETON,0};game.facing=3;
  world.tiles[10*W+11]=WALL;uint8_t mp=game.mp;turn=game.turns;
  check(!cast(0)&&game.mp==mp&&game.turns==turn,"wall blocks bolt without spending mana");
  game.mp=0;check(!cast(2)&&game.turns==turn,"insufficient mana costs no turn");
  openArena();game.hp=3;uint8_t pots=game.potions;
  check(consume(0)&&game.hp==19&&game.potions==pots-1,"potion restores sixteen HP");
  game.hp=game.maxhp;turn=game.turns;
  check(!consume(0)&&game.turns==turn,"full-health potion not wasted");
  game.hp=1;check(cast(1)&&game.hp==13,"mending light restores HP");
  game.food=0;game.turns=7;game.hp=1;check(waitOrDescend()&&game.mode==DEAD,"starvation can end a run");
  turn=game.turns;check(!move(1,0)&&!cast(0)&&game.turns==turn,"dead run cannot act");
  openArena();world.tiles[10*W+11]=WALL;see();
  check(visible(11,10)&&!visible(12,10),"wall visible and blocks vision");
  newGame(1);for(int n=1;n<LAST_FLOOR;n++){game.x=world.exitX;game.y=world.exitY;check(waitOrDescend(),"floor transition");}
  game.x=world.exitX;game.y=world.exitY;
  check(!waitOrDescend()&&event==BOSS_BLOCKS,"warden blocks final exit");
  for(int i=0;i<game.enemyCount;i++)if(game.enemies[i].kind==WARDEN)game.enemies[i].hp=0;
  check(waitOrDescend()&&game.mode==WON,"warden defeat unlocks victory");
}
static void saves() {
  newGame(777);
  for(int i=0;i<8;i++){move(i&1?1:0,i&1?0:1);}
  State snapshot=game;World geometry=world;
  check(restore(snapshot),"save resumes an explored floor");
  check(memcmp(&snapshot,&game,sizeof game)==0,"all dynamic state retained");
  check(memcmp(&geometry,&world,sizeof world)==0,"loot and geometry retained");
  State broken=snapshot;broken.enemyCount=255;check(!restore(broken),"invalid enemy count rejected");
  broken=snapshot;broken.x=255;check(!restore(broken),"invalid position rejected");
  broken=snapshot;broken.maxhp=0;check(!restore(broken),"invalid health rejected");
  broken=snapshot;broken.x=0;broken.y=0;check(!restore(broken),"wall-position save rejected");
  check(memcmp(&snapshot,&game,sizeof game)==0,"rejected save leaves live state intact");
}
static void soak() {
  uint32_t r=1234;unsigned actions=0;
  for(int run=0;run<120;run++) {
    newGame(uint32_t(run)+9900,run&1);
    for(int i=0;i<450&&game.mode==ALIVE;i++) {
      r=r*1664525u+1013904223u;int n=int(r%11);
      if(n<4) {static const int dx[]={0,0,-1,1},dy[]={-1,1,0,0};move(dx[n],dy[n]);}
      else if(n<7)consume(uint8_t(n-4));else if(n<10)cast(uint8_t(n-7));else waitOrDescend();
      check(valid(game),"fuzzed state valid");check(passable(game.x,game.y),"player stays on floor");
      if(i%31==0){State s=game;check(restore(s),"fuzzed save roundtrip");check(memcmp(&s,&game,sizeof s)==0,"fuzzed state stable");}
      actions++;
    }
  }
  printf("stonebound soak: %u attempted actions\n",actions);
}
int main() {
  check(sizeof(State)<=244,"save fits one flash record");
  check(sizeof(State)+sizeof(World)<1200,"game working set fits budget");
  generation();rules();saves();soak();
  printf("stonebound: %u checks passed; State=%zu World=%zu bytes\n",checks,sizeof(State),sizeof(World));
}
