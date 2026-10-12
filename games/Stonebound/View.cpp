// SPDX-License-Identifier: MIT
#include <CHGame.h>
#include "Game.h"
#include "View.h"
using namespace stone;
namespace view {
static const uint16_t palette[16]={
  0x10a4,0x1926,0x322a,0x532e,0xa597,0xef5a,0xe56a,0xcba7,
  0xba2a,0x6650,0x3ae9,0x4554,0x5c37,0xa41a,0xd5fd,0x2187
};
static const uint8_t art[7][8]={
  {0x18,0x3c,0x18,0x7e,0x5a,0x3c,0x24,0x66},
  {0x00,0x02,0x2e,0x7c,0x7e,0x28,0x00,0x00},
  {0x00,0x81,0xdb,0xff,0x7e,0x24,0x00,0x00},
  {0x3c,0x7e,0x5a,0x3c,0x18,0x3c,0x5a,0x24},
  {0x18,0x3c,0x7e,0x5a,0x7e,0x7e,0x5a,0x24},
  {0x66,0xff,0xdb,0xff,0x7e,0x7e,0x66,0xe7},
  {0x18,0x3c,0x7e,0xff,0x7e,0x3c,0x18,0x00}
};
static void glyph(int x,int y,int id,uint8_t c) {
  for(int row=0;row<8;row++) for(int col=0;col<8;col++)
    if(art[id][row]&(0x80u>>col)) gfx_pixel(x+col,y+row,c);
}
static void centred(int y,const char *s,uint8_t c=5) {text35((128-text35Width(s))/2,y,s,c);}
static void number(int x,int y,unsigned n,uint8_t c=5) {
  char s[11];int k=0;do{s[k++]=char('0'+n%10);n/=10;}while(n&&k<10);
  for(int a=0,b=k-1;a<b;a++,b--){char t=s[a];s[a]=s[b];s[b]=t;}s[k]=0;text35(x,y,s,c);
}
static void bar(int x,int y,int w,int val,int max,uint8_t c) {
  gfx_fillRect(x,y,w,4,1);if(max&&val)gfx_fillRect(x,y,(w*val)/max,4,c);
}
static void tile(int x,int y,int xx,int yy) {
  if(!seen(xx,yy))return;
  bool lit=visible(xx,yy);
  uint8_t t=world.tiles[yy*W+xx],base=lit?15:1;
  gfx_fillRect(x,y,8,8,base);
  if(t==WALL) {
    gfx_fillRect(x,y,8,8,lit?2:1);
    gfx_hline(x,y,8,lit?3:2);gfx_hline(x,y+4,8,lit?1:0);
    gfx_vline(x+((yy&1)?2:5),y,4,lit?1:0);
  } else if(t==DOOR) {
    gfx_fillRect(x+1,y+1,6,7,lit?7:2);
    gfx_vline(x+3,y+2,5,lit?6:3);gfx_pixel(x+5,y+4,lit?6:3);
  } else if(t==STAIRS) {
    for(int j=0;j<3;j++)gfx_hline(x+1+j,y+2+j*2,6-j,lit?6:3);
  } else if(lit) {
    gfx_pixel(x+1,y+5,2);if((xx+yy)%3==0)gfx_pixel(x+6,y+2,2);
  }
  if(!lit)return;
  int l=lootAt(xx,yy);
  if(l>=0) {
    uint8_t k=world.loot[l].kind;
    if(k==stone::GOLD) {gfx_fillRect(x+2,y+3,4,3,6);gfx_pixel(x+3,y+2,5);}
    else if(k==POTION||k==TONIC) {
      gfx_fillRect(x+3,y+1,2,2,5);gfx_fillRect(x+2,y+3,4,4,k==POTION?9:12);gfx_pixel(x+3,y+3,5);
    } else if(k==RATION) {gfx_fillRect(x+2,y+2,4,4,7);gfx_hline(x+2,y+2,4,6);}
    else if(k==WEAPON) {gfx_line(x+2,y+6,x+6,y+1,5);gfx_line(x+2,y+3,x+5,y+6,6);}
    else if(k==ARMOUR) {gfx_fillRect(x+2,y+2,4,4,12);gfx_hline(x+1,y+1,6,4);}
  }
  int e=enemyAt(xx,yy);
  if(e>=0) {
    uint8_t kind=game.enemies[e].kind;
    static const uint8_t colours[]={7,13,5,11,8};
    glyph(x,y,1+kind,colours[kind]);
    if(game.enemies[e].stun)gfx_pixel(x,y,14);
  }
  if(xx==game.x&&yy==game.y) {
    glyph(x,y,0,game.hero?14:9);
    const int8_t fx[]={3,3,-1,8},fy[]={-1,8,3,3};
    gfx_pixel(x+fx[game.facing],y+fy[game.facing],6);
  }
}
static void dungeon() {
  int camx=int(game.x)-7,camy=int(game.y)-5;
  if(camx<0)camx=0;if(camx>W-15)camx=W-15;
  if(camy<0)camy=0;if(camy>H-11)camy=H-11;
  for(int y=0;y<11;y++)for(int x=0;x<15;x++)tile(4+x*8,20+y*8,camx+x,camy+y);
  text35(3,2,"F",6);number(8,2,game.floor,6);text35(20,2,"LV",4);number(32,2,game.level);
  text35(53,2,"G",6);number(59,2,game.gold,6);
  text35(92,2,game.food<30?"HUNGRY":"FOOD",game.food<30?8:4);
  number(113,2,game.food,game.food<30?8:4);
  text35(3,11,"HP",9);bar(14,11,43,game.hp,game.maxhp,9);number(59,10,game.hp,9);
  text35(75,11,"MP",12);bar(87,11,26,game.mp,game.maxmp,12);number(115,10,game.mp,12);
  text35(3,111,message(),event==HURT||event==TRAPPED||event==HUNGRY?8:4);
  text35(3,120,"A WAIT/STAIRS B SPELL SELECT BAG",3);
  for(int i=0;i<game.enemyCount;i++)if(game.enemies[i].kind==WARDEN&&game.enemies[i].hp&&
      visible(game.enemies[i].x,game.enemies[i].y)) {
    gfx_fillRect(14,94,100,13,0);centred(95,"STONE WARDEN",8);
    bar(18,102,92,game.enemies[i].hp,28+game.floor,8);
  }
}
static void backdrop() {
  for(int y=0;y<16;y++) {
    gfx_hline(0,y*8,128,1);
    for(int x=(y&1)*8;x<128;x+=16)gfx_vline(x,y*8,8,1);
  }
}
static void box(int y,int h) {panel(8,y,112,h,3,0,3);}
static void choice(int y,int n,uint8_t sel,const char *s,uint8_t c=5) {
  if(n==sel)gfx_fillRect(12,y-2,104,9,2);
  text35(16,y,n==sel?">":" ",6);text35(24,y,s,c);
}
void draw(Screen screen,uint8_t selection,bool hasSave,uint8_t hero,uint32_t frame,
          const char *notice,uint16_t quitProgress) {
  (void)frame;gfx_setPalette(palette,16);gfx_clear(0);
  if(screen==PLAY||screen==BAG||screen==SPELLS||screen==PAUSE) dungeon();
  else backdrop();
  switch(screen) {
    case TITLE:
      centred(14,"A DUNGEON ROGUELIKE",4);
      text35x2((128-10*8)/2,30,"STONEBOUND",6);
      glyph(37,52,3,4);glyph(60,52,6,13);glyph(83,52,0,9);
      centred(70,hasSave?"A CONTINUE":"A BEGIN",5);
      centred(82,"B NEW JOURNEY",5);
      centred(94,hero?"SELECT: ARCANIST":"SELECT: DELVER",14);
      centred(108,"START: HOW TO PLAY",4);
      centred(120,"SIX FLOORS. ONE ANCIENT RUNE.",3);
      break;
    case STORY:
      box(12,100);centred(22,"THE UNDERHOLLOW",6);
      centred(40,"THE TOWN'S WARD HAS FAILED.",5);
      centred(52,"BELOW IT, SIX STONE FLOORS",4);
      centred(64,"GUARD THE LAST BINDING RUNE.",4);
      centred(76,"DEFEAT THE STONE WARDEN.",8);
      centred(88,"BRING THE LIGHT BACK.",9);
      centred(103,"A ENTER   B BACK",6);
      break;
    case BAG:
      box(24,78);centred(29,"SATCHEL",6);
      choice(43,0,selection,"HEALING POTION");number(103,43,game.potions,9);
      choice(55,1,selection,"MANA TONIC");number(103,55,game.tonics,12);
      choice(67,2,selection,"RATION");number(103,67,game.rations,6);
      text35(16,82,"ATK",4);number(34,82,attack());text35(57,82,"DEF",4);number(75,82,defence());
      centred(93,"A USE   B BACK",4);
      break;
    case SPELLS:
      box(24,78);centred(29,"SPELLBOOK",13);
      choice(43,0,selection,"SPARK BOLT     3 MP",12);
      choice(55,1,selection,"MENDING LIGHT  4 MP",9);
      choice(67,2,selection,"STONE NOVA     6 MP",13);
      centred(81,selection==0?"BOLT FOLLOWS YOUR FACING":selection==1?"RESTORES 12 HEALTH":"HITS NEARBY FOES + STUN",4);
      centred(93,"A CAST   B BACK",4);
      break;
    case PAUSE:
      box(21,87);centred(27,"PAUSED",6);
      choice(42,0,selection,"RESUME");
      choice(56,1,selection,"SAVE JOURNEY",9);
      choice(70,2,selection,"HOW TO PLAY");
      choice(84,3,selection,"SAVE + EXIT",6);
      centred(98,"A CHOOSE   B RESUME",4);
      break;
    case HELP:
      box(5,115);centred(12,"HOW TO PLAY",6);
      text35(14,27,"D-PAD WALK / BUMP TO ATTACK",5);
      text35(14,39,"A WAIT; ON STAIRS: DESCEND",5);
      text35(14,51,"B SPELLBOOK  SELECT SATCHEL",5);
      text35(14,63,"START PAUSE / SAVE",5);
      text35(14,75,"HOLD START 5S: SAVE + EXIT",6);
      text35(14,87,"MENUS DO NOT SPEND A TURN.",4);
      text35(14,99,"EAT RATIONS BEFORE STARVING.",9);
      centred(112,"A / B BACK",4);
      break;
    case END:
      box(10,108);
      centred(22,game.mode==WON?"THE LIGHT RETURNS":"LOST IN THE UNDERHOLLOW",game.mode==WON?9:8);
      glyph(60,42,game.mode==WON?6:3,game.mode==WON?13:4);
      centred(62,game.mode==WON?"YOU CLAIMED THE BINDING RUNE.":"THE STONE REMEMBERS YOU.",5);
      text35(23,78,"FLOOR",4);number(47,78,game.floor);
      text35(65,78,"GOLD",6);number(88,78,game.gold,6);
      text35(23,90,"LEVEL",4);number(47,90,game.level);
      text35(65,90,"TURNS",4);number(88,90,game.turns);
      centred(108,"A TITLE   B NEW JOURNEY",6);
      break;
    case PLAY:break;
  }
  if(notice) {gfx_fillRect(0,110,128,9,0);centred(112,notice,6);}
  if(quitProgress) {
    gfx_fillRect(5,110,118,17,0);centred(111,"SAVING + RETURNING TO MENU",6);
    bar(9,121,110,quitProgress,5000,6);
  }
}
}
