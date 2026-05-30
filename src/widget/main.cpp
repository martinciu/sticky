#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"        // WIFI_SSID, WIFI_PASSWORD
#include "config.h"         // location, timezone, refresh intervals
#include "debug_stats.h"    // debugStatsEvery()
#include "weather.h"        // Weather, wmoLabel(), parseWeather()  (lib/weather)

// ===================== Weather =====================
// The Weather model + parsing now live in lib/weather (so they can be
// unit-tested on the host). fetchWeather() below adds the device-only
// HTTPS networking around the pure parseWeather().

Weather fetchWeather() {
  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") +
               String(WIDGET_LAT, 4) + "&longitude=" + String(WIDGET_LON, 4) +
               "&current=temperature_2m,weather_code";
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, url);
  int status = http.GET();
  Weather w;
  if (status == 200)
    w = parseWeather(http.getString().c_str());
  Serial.printf("weather GET -> %d, ok=%d\n", status, w.ok);
  http.end();
  return w;
}

// ===================== State =====================
M5Canvas canvas(&M5.Display); // full-screen off-screen buffer (flicker-free)

Weather weather;
uint32_t lastWeatherOkMs = 0;  // when a fetch last succeeded
uint32_t lastWeatherTryMs = 0; // when we last attempted a fetch
uint32_t lastNtpMs = 0;
uint32_t lastWifiCheckMs = 0;
int lastSec = -1;

const int BRIGHTNESS[] = {32, 96, 192};
int brightnessIdx = 1;

bool wifiUp() { return WiFi.status() == WL_CONNECTED; }

bool weatherStale() {
  return !weather.ok || (millis() - lastWeatherOkMs) > (2UL * WEATHER_REFRESH_MS);
}

void refreshWeather() {
  lastWeatherTryMs = millis();
  Weather w = fetchWeather();
  if (w.ok) { // keep the previous reading on failure (stale, not blank)
    weather = w;
    lastWeatherOkMs = millis();
  }
}

// ===================== Render =====================
void drawCentered(const char *s, int y) {
  canvas.setCursor((canvas.width() - canvas.textWidth(s)) / 2, y);
  canvas.print(s);
}

void render(const struct tm &t) {
  canvas.fillSprite(TFT_BLACK);

  // --- top bar: wifi (left), battery (right) ---
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(wifiUp() ? TFT_GREEN : TFT_RED);
  canvas.setCursor(2, 2);
  canvas.print(wifiUp() ? "wifi" : "no wifi");

  int bat = M5.Power.getBatteryLevel();
  bool chg = M5.Power.isCharging();
  char b[16];
  snprintf(b, sizeof(b), "%d%%%s", bat, chg ? "+" : "");
  canvas.setTextColor(chg ? TFT_GREEN : TFT_WHITE);
  canvas.setCursor(canvas.width() - canvas.textWidth(b) - 2, 2);
  canvas.print(b);

  // --- big time ---
  char hms[16];
  strftime(hms, sizeof(hms), "%H:%M:%S", &t);
  canvas.setFont(&fonts::Font7);
  canvas.setTextColor(TFT_CYAN);
  drawCentered(hms, 24);

  // --- date ---
  char date[32];
  strftime(date, sizeof(date), "%a  %Y-%m-%d", &t);
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(TFT_WHITE);
  drawCentered(date, 80);

  // --- weather (dimmed if stale) ---
  char wx[40];
  if (weather.ok)
    snprintf(wx, sizeof(wx), "%.1fC  %s%s", weather.tempC, wmoLabel(weather.code),
             weatherStale() ? " (old)" : "");
  else
    snprintf(wx, sizeof(wx), "weather --");
  canvas.setFont(&fonts::Font4);
  canvas.setTextColor(weatherStale() ? TFT_DARKGREY : TFT_ORANGE);
  drawCentered(wx, 104);

  canvas.pushSprite(0, 0);
}

// ===================== Lifecycle =====================
void splash(const char *msg) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(10, 50);
  M5.Display.print(msg);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(BRIGHTNESS[brightnessIdx]);
  canvas.createSprite(M5.Display.width(), M5.Display.height()); // ~63 KB

  splash("WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (uint32_t t0 = millis(); !wifiUp() && millis() - t0 < 20000;)
    delay(250);

  splash("Time...");
  configTzTime(WIDGET_TZ, "pool.ntp.org", "time.google.com");
  lastNtpMs = millis();
  struct tm t;
  getLocalTime(&t, 10000); // wait up to 10 s for first sync

  splash("Weather...");
  refreshWeather();
}

void loop() {
  M5.update();

  // --- buttons ---
  if (M5.BtnA.wasPressed())
    refreshWeather(); // force a weather refresh
  if (M5.BtnB.wasPressed()) {
    brightnessIdx = (brightnessIdx + 1) % 3; // cycle screen brightness
    M5.Display.setBrightness(BRIGHTNESS[brightnessIdx]);
  }

  // --- periodic jobs (all non-blocking, millis-scheduled) ---
  if (millis() - lastWeatherTryMs >= WEATHER_REFRESH_MS)
    refreshWeather();
  if (millis() - lastNtpMs >= NTP_RESYNC_MS) {
    configTzTime(WIDGET_TZ, "pool.ntp.org", "time.google.com");
    lastNtpMs = millis();
  }
  if (!wifiUp() && millis() - lastWifiCheckMs >= 5000) {
    lastWifiCheckMs = millis();
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // try to reconnect; clock keeps ticking
  }

  // --- tick: redraw the whole frame when the second changes ---
  struct tm t;
  if (getLocalTime(&t) && t.tm_sec != lastSec) {
    lastSec = t.tm_sec;
    render(t);
  }

  debugStatsEvery(30000); // heap/stack report every 30 s
  delay(20);
}
