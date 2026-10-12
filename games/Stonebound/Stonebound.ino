// SPDX-License-Identifier: MIT
#include <CHGame.h>
#include "Game.h"
#include "View.h"
#ifdef CHSIM
#include <stdlib.h>
#endif
using namespace stone;
#if CHGAME_DEBUG
#include <stdio.h>
#endif
static view::Screen screen=view::TITLE,helpBack=view::TITLE;
static uint8_t selection=0,hero=0;
static bool hasSave=false,savePending=false;
static State saved;
static uint32_t quitStart=0,noticeUntil=0;
static bool quitHeld=false;
static const char *notice=nullptr;
constexpr uint32_t SAVE_MAGIC=save::magic("STBN");
static void tell(const char *s) {notice=s;noticeUntil=millis()+1800;}
static void saveNow() {
  if(save::store(SAVE_MAGIC,1,game)) {saved=game;hasSave=game.mode==ALIVE;tell("JOURNEY SAVED");}
  else tell("SAVE FAILED - STAY IN GAME");
  savePending=false;
}
static uint32_t runSeed() {
#ifdef CHSIM
  const char *s=getenv("STONEBOUND_SEED");
  if(s)return uint32_t(strtoul(s,nullptr,0));
#endif
  return (micros()<<7)^millis()^chgame.frameCount^0x53424e44u;
}
static void beginJourney() {
  newGame(runSeed(),hero);screen=view::PLAY;savePending=true;
}
#if CHGAME_DEBUG
static bool debugHook(char command,const char *) {
#ifdef CHSIM
  if(command=='v') {
    // Reset volatile game state, then reread the simulator's persistent pages.
    gfx_wait(); game=State{}; world=World{}; saved=State{};
    hasSave=save::load(SAVE_MAGIC,1,saved)&&valid(saved)&&saved.mode==ALIVE;
    hero=hasSave?saved.hero:0;screen=view::TITLE;selection=0;
    quitHeld=false;savePending=false;notice=nullptr;return true;
  }
#endif
  if(command!='u')return false;
  char line[160];
  snprintf(line,sizeof line,"STATE screen=%u mode=%u floor=%u hp=%u mp=%u turns=%u x=%u y=%u save=%u hero=%u\n",
    unsigned(screen),unsigned(game.mode),unsigned(game.floor),unsigned(game.hp),unsigned(game.mp),
    unsigned(game.turns),unsigned(game.x),unsigned(game.y),unsigned(hasSave),unsigned(hero));
  dbg::print(line);return true;
}
#endif
void setup() {
  chgame.boot();dbg::begin("STBN 0.1.0");
#if CHGAME_DEBUG
  dbg::hook=debugHook;
#endif
  chgame.startExits=false;chgame.setFrameRate(30);
  gfx_begin(GFX_DIV4,GFX_12BPP);
  hasSave=save::load(SAVE_MAGIC,1,saved)&&valid(saved)&&saved.mode==ALIVE;
  if(hasSave)hero=saved.hero;
}
void loop() {
  dbg::poll();
  if(!chgame.nextFrame())return;
  dbg::markUpdateStart();
  chgame.pollButtons();gfx_wait();
  uint32_t now=millis();
  if(chgame.pressed(START_BUTTON)) {
    if(!quitHeld){quitHeld=true;quitStart=now;}
    if(uint32_t(now-quitStart)>=5000) {
      if(game.mode==ALIVE&&screen!=view::TITLE&&screen!=view::STORY) {
        if(save::store(SAVE_MAGIC,1,game))chgame.exitToMenu();
        tell("SAVE FAILED - TRY AGAIN");quitStart=now;
      } else chgame.exitToMenu();
    }
  } else {
    if(quitHeld&&uint32_t(now-quitStart)<5000) {
      if(screen==view::PLAY){screen=view::PAUSE;selection=0;}
      else if(screen==view::PAUSE)screen=view::PLAY;
      else if(screen==view::TITLE){helpBack=screen;screen=view::HELP;}
    }
    quitHeld=false;
  }
  bool action=false;
  if(!quitHeld) {
    if(screen==view::TITLE) {
      if(chgame.justPressed(SELECT_BUTTON))hero^=1;
      if(chgame.justPressed(B_BUTTON))screen=view::STORY;
      else if(chgame.justPressed(A_BUTTON)) {
        if(hasSave&&restore(saved)){screen=view::PLAY;tell("JOURNEY CONTINUED");}
        else {hasSave=false;screen=view::STORY;}
      }
    } else if(screen==view::STORY) {
      if(chgame.justPressed(A_BUTTON))beginJourney();
      else if(chgame.justPressed(B_BUTTON))screen=view::TITLE;
    } else if(screen==view::PLAY) {
      if(chgame.justPressed(B_BUTTON)){screen=view::SPELLS;selection=0;}
      else if(chgame.justPressed(SELECT_BUTTON)){screen=view::BAG;selection=0;}
      else if(chgame.repeat(UP_BUTTON,9,5))action=move(0,-1);
      else if(chgame.repeat(DOWN_BUTTON,9,5))action=move(0,1);
      else if(chgame.repeat(LEFT_BUTTON,9,5))action=move(-1,0);
      else if(chgame.repeat(RIGHT_BUTTON,9,5))action=move(1,0);
      else if(chgame.justPressed(A_BUTTON)){
        uint8_t floor=game.floor;action=waitOrDescend();if(game.floor!=floor)savePending=true;
      }
    } else if(screen==view::BAG||screen==view::SPELLS||screen==view::PAUSE) {
      uint8_t count=screen==view::PAUSE?4:3;
      if(chgame.repeat(UP_BUTTON))selection=uint8_t((selection+count-1)%count);
      if(chgame.repeat(DOWN_BUTTON))selection=uint8_t((selection+1)%count);
      if(chgame.justPressed(B_BUTTON))screen=view::PLAY;
      else if(chgame.justPressed(A_BUTTON)) {
        if(screen==view::BAG){action=consume(selection);screen=view::PLAY;}
        else if(screen==view::SPELLS){action=cast(selection);screen=view::PLAY;}
        else if(selection==0)screen=view::PLAY;
        else if(selection==1)savePending=true;
        else if(selection==2){helpBack=view::PAUSE;screen=view::HELP;}
        else if(save::store(SAVE_MAGIC,1,game))chgame.exitToMenu();
        else tell("SAVE FAILED - STAY IN GAME");
      }
    } else if(screen==view::HELP) {
      if(chgame.justPressed(A_BUTTON)||chgame.justPressed(B_BUTTON))screen=helpBack;
    } else if(screen==view::END) {
      if(chgame.justPressed(A_BUTTON))screen=view::TITLE;
      else if(chgame.justPressed(B_BUTTON))screen=view::STORY;
    }
  }
  if(action){notice=nullptr;if(game.mode!=ALIVE){screen=view::END;savePending=true;}}
  if(savePending)saveNow();
  if(notice&&int32_t(now-noticeUntil)>=0)notice=nullptr;
  uint16_t hold=quitHeld?uint16_t(now-quitStart>5000?5000:now-quitStart):0;
  dbg::markRenderStart();
  view::draw(screen,selection,hasSave,hero,chgame.frameCount,notice,hold);
  dbg::markRenderEnd();
  gfx_flushAsync();
}
