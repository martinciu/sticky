#include <M5Unified.h>
#include "audio.h"

static constexpr uint32_t SAMPLE_RATE = 16000;
static constexpr size_t   VU_BLOCK    = 256;
static constexpr uint8_t  SPK_VOLUME  = 200;  // 0..255

enum class State { IDLE_VU, TONE };
static State state = State::IDLE_VU;

static M5Canvas canvas(&M5.Display);
static int16_t  vuBlock[VU_BLOCK];
static uint8_t  bar = 0;
static uint8_t  peakHold = 0;
static const uint32_t toneFreqs[] = {523, 659, 784};  // C5 E5 G5
static size_t   toneIdx = 0;

// Mic and speaker share the one ES8311 codec, so only one can be active at a
// time. Always end the other before begin-ing it.
static void enterMic()     { M5.Speaker.end(); M5.Mic.begin(); }
static void enterSpeaker() { M5.Mic.end();     M5.Speaker.begin(); }

static void drawFrame(const char* status, uint16_t statusColor) {
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextColor(statusColor, TFT_BLACK);
  canvas.setTextSize(2);
  canvas.setCursor(4, 4);
  canvas.print(status);

  const int x = 4, y = 40, w = M5.Display.width() - 8, h = 28;
  canvas.drawRect(x, y, w, h, TFT_DARKGREY);
  int fill = (w - 2) * bar / 100;
  uint16_t c = bar < 60 ? TFT_GREEN : (bar < 85 ? TFT_YELLOW : TFT_RED);
  canvas.fillRect(x + 1, y + 1, fill, h - 2, c);
  int px = x + 1 + (w - 2) * peakHold / 100;
  canvas.drawFastVLine(px, y + 1, h - 2, TFT_WHITE);

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(4, y + h + 6);
  canvas.printf("level %3d/100", bar);

  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.setCursor(4, M5.Display.height() - 12);
  canvas.print("A: (soon)   B: tone");
  canvas.pushSprite(0, 0);
}

static void sampleVu() {
  if (M5.Mic.record(vuBlock, VU_BLOCK, SAMPLE_RATE)) {
    audio::Level lv = audio::computeLevel(vuBlock, VU_BLOCK);
    bar = lv.bar;
    if (bar > peakHold) peakHold = bar;
  }
  if (peakHold > 0) peakHold -= 1;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);
  canvas.createSprite(M5.Display.width(), M5.Display.height());
  M5.Speaker.setVolume(SPK_VOLUME);

  enterMic();  // start in mic mode for the live VU
  if (!M5.Mic.isEnabled()) Serial.println("ERROR: mic failed to start");
}

void loop() {
  M5.update();
  switch (state) {
    case State::IDLE_VU:
      sampleVu();
      drawFrame("VU", TFT_GREEN);
      if (M5.BtnB.wasPressed()) {
        enterSpeaker();
        M5.Speaker.tone(toneFreqs[toneIdx], 200);  // 200 ms beep
        toneIdx = (toneIdx + 1) % 3;
        state = State::TONE;
        Serial.println("TONE");
      }
      break;
    case State::TONE:
      drawFrame("TONE", TFT_MAGENTA);
      if (!M5.Speaker.isPlaying()) {  // beep finished
        enterMic();
        state = State::IDLE_VU;
        Serial.println("IDLE_VU");
      }
      break;
  }
  delay(5);
}
