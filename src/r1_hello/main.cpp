#include "lgfx/v1/misc/enum.hpp"
#include <M5Unified.h>

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg); // inits the StickS3: display, power, buttons, etc.

  Serial.begin(115200);
  Serial.println("Hello from StickS3!");

  M5.Display.setRotation(1); // landscape: ~240 wide x 135 tall
  M5.Display.setFont(&fonts::efontJA_16); // Unicode font w/ Polish glyphs (grows binary)
  M5.Display.fillScreen(TFT_GOLD);
  M5.Display.setTextColor(TFT_BLACK, TFT_GOLD);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 10);
  M5.Display.println("Hello,");
  M5.Display.println("StickS3!");
  M5.Display.println("Żółwik!");
}

void loop() {
  M5.update();
  Serial.printf("uptime: %lu ms\n", millis());
  delay(1000);
}
