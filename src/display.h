#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "config.h"

// Display presets for the AIPI Lite's 128x128 screen.
// Preset 1 is the one that worked in the first working video (ST7735S driver, window offset 2,3,
// colours not inverted, blue-green-red order). Preset 0 is a "grid" view for diagnosing.
struct LcdPreset {
  bool st7789;  // true = ST7789 driver, false = ST7735S driver
  bool raw;     // true = show the controller's whole memory (grid view)
  int offX, offY;
  bool invert;
  bool bgr;
};

static const LcdPreset LCD_PRESETS[] = {
    {false, true, 0, 0, false, true},   // 0: grid view
    {false, false, 2, 3, false, true},  // 1: worked before
    {false, false, 2, 1, false, false}, // 2
    {false, false, 2, 1, false, true},  // 3
    {false, false, 2, 1, true, false},  // 4
    {false, false, 0, 0, false, true},  // 5
    {true, false, 0, 0, true, true},    // 6: ST7789
    {true, false, 2, 1, true, true},    // 7: ST7789
};
static const int LCD_PRESET_COUNT = sizeof(LCD_PRESETS) / sizeof(LCD_PRESETS[0]);
#define LCD_DEFAULT_PRESET 1

static inline int lcdPresetIndex(int idx) { return ((idx % LCD_PRESET_COUNT) + LCD_PRESET_COUNT) % LCD_PRESET_COUNT; }
static inline bool lcdPresetIsRaw(int idx) { return LCD_PRESETS[lcdPresetIndex(idx)].raw; }

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _p7789;
  lgfx::Panel_ST7735S _p7735;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;

  template <class PANEL>
  void setupPanel(PANEL& pn, const LcdPreset& p, bool setMemory) {
    auto cfg = pn.config();
    cfg.pin_cs = PIN_LCD_CS;
    cfg.pin_rst = PIN_LCD_RST;
    cfg.pin_busy = -1;
    if (setMemory) {  // ST7789 has 240x320 memory; the ST7735S keeps its own defaults
      cfg.memory_width = 240;
      cfg.memory_height = 320;
    }
    cfg.panel_width = p.raw ? cfg.memory_width : LCD_WIDTH;
    cfg.panel_height = p.raw ? cfg.memory_height : LCD_HEIGHT;
    cfg.offset_x = p.raw ? 0 : p.offX;
    cfg.offset_y = p.raw ? 0 : p.offY;
    cfg.offset_rotation = 0;
    cfg.readable = false;
    cfg.invert = p.invert;
    cfg.rgb_order = p.bgr;
    cfg.dlen_16bit = false;
    cfg.bus_shared = false;
    pn.config(cfg);
  }

 public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 20000000;
      cfg.freq_read = 0;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = PIN_LCD_SCLK;
      cfg.pin_mosi = PIN_LCD_MOSI;
      cfg.pin_miso = -1;
      cfg.pin_dc = PIN_LCD_DC;
      _bus.config(cfg);
      _p7789.setBus(&_bus);
      _p7735.setBus(&_bus);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl = PIN_LCD_BL;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _p7789.setLight(&_light);
      _p7735.setLight(&_light);
    }
    applyPreset(LCD_DEFAULT_PRESET);
  }

  // Call before init().
  void applyPreset(int idx) {
    const LcdPreset& p = LCD_PRESETS[lcdPresetIndex(idx)];
    if (p.st7789) {
      setupPanel(_p7789, p, true);
      setPanel(&_p7789);
    } else {
      setupPanel(_p7735, p, false);
      setPanel(&_p7735);
    }
  }
};
