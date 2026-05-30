#include <M5Unified.h>

int counter = 0;
const char *lastBtn = "none";

// Redraw the whole screen. Called once at boot and again on every button
// press (event-driven) — so plain M5.Display drawing is fine here; no need for
// the sprite/double-buffering from rung 2, since we're not animating per frame.
void draw() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(2);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.printf("Count: %d", counter);

  M5.Display.setCursor(4, 32);
  M5.Display.printf("Last: %s", lastBtn);

  int bat = M5.Power.getBatteryLevel(); // 0-100, or -1 if unsupported
  bool charging = M5.Power.isCharging();
  M5.Display.setCursor(4, 60);
  M5.Display.setTextColor(charging ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
  M5.Display.printf("Bat: %d%%%s", bat, charging ? " (chg)" : "");
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg); // bring up the board (once)
  Serial.begin(115200);
  M5.Display.setRotation(1);
  draw(); // first render
}

void loop() {
  M5.update(); // 1. sample inputs this instant

  if (M5.BtnA.wasPressed()) { // 2. react to events (rising edge)
    counter++;
    lastBtn = "A";
    Serial.println("BtnA pressed -- note which PHYSICAL button this is");
    draw(); // 3. render
  }
  if (M5.BtnB.wasPressed()) {
    counter--;
    lastBtn = "B";
    Serial.println("BtnB pressed -- note which PHYSICAL button this is");
    draw();
  }

  delay(10); // 4. pace the loop + yield to FreeRTOS
}
