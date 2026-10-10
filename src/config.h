#pragma once
// ---------- AIPI Lite pins (from community projects) ----------
// Display
#define PIN_LCD_BL    3
#define PIN_LCD_DC    7
#define PIN_LCD_CS    15
#define PIN_LCD_SCLK  16
#define PIN_LCD_MOSI  17
#define PIN_LCD_RST   18
// Buttons (both active low)
#define PIN_BUTTON_TALK 42   // tap = next screen / "yes", hold = talk to Nova
#define PIN_BUTTON_BACK 1
#define PIN_BUTTON PIN_BUTTON_TALK   // name used by the clock/weather build    // tap = dismiss / "no", hold 4 s = reset Wi-Fi + Nova setup
// Power latch (must be on before the audio chip starts) and speaker amplifier
#define POWER_LATCH_PIN -1   // Phase 2 (audio) will switch this to 10
#define PIN_PA_EN       9
// Audio (ES8311 codec)
#define PIN_I2S_MCLK  6
#define PIN_I2S_BCLK  14
#define PIN_I2S_WS    12
#define PIN_I2S_DOUT  11
#define PIN_I2S_DIN   13
#define PIN_I2C_A     4   // the code tries A/B and then B/A for SDA/SCL
#define PIN_I2C_B     5

// ---------- Audio tuning ----------
#define AUDIO_SAMPLE_RATE 16000
#define MIC_GAIN_REG      4      // 0..7 = 0, 6, 12, 18, 24, 30, 36, 42 dB
#define MIC_SOFT_GAIN     2      // extra digital gain (1 = none)
#define SPK_VOLUME_REG    0xBF   // 0xBF = 0 dB, higher = louder (0xC8 about +4.5 dB)
#define VAD_MIN_THRESHOLD 250    // lower = more sensitive to quiet speech

// ---------- Display ----------
// Driver type, offsets, inversion and colour order come from the presets in display.h.
#define LCD_WIDTH       128
#define LCD_HEIGHT      128
#define LCD_ROTATION    0      // 0-3 (change if the picture is sideways)
#define LCD_BRIGHTNESS  200    // 0-255
