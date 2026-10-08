#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "config.h"

// Display presets. The AIPI Lite's screen controller is reported differently by different
// community projects (ST7789 vs ST7735), so Nova can cycle through likely settings at boot.
struct LcdPreset {
  bool st7789;  // true = ST7789 driver, false = ST7735S driver
  int offX, offY;
  bool invert;
  bool bgr;
};

static const LcdPreset LCD_PRESETS[] = {
    {true, 0, 0, true, false},   // 0
    {true, 0, 0, true, true},    // 1
    {true, 0, 0, false, false},  // 2
    {true, 0, 0, false, true},   // 3
    {false, 2, 1, false, false}, // 4
    {false, 2, 1, false, true},  // 5
    {false, 2, 1, true, false},  // 6
    {false, 2, 3, false, true},  // 7
};
static const int LCD_PRESET_COUNT = sizeof(LCD_PRESETS) / sizeof(LCD_PRESETS[0]);

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _p7789;
  lgfx::Panel_ST7735S _p7735;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;

  template <class PANEL>
  void setupPanel(PANEL& pn, const LcdPreset& p) {
    auto cfg = pn.config();
    cfg.pin_cs = PIN_LCD_CS;
    cfg.pin_rst = PIN_LCD_RST;
    cfg.pin_busy = -1;
    cfg.panel_width = LCD_WIDTH;
    cfg.panel_height = LCD_HEIGHT;
    cfg.offset_x = p.offX;
    cfg.offset_y = p.offY;
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
    applyPreset(0);
  }

  // Call before init().
  void applyPreset(int idx) {
    const LcdPreset& p = LCD_PRESETS[((idx % LCD_PRESET_COUNT) + LCD_PRESET_COUNT) % LCD_PRESET_COUNT];
    if (p.st7789) {
      setupPanel(_p7789, p);
      setPanel(&_p7789);
    } else {
      setupPanel(_p7735, p);
      setPanel(&_p7735);
    }
  }
};
