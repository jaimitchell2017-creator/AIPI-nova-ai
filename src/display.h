#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "config.h"

// Display presets for the AIPI Lite's 128x128 ST7789 screen (BGR colour order, inverted).
// Preset 0 is a "grid" view that shows the whole controller memory with coordinates, so we can
// read off where the 128x128 window really sits. Presets 1+ try likely window offsets.
struct LcdPreset {
  bool raw;  // true = show the whole 240x320 controller memory (grid view)
  int offX, offY;
  bool invert;
  bool bgr;
};

static const LcdPreset LCD_PRESETS[] = {
    {true, 0, 0, true, true},    // 0: grid view
    {false, 0, 0, true, true},   // 1
    {false, 2, 1, true, true},   // 2
    {false, 0, 32, true, true},  // 3
    {false, 32, 0, true, true},  // 4
    {false, 0, 80, true, true},  // 5
    {false, 80, 0, true, true},  // 6
    {false, 2, 3, true, true},   // 7
};
static const int LCD_PRESET_COUNT = sizeof(LCD_PRESETS) / sizeof(LCD_PRESETS[0]);

static inline int lcdPresetIndex(int idx) { return ((idx % LCD_PRESET_COUNT) + LCD_PRESET_COUNT) % LCD_PRESET_COUNT; }
static inline bool lcdPresetIsRaw(int idx) { return LCD_PRESETS[lcdPresetIndex(idx)].raw; }

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;

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
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl = PIN_LCD_BL;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
    applyPreset(0);
  }

  // Call before init().
  void applyPreset(int idx) {
    const LcdPreset& p = LCD_PRESETS[lcdPresetIndex(idx)];
    auto cfg = _panel.config();
    cfg.pin_cs = PIN_LCD_CS;
    cfg.pin_rst = PIN_LCD_RST;
    cfg.pin_busy = -1;
    cfg.memory_width = 240;
    cfg.memory_height = 320;
    cfg.panel_width = p.raw ? 240 : LCD_WIDTH;
    cfg.panel_height = p.raw ? 320 : LCD_HEIGHT;
    cfg.offset_x = p.raw ? 0 : p.offX;
    cfg.offset_y = p.raw ? 0 : p.offY;
    cfg.offset_rotation = 0;
    cfg.readable = false;
    cfg.invert = p.invert;
    cfg.rgb_order = p.bgr;
    cfg.dlen_16bit = false;
    cfg.bus_shared = false;
    _panel.config(cfg);
  }
};
