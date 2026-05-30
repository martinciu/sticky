#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "debug_stats.h"

// Your location for Open-Meteo (default: Warsaw). Change to your lat/lon.
const double LAT = 52.23;
const double LON = 21.01;

struct Weather {
  bool ok = false;
  float tempC = 0.0f;
  int code = -1; // WMO weather code
};

// WMO weather code -> short label (Open-Meteo reports these standardized codes).
const char *wmoLabel(int code) {
  switch (code) {
  case 0:                 return "Clear";
  case 1: case 2: case 3: return "Cloudy";
  case 45: case 48:       return "Fog";
  case 51: case 53: case 55: return "Drizzle";
  case 61: case 63: case 65: return "Rain";
  case 71: case 73: case 75: return "Snow";
  case 80: case 81: case 82: return "Showers";
  case 95: case 96: case 99: return "Storm";
  default:                return "?";
  }
}

// PURE: turn an Open-Meteo JSON string into a Weather struct. No network or
// hardware here -> this is the piece we can unit-test on the host later.
Weather parseWeather(const String &json) {
  Weather w;
  JsonDocument doc;
  if (deserializeJson(doc, json))
    return w; // malformed JSON -> ok stays false
  JsonObject cur = doc["current"];
  if (cur.isNull())
    return w;
  w.tempC = cur["temperature_2m"] | 0.0f;
  w.code = cur["weather_code"] | -1;
  w.ok = (w.code >= 0);
  return w;
}

// Fetch current weather over HTTPS from Open-Meteo (no API key required).
Weather fetchWeather() {
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" +
               String(LAT, 4) + "&longitude=" + String(LON, 4) +
               "&current=temperature_2m,weather_code";
  WiFiClientSecure client;
  client.setInsecure(); // skip cert validation (fine for a read-only request)
  HTTPClient http;
  http.begin(client, url);
  int status = http.GET();
  Serial.printf("GET -> %d\n", status);
  Weather w;
  if (status == 200)
    w = parseWeather(http.getString());
  http.end();
  return w;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
    delay(250);
}

void status(const char *msg) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(10, 50);
  M5.Display.print(msg);
}

void drawWeather(const Weather &w) {
  M5.Display.fillScreen(TFT_BLACK);
  if (!w.ok) {
    M5.Display.setFont(&fonts::Font4);
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.setCursor(10, 50);
    M5.Display.print("weather fail");
    return;
  }
  // Big temperature (Font7 has digits + '.'; no degree glyph, so unit is implied).
  char temp[16];
  snprintf(temp, sizeof(temp), "%.1f", w.tempC);
  M5.Display.setFont(&fonts::Font7);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setCursor((M5.Display.width() - M5.Display.textWidth(temp)) / 2, 18);
  M5.Display.print(temp);

  // Condition label below.
  const char *label = wmoLabel(w.code);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor((M5.Display.width() - M5.Display.textWidth(label)) / 2, 95);
  M5.Display.print(label);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);

  status("WiFi...");
  connectWiFi();
  status("Fetching...");
  drawWeather(fetchWeather());
  printStats(); // baseline at boot; getMinFreeHeap captures the fetch's dip
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) { // press BtnA to re-fetch
    status("Fetching...");
    drawWeather(fetchWeather());
    printStats(); // watch the heap dip caused by each fetch
  }
  debugStatsEvery(); // periodic heap/stack report (every 5 s)
  delay(20);
}
