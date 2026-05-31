#include <M5Unified.h>
#include "audio.h"

static constexpr uint32_t SAMPLE_RATE = 16000;  // 16 kHz mono (crisper; ~half the max clip length of 8 kHz)
static constexpr size_t   VU_BLOCK    = 256;    // ~16 ms per VU read at 16 kHz
static constexpr uint8_t  SPK_VOLUME  = 255;    // max (0..255)

// The record buffer is sized at runtime to the free internal DMA RAM (see
// setup), capped at MAX_REC_SECONDS so we never grab everything. int16 mono =>
// SAMPLE_RATE * 2 bytes per second.
static constexpr float  MAX_REC_SECONDS = 20.0f;       // upper bound on clip length
static constexpr size_t HEAP_MARGIN     = 96 * 1024;   // leave headroom for stack/runtime

enum class State { IDLE_VU, RECORDING, PLAYBACK, TONE };
static State state = State::IDLE_VU;

static M5Canvas canvas(&M5.Display);
static int16_t  vuBlock[VU_BLOCK];
static int16_t* recBuf     = nullptr;   // capture buffer (internal DMA RAM)
static size_t   recSamples = 0;         // actual capacity in samples (set in setup)
static float    recSeconds = 0.0f;      // recSamples / SAMPLE_RATE (for the progress bar)
static bool     recBufOk   = false;
static uint32_t recStart   = 0;

static uint8_t  bar = 0;
static uint8_t  peakHold = 0;
static const uint32_t toneFreqs[] = {523, 659, 784};  // C5 E5 G5
static size_t   toneIdx = 0;

// Mic and speaker share the one ES8311 codec, so only one can be active at a
// time. Always end the other before begin-ing it.
static void enterMic()     { M5.Speaker.end(); M5.Mic.begin(); }
static void enterSpeaker() { M5.Mic.end();     M5.Speaker.begin(); }

// status: short state label. progressPct >= 0 draws a capture-progress bar
// instead of the live level (used while RECORDING).
static void drawFrame(const char* status, uint16_t statusColor, int progressPct) {
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextColor(statusColor, TFT_BLACK);
  canvas.setTextSize(2);
  canvas.setCursor(4, 4);
  canvas.print(status);

  const int x = 4, y = 40, w = M5.Display.width() - 8, h = 28;
  canvas.drawRect(x, y, w, h, TFT_DARKGREY);
  int val = progressPct >= 0 ? progressPct : bar;
  int fill = (w - 2) * val / 100;
  uint16_t c = val < 60 ? TFT_GREEN : (val < 85 ? TFT_YELLOW : TFT_RED);
  canvas.fillRect(x + 1, y + 1, fill, h - 2, c);
  if (progressPct < 0) {  // peak-hold tick only in live-VU mode
    int px = x + 1 + (w - 2) * peakHold / 100;
    canvas.drawFastVLine(px, y + 1, h - 2, TFT_WHITE);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(4, y + h + 6);
  if (progressPct >= 0) canvas.printf("rec %3d%% of %.0fs", progressPct, recSeconds);
  else                  canvas.printf("level %3d/100", bar);

  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.setCursor(4, M5.Display.height() - 12);
  canvas.print(recBufOk ? "A: rec+play   B: tone" : "A: --   B: tone");
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

  // Size the record buffer to the free internal DMA RAM minus a safety margin,
  // capped at MAX_REC_SECONDS. The I2S DMA needs INTERNAL RAM (MALLOC_CAP_8BIT) --
  // not PSRAM, which isn't reliably DMA-accessible on the S3 and isn't even
  // initialised in this build -- so free internal SRAM is the real ceiling on
  // clip length. (Mirrors M5Unified's audio examples, which all use 8BIT.)
  const size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  const size_t budgetBytes  = freeInternal > HEAP_MARGIN ? freeInternal - HEAP_MARGIN : 0;
  const size_t capBytes     = (size_t)(SAMPLE_RATE * MAX_REC_SECONDS) * sizeof(int16_t);
  size_t recBytes = (budgetBytes < capBytes ? budgetBytes : capBytes) & ~((size_t)1);

  recBuf = (int16_t*)heap_caps_malloc(recBytes, MALLOC_CAP_8BIT);
  recBufOk = (recBuf != nullptr);
  if (recBufOk) {
    recSamples = recBytes / sizeof(int16_t);
    recSeconds = (float)recSamples / SAMPLE_RATE;
  }
  Serial.printf("PSRAM size=%u free=%u | internal free=%u | recBuf=%u B (%.1f s @ %u Hz)=%s\n",
                (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram(),
                (unsigned)freeInternal, (unsigned)recBytes, recSeconds,
                (unsigned)SAMPLE_RATE, recBufOk ? "OK" : "FAILED");

  // Start the mic for the live VU. Don't call Speaker.end() here: at boot the
  // speaker isn't installed yet, and ending it logs a spurious I2S error.
  M5.Mic.begin();
  if (!M5.Mic.isEnabled()) Serial.println("ERROR: mic failed to start");
}

void loop() {
  M5.update();
  switch (state) {
    case State::IDLE_VU:
      sampleVu();
      drawFrame("VU", TFT_GREEN, -1);
      if (recBufOk && M5.BtnA.wasPressed()) {
        // One gap-free capture owns the mic; poll isRecording() to detect the end.
        M5.Mic.record(recBuf, recSamples, SAMPLE_RATE);
        recStart = millis();
        state = State::RECORDING;
        Serial.println("RECORDING");
      } else if (M5.BtnB.wasPressed()) {
        enterSpeaker();
        M5.Speaker.tone(toneFreqs[toneIdx], 200);
        toneIdx = (toneIdx + 1) % 3;
        state = State::TONE;
        Serial.println("TONE");
      }
      break;

    case State::RECORDING: {
      uint32_t elapsed = millis() - recStart;
      int pct = (int)(elapsed * 100 / (uint32_t)(recSeconds * 1000));
      if (pct > 100) pct = 100;
      drawFrame("REC", TFT_RED, pct);
      // The >100 ms guard avoids a start-up race where isRecording() reads 0
      // before the capture task has begun.
      if (elapsed > 100 && M5.Mic.isRecording() == 0) {
        enterSpeaker();
        M5.Speaker.playRaw(recBuf, recSamples, SAMPLE_RATE);
        state = State::PLAYBACK;
        Serial.println("PLAYBACK");
      }
      break;
    }

    case State::PLAYBACK:
      drawFrame("PLAY", TFT_YELLOW, -1);
      if (!M5.Speaker.isPlaying()) {
        enterMic();
        state = State::IDLE_VU;
        Serial.println("IDLE_VU");
      }
      break;

    case State::TONE:
      drawFrame("TONE", TFT_MAGENTA, -1);
      if (!M5.Speaker.isPlaying()) {
        enterMic();
        state = State::IDLE_VU;
        Serial.println("IDLE_VU");
      }
      break;
  }
  delay(5);
}
