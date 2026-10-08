#pragma once
// ---------- AIPI Lite pins (from community projects; see README) ----------
#define PIN_LCD_BL    3
#define PIN_LCD_DC    7
#define PIN_LCD_CS    15
#define PIN_LCD_SCLK  16
#define PIN_LCD_MOSI  17
#define PIN_LCD_RST   18
#define PIN_BUTTON    42   // active low

// GPIO10 is the board's power latch. Phase 1 leaves it alone (-1).
// Only set to 10 if the device switches off by itself when unplugged/idle.
#define POWER_LATCH_PIN -1

// ---------- Display ----------
// Driver type, offsets, inversion and colour order come from the presets in display.h.
// At every boot the test pattern shows for 7 seconds: press the button during that time
// to try the next preset. The one you leave on is remembered.
#define LCD_WIDTH       128
#define LCD_HEIGHT      128
#define LCD_ROTATION    0      // 0-3
#define LCD_BRIGHTNESS  200    // 0-255
