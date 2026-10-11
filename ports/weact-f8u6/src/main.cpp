// Hardware check for the WeAct F8U6 variant. Shared upstream CHGame APIs only.
#include <Arduino.h>
#include <CHGame.h>
#include <CHGfx.h>

static const char *const names[] = {"A", "B", "UP", "DOWN", "LEFT", "RIGHT", "START", "SELECT"};

void setup() {
    chgame.boot();
    chgame.setFrameRate(30);
    gfx_begin(GFX_DIV4, GFX_16BPP); // 12 MHz is a useful breadboard starting point.
    gfx_setPaletteEntry(0, 0x0000);
    gfx_setPaletteEntry(1, 0xFFFF);
    gfx_setPaletteEntry(2, 0x07E0);
    Serial.begin(); // USB baud comes from the host; begin() takes a FIFO size.
}

void loop() {
    if (!chgame.nextFrame()) return;
    chgame.pollButtons();
    gfx_clear(0);
    gfx_text(4, 4, "WEACT F8U6", 1);
    gfx_text(4, 16, "HOLD START: MENU", 1);
    for (uint8_t i = 0; i < 8; ++i) {
        bool held = chgame.anyPressed(uint8_t(1u << i));
        gfx_fillRect(4, 30 + i * 11, 120, 9, held ? 2 : 0);
        gfx_text(8, 31 + i * 11, names[i], held ? 0 : 1);
    }
    gfx_flush();
    if (chgame.justPressedMask()) {
        Serial.print("BUTTONS ");
        Serial.println(chgame.buttons(), HEX);
    }
}
