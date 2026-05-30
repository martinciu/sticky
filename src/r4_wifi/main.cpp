#include "secrets.h" // WIFI_SSID, WIFI_PASSWORD (copied from secrets.example.h)
#include <M5Unified.h>
#include <WiFi.h>

const int RSSI_Y = 52; // y-position of the (live) RSSI line
uint32_t lastRssi = 0; // millis() timestamp of the last RSSI refresh

// Redraw ONLY the RSSI line (not the whole screen). Opaque text (foreground +
// background color) overwrites the old pixels; the fixed %4d width + trailing
// space clear leftover characters when the number shrinks (e.g. -100 -> -54).
void drawRssi() {
  M5.Display.setTextSize(2);
  M5.Display.setCursor(4, RSSI_Y);
  if (WiFi.status() == WL_CONNECTED) {
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.printf("RSSI:%4d dBm ", WiFi.RSSI());
  } else {
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.print("RSSI: --      ");
  }
  int bat = M5.Power.getBatteryLevel(); // 0-100, or -1 if unsupported
  bool charging = M5.Power.isCharging();
  M5.Display.setCursor(4, 70);
  M5.Display.setTextColor(charging ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
  M5.Display.printf("Bat: %d%%%s", bat, charging ? " (chg)" : "");
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  M5.Display.getBrightness();
  M5.Display.setRotation(1);
  M5.Display.setTextSize(2);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.print("WiFi: connecting");
  Serial.printf("Connecting to %s\n", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(300);
    Serial.print(".");
    M5.Display.print(".");
  }

  // Static part (only changes on connect): status + IP.
  M5.Display.fillScreen(TFT_BLACK);
  if (WiFi.status() == WL_CONNECTED) {
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.setCursor(4, 4);
    M5.Display.println("Connected!");
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setCursor(4, 28);
    M5.Display.printf("IP:%s", WiFi.localIP().toString().c_str());
    Serial.printf("\nConnected. IP=%s\n", WiFi.localIP().toString().c_str());
  } else {
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.setCursor(4, 4);
    M5.Display.println("FAILED");
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setCursor(4, 28);
    M5.Display.print("check secrets.h");
    Serial.println("\nWiFi connect failed.");
  }
  drawRssi(); // first reading
}

void loop() {
  M5.update();

  // Non-blocking timer: refresh RSSI once per second WITHOUT freezing the loop
  // (no delay(1000)). This is the exact pattern the clock will use to tick.
  if (millis() - lastRssi >= 1000) {
    lastRssi = millis();
    drawRssi();
  }
  sleep(500);
}
