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

// ---------- Display calibration (change these if the picture looks wrong) ----------
#define LCD_WIDTH       128
#define LCD_HEIGHT      128
#define LCD_OFFSET_X    0      // try 2, 32 or 80 if the picture is shifted
#define LCD_OFFSET_Y    0      // try 1, 3, 32 or 80 if the picture is shifted
#define LCD_INVERT      true   // flip to false if colours look negative
#define LCD_RGB_ORDER   false  // flip to true if red and blue are swapped
#define LCD_ROTATION    0      // 0-3
#define LCD_BRIGHTNESS  200    // 0-255
