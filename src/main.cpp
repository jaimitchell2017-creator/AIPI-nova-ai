// Nova for XORIGIN AIPI Lite - Phase 1: clock + weather desk display
// Button (GPIO42): short press = next screen, hold 4 s = reset Wi-Fi/city setup
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include <LittleFS.h>
#include "config.h"
#include "display.h"

static LGFX lcd;
static LGFX_Sprite canvas(&lcd);
static Preferences prefs;

// ---------- colours ----------
static uint16_t COL_BG, COL_PANEL, COL_TEXT, COL_DIM, COL_ACCENT, COL_SUN, COL_CLOUD, COL_RAIN, COL_BOLT;
static void initColors() {
  COL_BG = lcd.color565(8, 12, 28);
  COL_PANEL = lcd.color565(24, 32, 64);
  COL_TEXT = lcd.color565(235, 240, 255);
  COL_DIM = lcd.color565(130, 140, 170);
  COL_ACCENT = lcd.color565(90, 200, 255);
  COL_SUN = lcd.color565(255, 200, 40);
  COL_CLOUD = lcd.color565(190, 200, 215);
  COL_RAIN = lcd.color565(80, 160, 255);
  COL_BOLT = lcd.color565(255, 230, 60);
}

// ---------- settings & state ----------
struct Settings {
  String city = "Sydney";
  bool use24h = false;
  bool fahrenheit = false;
} cfg;

struct Weather {
  bool valid = false;
  float temp = 0;
  int code = 0;
  bool isDay = true;
  int days = 0;
  float hi[7], lo[7];
  int dcode[7];
  char date[7][11];
} wx;

static double gLat = 0, gLon = 0;
static bool haveLoc = false;
static long gOffset = 0;
static String gGeoCity = "";
static String gPlace = "";

static int screenIdx = 0;  // 0 clock, 1 weather, 2 week
static bool dirty = true;
static unsigned long lastInteraction = 0, lastDraw = 0, nextWeather = 0;
static bool shouldSave = false;
static bool fsOk = false;
static int g_preset = 0, g_rot = 0, g_bright = 200;
static void resetWifi();

static void loadSettings() {
  prefs.begin("nova", true);
  cfg.city = prefs.getString("city", "Sydney");
  cfg.use24h = prefs.getBool("h24", false);
  cfg.fahrenheit = prefs.getBool("f", false);
  gLat = prefs.getDouble("lat", 0);
  gLon = prefs.getDouble("lon", 0);
  haveLoc = prefs.getBool("hasloc", false);
  gOffset = prefs.getLong("off", 0);
  gGeoCity = prefs.getString("geocity", "");
  gPlace = prefs.getString("place", "");
  prefs.end();
}

static void saveSettings() {
  prefs.begin("nova", false);
  prefs.putString("city", cfg.city);
  prefs.putBool("h24", cfg.use24h);
  prefs.putBool("f", cfg.fahrenheit);
  prefs.end();
}

static void saveLocation() {
  prefs.begin("nova", false);
  prefs.putDouble("lat", gLat);
  prefs.putDouble("lon", gLon);
  prefs.putBool("hasloc", haveLoc);
  prefs.putLong("off", gOffset);
  prefs.putString("geocity", gGeoCity);
  prefs.putString("place", gPlace);
  prefs.end();
}

// ---------- weather helpers ----------
enum Kind { K_CLEAR, K_CLOUD, K_RAIN, K_SNOW, K_STORM, K_FOG };

static Kind kindOf(int c) {
  if (c == 0 || c == 1) return K_CLEAR;
  if (c == 2 || c == 3) return K_CLOUD;
  if (c == 45 || c == 48) return K_FOG;
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return K_RAIN;
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return K_SNOW;
  if (c >= 95) return K_STORM;
  return K_CLOUD;
}

static const char* textOf(int c) {
  switch (c) {
    case 0: return "Clear";
    case 1: return "Mostly clear";
    case 2: return "Partly cloudy";
    case 3: return "Overcast";
    case 45: case 48: return "Fog";
    case 51: case 53: case 55: return "Drizzle";
    case 56: case 57: return "Freezing drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66: case 67: return "Freezing rain";
    case 71: case 73: case 75: case 77: return "Snow";
    case 80: case 81: case 82: return "Showers";
    case 85: case 86: return "Snow showers";
    case 95: case 96: case 99: return "Thunderstorm";
    default: return "Unknown";
  }
}

static String urlEncode(const String& s) {
  String o;
  char buf[4];
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
      o += c;
    } else if (c == ' ') {
      o += "%20";
    } else {
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      o += buf;
    }
  }
  return o;
}

static bool httpGetJson(const String& url, JsonDocument& doc) {
  WiFiClientSecure client;
  client.setInsecure();  // public weather data only; no secrets are sent
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != 200) {
    Serial.printf("HTTP %d for %s\n", code, url.c_str());
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("JSON error: %s\n", err.c_str());
    return false;
  }
  return true;
}

static bool geocode() {
  String url = "https://geocoding-api.open-meteo.com/v1/search?count=1&language=en&format=json&name=" + urlEncode(cfg.city);
  JsonDocument doc;
  if (!httpGetJson(url, doc)) return false;
  if (doc["results"].isNull() || doc["results"].size() == 0) return false;
  gLat = doc["results"][0]["latitude"].as<double>();
  gLon = doc["results"][0]["longitude"].as<double>();
  gPlace = doc["results"][0]["name"].as<String>();
  gGeoCity = cfg.city;
  haveLoc = true;
  saveLocation();
  return true;
}

static bool fetchWeather() {
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(gLat, 4) + "&longitude=" + String(gLon, 4) +
               "&current=temperature_2m,weather_code,is_day" +
               "&daily=weather_code,temperature_2m_max,temperature_2m_min" +
               "&timezone=auto&forecast_days=7" + (cfg.fahrenheit ? "&temperature_unit=fahrenheit" : "");
  JsonDocument doc;
  if (!httpGetJson(url, doc)) return false;
  if (doc["current"].isNull() || doc["daily"].isNull()) return false;

  wx.temp = doc["current"]["temperature_2m"].as<float>();
  wx.code = doc["current"]["weather_code"].as<int>();
  wx.isDay = doc["current"]["is_day"].as<int>() == 1;
  JsonArray t = doc["daily"]["time"].as<JsonArray>();
  wx.days = min((int)t.size(), 7);
  for (int i = 0; i < wx.days; i++) {
    wx.hi[i] = doc["daily"]["temperature_2m_max"][i].as<float>();
    wx.lo[i] = doc["daily"]["temperature_2m_min"][i].as<float>();
    wx.dcode[i] = doc["daily"]["weather_code"][i].as<int>();
    strlcpy(wx.date[i], t[i].as<const char*>(), sizeof(wx.date[i]));
  }
  wx.valid = true;

  long off = doc["utc_offset_seconds"].as<long>();
  if (off != gOffset) {
    gOffset = off;
    saveLocation();
  }
  configTime(gOffset, 0, "pool.ntp.org", "time.google.com");
  return true;
}

static bool refreshWeather() {
  if (!haveLoc || gGeoCity != cfg.city) {
    if (!geocode()) return false;
  }
  return fetchWeather();
}

// ---------- drawing ----------
static void drawCloud(int cx, int cy, int s, uint16_t col) {
  canvas.fillCircle(cx - s / 4, cy + s / 12, s / 4, col);
  canvas.fillCircle(cx + s / 10, cy - s / 8, s / 3, col);
  canvas.fillCircle(cx + s / 3, cy + s / 12, s / 5, col);
  canvas.fillRect(cx - s / 4, cy + s / 12, s * 0.58f, s / 5, col);
}

static void drawIcon(int cx, int cy, int s, Kind k, bool day) {
  switch (k) {
    case K_CLEAR:
      if (day) {
        canvas.fillCircle(cx, cy, s / 4, COL_SUN);
        for (int i = 0; i < 8; i++) {
          float a = i * PI / 4;
          canvas.drawLine(cx + cosf(a) * s * 0.36f, cy + sinf(a) * s * 0.36f, cx + cosf(a) * s * 0.5f, cy + sinf(a) * s * 0.5f, COL_SUN);
        }
      } else {
        canvas.fillCircle(cx, cy, s / 3, COL_TEXT);
        canvas.fillCircle(cx + s / 7, cy - s / 9, s / 3, COL_BG);
      }
      break;
    case K_CLOUD:
      drawCloud(cx, cy, s, COL_CLOUD);
      break;
    case K_RAIN:
      drawCloud(cx, cy - s / 8, s, COL_CLOUD);
      for (int i = -1; i <= 1; i++) canvas.drawLine(cx + i * s / 4, cy + s / 4, cx + i * s / 4 - s / 12, cy + s / 2, COL_RAIN);
      break;
    case K_SNOW:
      drawCloud(cx, cy - s / 8, s, COL_CLOUD);
      for (int i = -1; i <= 1; i++) canvas.fillCircle(cx + i * s / 4, cy + s / 3, max(1, s / 16), COL_TEXT);
      break;
    case K_STORM:
      drawCloud(cx, cy - s / 8, s, COL_DIM);
      canvas.fillTriangle(cx + s / 12, cy + s / 10, cx - s / 8, cy + s / 3, cx + s / 16, cy + s / 3, COL_BOLT);
      canvas.fillTriangle(cx + s / 16, cy + s / 3, cx + s / 6, cy + s / 10, cx - s / 12, cy + s / 2, COL_BOLT);
      break;
    case K_FOG:
      for (int i = -1; i <= 1; i++) canvas.drawFastHLine(cx - s / 3, cy + i * s / 6, s * 2 / 3, COL_CLOUD);
      break;
  }
}

static void weekdayShort(const char* iso, char* out) {  // "2026-10-08" -> "Thu"
  int y = 0, m = 0, d = 0;
  sscanf(iso, "%d-%d-%d", &y, &m, &d);
  struct tm t = {};
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  t.tm_hour = 12;
  mktime(&t);
  strftime(out, 4, "%a", &t);
}

static void showLines(const char* l1, const char* l2 = nullptr, const char* l3 = nullptr, const char* l4 = nullptr, const char* l5 = nullptr) {
  canvas.fillScreen(COL_BG);
  canvas.setFont(&lgfx::fonts::Font2);
  canvas.setTextSize(1);
  canvas.setTextDatum(lgfx::textdatum::middle_center);
  const char* ls[] = {l1, l2, l3, l4, l5};
  int n = 0;
  for (auto l : ls) if (l) n++;
  int y = 64 - (n * 18) / 2 + 9;
  bool first = true;
  for (auto l : ls) {
    if (!l) continue;
    canvas.setTextColor(first ? COL_ACCENT : COL_TEXT);
    canvas.drawString(l, 64, y);
    y += 18;
    first = false;
  }
  canvas.pushSprite(0, 0);
}

static void testPattern(int preset) {
  canvas.fillScreen(TFT_BLACK);
  canvas.drawRect(0, 0, 128, 128, TFT_WHITE);
  canvas.fillRect(1, 1, 14, 14, TFT_RED);
  canvas.fillRect(113, 1, 14, 14, TFT_GREEN);
  canvas.fillRect(1, 113, 14, 14, TFT_BLUE);
  canvas.fillRect(113, 113, 14, 14, TFT_YELLOW);
  canvas.setFont(&lgfx::fonts::Font4);
  canvas.setTextDatum(lgfx::textdatum::middle_center);
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("NOVA", 64, 40);
  canvas.setFont(&lgfx::fonts::Font2);
  char pb[24];
  snprintf(pb, sizeof(pb), "Preset %d (0-%d)", preset, LCD_PRESET_COUNT - 1);
  canvas.drawString(pb, 64, 66);
  canvas.drawString("Press button", 64, 86);
  canvas.drawString("if picture is bad", 64, 102);
  canvas.pushSprite(0, 0);
}

static void gridPattern() {
  // Drawn straight onto the screen (no big sprite), so it cannot run out of memory.
  uint16_t dim = lcd.color565(40, 40, 40), mid = lcd.color565(120, 120, 120);
  lcd.fillScreen(TFT_BLACK);
  for (int x = 0; x < 240; x += 20) lcd.drawFastVLine(x, 0, 320, (x % 40 == 0) ? mid : dim);
  for (int y = 0; y < 320; y += 20) lcd.drawFastHLine(0, y, 240, (y % 40 == 0) ? mid : dim);
  lcd.setFont(&lgfx::fonts::Font0);
  lcd.setTextSize(1);
  lcd.setTextDatum(lgfx::textdatum::top_left);
  lcd.setTextColor(TFT_WHITE);
  char b[12];
  for (int y = 0; y < 320; y += 40) {
    for (int x = 0; x < 240; x += 40) {
      snprintf(b, sizeof(b), "%d,%d", x, y);
      lcd.drawString(b, x + 2, y + 2);
    }
  }
  lcd.fillRect(0, 0, 8, 8, TFT_RED);
  lcd.fillRect(232, 0, 8, 8, TFT_GREEN);
  lcd.fillRect(0, 312, 8, 8, TFT_BLUE);
  lcd.fillRect(232, 312, 8, 8, TFT_YELLOW);
}

// ---------- serial commands (type them in the Logs & Console box, then press Enter) ----------
//   preset N   -> use display preset N (the number the log prints, 0-7) and restart
//   rot N      -> screen rotation 0-3 and restart
//   reset      -> forget display settings and restart
static String serialBuf;

static void notesAdd(const String& text) {
  if (!fsOk) {
    Serial.println("Storage is not ready.");
    return;
  }
  File f = LittleFS.open("/notes.txt", FILE_APPEND);
  if (!f) {
    Serial.println("Could not open the notes file.");
    return;
  }
  struct tm t;
  char ts[24] = "no-time";
  if (getLocalTime(&t, 0)) strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M", &t);
  f.printf("%s | %s\n", ts, text.c_str());
  f.close();
  Serial.println("Note saved on the device.");
}

static void notesList() {
  if (!fsOk) {
    Serial.println("Storage is not ready.");
    return;
  }
  File f = LittleFS.open("/notes.txt", FILE_READ);
  if (!f) {
    Serial.println("No notes yet.");
    return;
  }
  int n = 0;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length()) Serial.printf("%d: %s\n", ++n, line.c_str());
  }
  f.close();
  if (!n) Serial.println("No notes yet.");
}

static void printStatus() {
  struct tm t;
  char ts[32] = "not synced yet";
  if (getLocalTime(&t, 0)) strftime(ts, sizeof(ts), "%a %d %b %Y %H:%M:%S", &t);
  Serial.printf("Display preset %d, rotation %d, brightness %d\n", g_preset, g_rot, g_bright);
  if (WiFi.status() == WL_CONNECTED) Serial.printf("Wi-Fi: %s, IP %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  else Serial.println("Wi-Fi: not connected");
  Serial.printf("City: %s, %s clock, %s\n", cfg.city.c_str(), cfg.use24h ? "24-hour" : "12-hour", cfg.fahrenheit ? "Fahrenheit" : "Celsius");
  Serial.printf("Time: %s\n", ts);
  Serial.printf("Weather loaded: %s\n", wx.valid ? "yes" : "no");
  if (fsOk) Serial.printf("Storage: %u of %u bytes used\n", (unsigned)LittleFS.usedBytes(), (unsigned)LittleFS.totalBytes());
  Serial.printf("Free memory: %u bytes, uptime %lu s\n", (unsigned)ESP.getFreeHeap(), millis() / 1000);
}

static void printHelp() {
  Serial.println("Commands:");
  Serial.println("  help                 show this list");
  Serial.println("  status               settings, Wi-Fi, time, memory, storage");
  Serial.println("  time                 show the current time");
  Serial.println("  city NAME            set the weather city (e.g. city Sydney)");
  Serial.println("  units c | f          Celsius or Fahrenheit");
  Serial.println("  clock 12 | 24        12-hour or 24-hour clock");
  Serial.println("  weather              refresh the weather now");
  Serial.println("  screen 0-2           show clock, weather or 7-day screen");
  Serial.println("  brightness 0-255     screen brightness (saved)");
  Serial.println("  note TEXT            save a note on the device (works offline)");
  Serial.println("  notes                list saved notes");
  Serial.println("  clearnotes           delete all notes");
  Serial.println("  preset 0-7           display preset (saved, restarts)");
  Serial.println("  rot 0-3              screen rotation (saved, restarts)");
  Serial.println("  wifi reset           forget Wi-Fi and city setup (restarts)");
  Serial.println("  reset                clear display settings (restarts)");
  Serial.println("  reboot               restart the device");
}

static void runCommand(String orig) {
  orig.trim();
  String l = orig;
  l.toLowerCase();
  int sp = l.indexOf(' ');
  String cmd = sp < 0 ? l : l.substring(0, sp);
  String arg = sp < 0 ? "" : l.substring(sp + 1);
  String argOrig = sp < 0 ? "" : orig.substring(sp + 1);
  arg.trim();
  argOrig.trim();

  if (cmd == "help" || cmd == "?") {
    printHelp();
  } else if (cmd == "status") {
    printStatus();
  } else if (cmd == "time") {
    struct tm t;
    if (getLocalTime(&t, 0)) {
      char ts[40];
      strftime(ts, sizeof(ts), "%A %d %B %Y, %H:%M:%S", &t);
      Serial.println(ts);
    } else {
      Serial.println("Time not synced yet (needs Wi-Fi).");
    }
  } else if (cmd == "city") {
    if (!argOrig.length()) {
      Serial.printf("City is %s. Use: city Sydney\n", cfg.city.c_str());
    } else {
      cfg.city = argOrig;
      saveSettings();
      haveLoc = false;
      nextWeather = 0;
      Serial.printf("City set to %s. Refreshing weather...\n", cfg.city.c_str());
    }
  } else if (cmd == "units") {
    if (arg == "c" || arg == "f") {
      cfg.fahrenheit = (arg == "f");
      saveSettings();
      nextWeather = 0;
      Serial.printf("Units: %s\n", cfg.fahrenheit ? "Fahrenheit" : "Celsius");
    } else {
      Serial.println("Use: units c   or   units f");
    }
  } else if (cmd == "clock") {
    if (arg == "12" || arg == "24") {
      cfg.use24h = (arg == "24");
      saveSettings();
      dirty = true;
      Serial.printf("Clock: %s-hour\n", arg.c_str());
    } else {
      Serial.println("Use: clock 12   or   clock 24");
    }
  } else if (cmd == "weather") {
    nextWeather = 0;
    Serial.println("Refreshing weather...");
  } else if (cmd == "screen") {
    int n = arg.toInt();
    if (n < 0 || n > 2) {
      Serial.println("Use: screen 0, 1 or 2");
    } else {
      screenIdx = n;
      lastInteraction = millis();
      dirty = true;
      Serial.printf("Showing screen %d\n", n);
    }
  } else if (cmd == "brightness") {
    int n = arg.toInt();
    if (!arg.length() || n < 0 || n > 255) {
      Serial.printf("Brightness is %d. Use: brightness 0-255\n", g_bright);
    } else {
      g_bright = n;
      lcd.setBrightness(n);
      prefs.begin("novacal", false);
      prefs.putInt("bright", n);
      prefs.end();
      Serial.printf("Brightness %d saved.\n", n);
    }
  } else if (cmd == "note") {
    if (!argOrig.length()) Serial.println("Use: note buy milk");
    else notesAdd(argOrig);
  } else if (cmd == "notes") {
    notesList();
  } else if (cmd == "clearnotes") {
    if (fsOk) LittleFS.remove("/notes.txt");
    Serial.println("Notes deleted.");
  } else if (cmd == "preset") {
    int n = arg.toInt();
    if (!arg.length() || n < 0 || n >= LCD_PRESET_COUNT) {
      Serial.printf("Use: preset 0 to preset %d\n", LCD_PRESET_COUNT - 1);
      return;
    }
    prefs.begin("novacal", false);
    prefs.putInt("preset2", n);
    prefs.end();
    Serial.printf("Saved display preset %d. Restarting...\n", n);
    delay(300);
    ESP.restart();
  } else if (cmd == "rot") {
    int n = arg.toInt();
    if (!arg.length() || n < 0 || n > 3) {
      Serial.println("Use: rot 0 to rot 3");
      return;
    }
    prefs.begin("novacal", false);
    prefs.putInt("rot2", n);
    prefs.end();
    Serial.printf("Saved rotation %d. Restarting...\n", n);
    delay(300);
    ESP.restart();
  } else if (cmd == "wifi" && arg == "reset") {
    Serial.println("Forgetting Wi-Fi settings...");
    resetWifi();
  } else if (cmd == "reset") {
    prefs.begin("novacal", false);
    prefs.clear();
    prefs.end();
    Serial.println("Display settings cleared. Restarting...");
    delay(300);
    ESP.restart();
  } else if (cmd == "reboot" || cmd == "restart") {
    Serial.println("Restarting...");
    delay(300);
    ESP.restart();
  } else {
    Serial.printf("Unknown command: %s   (type help)\n", cmd.c_str());
  }
}

static void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      String l = serialBuf;
      serialBuf = "";
      l.trim();
      if (l.length()) runCommand(l);
    } else if (serialBuf.length() < 64) {
      serialBuf += c;
    }
  }
}

// Waits for a press of the main button and, if one comes, moves to the next display preset and restarts.
// timeoutMs = 0 means wait forever. Typed serial commands are also handled here.
static void presetWindow(int preset, unsigned long timeoutMs) {
  unsigned long w = millis();
  while (digitalRead(PIN_BUTTON) == LOW && millis() - w < 3000) delay(10);
  unsigned long t0 = millis();
  while (timeoutMs == 0 || millis() - t0 < timeoutMs) {
    handleSerial();
    if (digitalRead(PIN_BUTTON) == LOW) {
      delay(40);
      if (digitalRead(PIN_BUTTON) == LOW) {
        int next = (preset + 1) % LCD_PRESET_COUNT;
        prefs.begin("novacal", false);
        prefs.putInt("preset2", next);
        prefs.end();
        Serial.printf("Button pressed: trying display preset %d\n", next);
        delay(300);
        ESP.restart();
      }
    }
    delay(10);
  }
}

static int shownTemp(float t) { return (int)lroundf(t); }

static void drawClock() {
  canvas.fillScreen(COL_BG);
  struct tm t;
  bool ok = getLocalTime(&t, 0);
  canvas.setTextDatum(lgfx::textdatum::middle_center);

  // date
  canvas.setFont(&lgfx::fonts::Font2);
  canvas.setTextSize(1);
  canvas.setTextColor(COL_DIM);
  if (ok) {
    char d[24];
    strftime(d, sizeof(d), "%a %e %b", &t);
    canvas.drawString(d, 64, 12);
  } else {
    canvas.drawString("Syncing time...", 64, 12);
  }

  // time
  char tbuf[8];
  if (ok) {
    if (cfg.use24h) snprintf(tbuf, sizeof(tbuf), "%02d:%02d", t.tm_hour, t.tm_min);
    else snprintf(tbuf, sizeof(tbuf), "%d:%02d", t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12, t.tm_min);
  } else {
    snprintf(tbuf, sizeof(tbuf), "--:--");
  }
  canvas.setFont(&lgfx::fonts::Font7);
  canvas.setTextSize(0.7f);
  canvas.setTextColor(COL_TEXT);
  canvas.drawString(tbuf, 64, 50);

  if (ok && !cfg.use24h) {
    canvas.setFont(&lgfx::fonts::Font2);
    canvas.setTextSize(1);
    canvas.setTextColor(COL_ACCENT);
    canvas.drawString(t.tm_hour >= 12 ? "PM" : "AM", 64, 78);
  }

  // weather pill
  canvas.fillRoundRect(10, 90, 108, 30, 12, COL_PANEL);
  if (wx.valid) {
    drawIcon(32, 105, 26, kindOf(wx.code), wx.isDay);
    char tb[12];
    snprintf(tb, sizeof(tb), "%d%s", shownTemp(wx.temp), cfg.fahrenheit ? "F" : "C");
    canvas.setFont(&lgfx::fonts::Font4);
    canvas.setTextDatum(lgfx::textdatum::middle_left);
    canvas.setTextColor(COL_TEXT);
    canvas.drawString(tb, 54, 105);
  } else {
    canvas.setFont(&lgfx::fonts::Font2);
    canvas.setTextColor(COL_DIM);
    canvas.drawString(WiFi.status() == WL_CONNECTED ? "Loading weather" : "No Wi-Fi", 64, 105);
  }

  // seconds bar
  if (ok) {
    canvas.fillRect(4, 124, 120, 2, COL_PANEL);
    canvas.fillRect(4, 124, (120 * t.tm_sec) / 59, 2, COL_ACCENT);
  }
  canvas.pushSprite(0, 0);
}

static void drawWeather() {
  canvas.fillScreen(COL_BG);
  canvas.setTextDatum(lgfx::textdatum::middle_center);
  canvas.setFont(&lgfx::fonts::Font2);
  canvas.setTextSize(1);
  if (!wx.valid) {
    canvas.setTextColor(COL_DIM);
    canvas.drawString("No weather yet", 64, 64);
    canvas.pushSprite(0, 0);
    return;
  }
  canvas.setTextColor(COL_ACCENT);
  canvas.drawString(gPlace.c_str(), 64, 9);
  drawIcon(64, 40, 46, kindOf(wx.code), wx.isDay);

  char tb[12];
  snprintf(tb, sizeof(tb), "%d%s", shownTemp(wx.temp), cfg.fahrenheit ? "F" : "C");
  canvas.setFont(&lgfx::fonts::Font4);
  canvas.setTextSize(1.6f);
  canvas.setTextColor(COL_TEXT);
  canvas.drawString(tb, 64, 82);

  canvas.setFont(&lgfx::fonts::Font2);
  canvas.setTextSize(1);
  canvas.setTextColor(COL_DIM);
  canvas.drawString(textOf(wx.code), 64, 107);
  char hl[24];
  snprintf(hl, sizeof(hl), "H %d   L %d", shownTemp(wx.hi[0]), shownTemp(wx.lo[0]));
  canvas.drawString(hl, 64, 120);
  canvas.pushSprite(0, 0);
}

static void drawWeek() {
  canvas.fillScreen(COL_BG);
  canvas.setFont(&lgfx::fonts::Font2);
  canvas.setTextSize(1);
  canvas.setTextDatum(lgfx::textdatum::middle_center);
  canvas.setTextColor(COL_ACCENT);
  canvas.drawString("7-day forecast", 64, 8);
  if (!wx.valid) {
    canvas.setTextColor(COL_DIM);
    canvas.drawString("No weather yet", 64, 64);
    canvas.pushSprite(0, 0);
    return;
  }
  canvas.setTextDatum(lgfx::textdatum::middle_left);
  for (int i = 0; i < wx.days; i++) {
    int y = 26 + i * 15;
    char wd[4], hl[16];
    weekdayShort(wx.date[i], wd);
    snprintf(hl, sizeof(hl), "%d/%d", shownTemp(wx.hi[i]), shownTemp(wx.lo[i]));
    canvas.setTextColor(i == 0 ? COL_ACCENT : COL_TEXT);
    canvas.drawString(wd, 8, y);
    canvas.setTextColor(COL_TEXT);
    canvas.drawString(hl, 52, y);
    drawIcon(112, y, 14, kindOf(wx.dcode[i]), true);
  }
  canvas.pushSprite(0, 0);
}

static void draw() {
  if (screenIdx == 0) drawClock();
  else if (screenIdx == 1) drawWeather();
  else drawWeek();
}

// ---------- Wi-Fi setup ----------
static void apCallback(WiFiManager*) {
  showLines("Nova setup", "Join Wi-Fi:", "Nova-Setup", "then open", "192.168.4.1");
}

static void saveCallback() { shouldSave = true; }

static void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  char h24[2] = {cfg.use24h ? '1' : '0', 0};
  char fah[2] = {cfg.fahrenheit ? '1' : '0', 0};
  WiFiManagerParameter pCity("city", "City (e.g. Sydney)", cfg.city.c_str(), 40);
  WiFiManagerParameter pH24("h24", "24-hour clock? (1 = yes, 0 = 12-hour)", h24, 2);
  WiFiManagerParameter pFah("fahr", "Fahrenheit? (1 = yes, 0 = Celsius)", fah, 2);
  wm.addParameter(&pCity);
  wm.addParameter(&pH24);
  wm.addParameter(&pFah);
  wm.setAPCallback(apCallback);
  wm.setSaveParamsCallback(saveCallback);
  wm.setConfigPortalTimeout(600);
  wm.setConnectTimeout(20);
  showLines("Nova", "Connecting", "to Wi-Fi...");
  if (!wm.autoConnect("Nova-Setup")) {
    showLines("Wi-Fi failed", "Restarting...");
    delay(2000);
    ESP.restart();
  }
  if (shouldSave) {
    String c = String(pCity.getValue());
    c.trim();
    if (c.length() > 0) cfg.city = c;
    cfg.use24h = pH24.getValue()[0] == '1';
    cfg.fahrenheit = pFah.getValue()[0] == '1';
    saveSettings();
    haveLoc = false;  // force a fresh lookup for the new city
  }
}

static void resetWifi() {
  showLines("Resetting", "Wi-Fi + city", "settings...");
  WiFiManager wm;
  wm.resetSettings();
  prefs.begin("nova", false);
  prefs.clear();
  prefs.end();
  delay(1500);
  ESP.restart();
}

// ---------- button ----------
static void handleButton() {
  static bool wasDown = false, longDone = false;
  static unsigned long downAt = 0;
  bool down = digitalRead(PIN_BUTTON) == LOW;
  unsigned long now = millis();
  if (down && !wasDown) {
    downAt = now;
    longDone = false;
  }
  if (down && !longDone && now - downAt >= 4000) {
    longDone = true;
    resetWifi();
  }
  if (!down && wasDown && !longDone && now - downAt >= 40) {
    screenIdx = (screenIdx + 1) % 3;
    lastInteraction = now;
    dirty = true;
  }
  wasDown = down;
}

// ---------- main ----------
void setup() {
  Serial.begin(115200);
  if (POWER_LATCH_PIN >= 0) {
    pinMode(POWER_LATCH_PIN, OUTPUT);
    digitalWrite(POWER_LATCH_PIN, HIGH);
  }
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  prefs.begin("novacal", true);
  bool calibrated = true;  // a default preset (LCD_DEFAULT_PRESET) is used until you change it
  int preset = prefs.getInt("preset2", LCD_DEFAULT_PRESET);
  int rot = prefs.getInt("rot2", LCD_ROTATION);
  int bright = prefs.getInt("bright", LCD_BRIGHTNESS);
  prefs.end();
  g_preset = preset;
  g_rot = rot;
  g_bright = bright;
  fsOk = LittleFS.begin(true);
  if (!fsOk) Serial.println("WARNING: storage could not start");
  bool raw = lcdPresetIsRaw(preset);
  Serial.printf("Nova boot, display preset %d%s%s, rotation %d\n", preset, raw ? " (grid view)" : "", calibrated ? " (saved)" : "", rot);
  Serial.println("Type help in the console to see all commands.");

  lcd.applyPreset(preset);
  lcd.init();
  // During calibration always use rotation 0 so offsets are easy to judge.
  lcd.setRotation((calibrated && !raw) ? rot : 0);
  lcd.setBrightness(bright);
  canvas.setColorDepth(16);
  if (!canvas.createSprite(128, 128)) Serial.println("WARNING: could not allocate the 128x128 sprite");
  initColors();

  if (raw) {
    gridPattern();
    presetWindow(preset, 0);  // stays on the grid until the button is pressed
  } else if (!calibrated) {
    testPattern(preset);
    presetWindow(preset, 7000);  // press the button within 7 s to try the next preset
  } else {
    // Saved setting: a 3 s window at power-up. Press either button to try the next preset.
    presetWindow(preset, 3000);
  }

  loadSettings();
  connectWifi();
  Serial.printf("Wi-Fi connected, IP %s\n", WiFi.localIP().toString().c_str());
  configTime(gOffset, 0, "pool.ntp.org", "time.google.com");
  showLines("Nova", "Getting weather...");
  if (!refreshWeather()) nextWeather = millis() + 30000;
  else nextWeather = millis() + 15UL * 60UL * 1000UL;
  lastInteraction = millis();
}

void loop() {
  handleSerial();
  handleButton();
  unsigned long now = millis();

  if (WiFi.status() == WL_CONNECTED && (long)(now - nextWeather) >= 0) {
    bool ok = refreshWeather();
    nextWeather = now + (ok ? 15UL * 60UL * 1000UL : 2UL * 60UL * 1000UL);
    dirty = true;
  }
  if (screenIdx != 0 && now - lastInteraction > 30000) {
    screenIdx = 0;
    dirty = true;
  }
  if (dirty || now - lastDraw >= 1000) {
    draw();
    lastDraw = now;
    dirty = false;
  }
  delay(10);
}
