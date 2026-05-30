#include "secrets.h"
#include <M5Unified.h>
#include <WiFi.h>
#include <time.h>

// Poland timezone as a POSIX TZ string: CET (UTC+1) standard, switching to
// CEST (UTC+2) from the last Sunday of March to the last Sunday of October.
// The C runtime applies DST automatically from this rule.
const char *TZ_POLAND = "CET-1CEST,M3.5.0,M10.5.0/3";

int lastSec = -1; // redraw only when the displayed second actually changes

void connectWiFi() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.print("WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    M5.Display.print(".");
  }
}

void drawClock(const struct tm &t) {
  char hms[16], date[32];
  strftime(hms, sizeof(hms), "%H:%M:%S", &t);
  strftime(date, sizeof(date), "%a  %Y-%m-%d", &t);

  // Big 7-segment-style time, centered. Opaque text (fg,bg) overwrites the
  // previous value in place; fixed-width digits keep the x constant -> no
  // flicker.
  M5.Display.setFont(&fonts::Font7);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.setCursor((M5.Display.width() - M5.Display.textWidth(hms)) / 2,
                       30);
  M5.Display.print(hms);

  // Date underneath, smaller font.
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor((M5.Display.width() - M5.Display.textWidth(date)) / 2,
                       104);
  M5.Display.print(date);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);

  connectWiFi();

  // Start NTP: pull UTC from time servers, then apply our timezone string.
  configTzTime(TZ_POLAND, "pool.ntp.org", "time.google.com");

  // getLocalTime() returns false until NTP has actually set the clock.
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.print("Syncing time");
  struct tm t;
  while (!getLocalTime(&t, 1000)) {
    Serial.println("waiting for NTP...");
    M5.Display.print(".");
  }
  Serial.println("Time synced.");
  M5.Display.fillScreen(
      TFT_BLACK); // clear once; loop redraws opaquely from here
}

void loop() {
  M5.update();

  struct tm t;
  if (getLocalTime(&t) &&
      t.tm_sec != lastSec) { // tick: redraw on second change
    lastSec = t.tm_sec;
    drawClock(t);
  }
  delay(50);
}
