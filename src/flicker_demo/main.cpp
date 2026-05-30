// DELIBERATELY BAD rendering, for comparison with the `widget` env.
// It draws straight to the LCD and clears the whole screen with fillScreen()
// every second -- so you SEE the redraw: a black flash, then elements popping
// in. Flash this, then flash `widget` (sprite/double-buffered) to feel the
// difference. Reuses config.h + secrets.h; no weather, just the ticking face.

#include <M5Unified.h>
#include <WiFi.h>
#include <time.h>
#include "secrets.h"
#include "config.h"

int lastSec = -1;

void drawCentered(const char *s, int y) {
  M5.Display.setCursor((M5.Display.width() - M5.Display.textWidth(s)) / 2, y);
  M5.Display.print(s);
}

void renderDirect(const struct tm &t) {
  M5.Display.fillScreen(TFT_BLACK); // <-- the culprit: clears the REAL screen

  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(TFT_GREEN);
  M5.Display.setCursor(2, 2);
  M5.Display.print("wifi");

  char b[16];
  snprintf(b, sizeof(b), "%d%%", M5.Power.getBatteryLevel());
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.setCursor(M5.Display.width() - M5.Display.textWidth(b) - 2, 2);
  M5.Display.print(b);

  char hms[16];
  strftime(hms, sizeof(hms), "%H:%M:%S", &t);
  M5.Display.setFont(&fonts::Font7);
  M5.Display.setTextColor(TFT_CYAN);
  drawCentered(hms, 24);

  char date[32];
  strftime(date, sizeof(date), "%a  %Y-%m-%d", &t);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextColor(TFT_WHITE);
  drawCentered(date, 80);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(10, 50);
  M5.Display.print("WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  for (uint32_t t0 = millis();
       WiFi.status() != WL_CONNECTED && millis() - t0 < 20000;)
    delay(250);

  configTzTime(WIDGET_TZ, "pool.ntp.org", "time.google.com");
  struct tm t;
  getLocalTime(&t, 10000);
}

void loop() {
  M5.update();
  struct tm t;
  if (getLocalTime(&t) && t.tm_sec != lastSec) {
    lastSec = t.tm_sec;
    renderDirect(t); // direct to LCD -> visible flash each second
  }
  delay(20);
}
