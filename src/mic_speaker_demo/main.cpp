#include <M5Unified.h>
#include "audio.h"

static constexpr uint32_t SAMPLE_RATE = 16000;  // 16 kHz mono
static constexpr size_t   VU_BLOCK    = 256;     // ~16 ms per VU read

static M5Canvas canvas(&M5.Display);
static int16_t  vuBlock[VU_BLOCK];
static uint8_t  bar = 0;        // current level 0..100
static uint8_t  peakHold = 0;   // decaying peak marker

static void drawVu() {
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextColor(TFT_GREEN, TFT_BLACK);
  canvas.setTextSize(2);
  canvas.setCursor(4, 4);
  canvas.print("VU");

  const int x = 4, y = 40, w = M5.Display.width() - 8, h = 28;
  canvas.drawRect(x, y, w, h, TFT_DARKGREY);
  int fill = (w - 2) * bar / 100;
  uint16_t c = bar < 60 ? TFT_GREEN : (bar < 85 ? TFT_YELLOW : TFT_RED);
  canvas.fillRect(x + 1, y + 1, fill, h - 2, c);
  int px = x + 1 + (w - 2) * peakHold / 100;
  canvas.drawFastVLine(px, y + 1, h - 2, TFT_WHITE);  // peak-hold tick

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(4, y + h + 6);
  canvas.printf("level %3d/100", bar);
  canvas.pushSprite(0, 0);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);
  canvas.createSprite(M5.Display.width(), M5.Display.height());

  // Mic and speaker share the one ES8311 codec; ensure the speaker is off so
  // the mic can own it for the live VU.
  M5.Speaker.end();
  M5.Mic.begin();
  if (!M5.Mic.isEnabled()) Serial.println("ERROR: mic failed to start");
}

void loop() {
  M5.update();
  if (M5.Mic.record(vuBlock, VU_BLOCK, SAMPLE_RATE)) {
    audio::Level lv = audio::computeLevel(vuBlock, VU_BLOCK);
    bar = lv.bar;
    if (bar > peakHold) peakHold = bar;
  }
  if (peakHold > 0) peakHold -= 1;  // decay
  drawVu();
  delay(5);
}
