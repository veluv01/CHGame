// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
namespace view {
enum Screen : uint8_t { TITLE, STORY, PLAY, BAG, SPELLS, PAUSE, HELP, END };
void draw(Screen screen,uint8_t selection,bool hasSave,uint8_t hero,uint32_t frame,
          const char *notice=nullptr,uint16_t quitProgress=0);
}
