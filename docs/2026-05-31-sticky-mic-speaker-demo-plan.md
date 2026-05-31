# Mic + Speaker Demo (`mic_speaker_demo`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (inline — see note) or superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Why mostly inline:** Tasks 1–2 build the pure `lib/audio` level math and are real host-side TDD (`pio test -e native`) — a subagent *could* do these. Tasks 3–5 are on-device code whose only verification is **a human flashing the board and watching the LCD / listening to the speaker** — a subagent cannot observe hardware. Run those inline with the user driving the device.

**Goal:** A standalone `mic_speaker_demo` rung for the StickS3 that shows a live microphone VU meter, records ~2 s and plays it back, and plays a tone on a button — teaching that the mic and speaker share one ES8311 codec and must be toggled.

**Architecture:** Pure level math (`computeLevel`, `dbToBar`) lives in `lib/audio/` and is host-unit-tested like `lib/weather`. The device-only mic capture, speaker output, ES8311 codec handoff, and rendering live in a thin `src/mic_speaker_demo/main.cpp` running a non-blocking state machine (`IDLE_VU → RECORDING → PLAYBACK`, plus `TONE`) with flicker-free sprite rendering. A new `[env:mic_speaker_demo]` isolates the rung via `build_src_filter`; the `native` test env needs no change (PlatformIO's LDF compiles `lib/audio` as soon as the test `#include`s it).

**Tech Stack:** PlatformIO Core (CLI), Arduino-ESP32, `M5Unified` (`M5.Mic`, `M5.Speaker`, `M5Canvas`), Unity (host tests), ESP32-S3 + 8 MB PSRAM.

---

## File Structure

| File | Responsibility |
|------|----------------|
| `lib/audio/audio.h` | Pure interface: `audio::Level` struct, `computeLevel()`, `dbToBar()`. No M5/Arduino deps. |
| `lib/audio/audio.cpp` | Implementation of the level math (RMS, peak, dBFS, bar mapping). |
| `test/test_audio/test_audio.cpp` | Unity host tests for `lib/audio` (dual-mode, like `test_weather`). |
| `src/mic_speaker_demo/main.cpp` | Thin device rung: mic VU, record/playback, tone, ES8311 toggle, sprite render. |
| `platformio.ini` | Add one `[env:mic_speaker_demo]` block (`extends = device`). |

Reference (read first): `lib/weather/weather.h` + `test/test_weather/test_weather.cpp` for the lib/test conventions; `src/r3_buttons/main.cpp` and `src/flicker_demo/` for the device + sprite patterns; M5Unified `examples/Basic/Microphone` and `examples/Basic/Speaker` under `.pio/libdeps/*/M5Unified/`.

---

## Task 1: `lib/audio` — `dbToBar` (host TDD)

**Files:**
- Create: `lib/audio/audio.h`
- Create: `test/test_audio/test_audio.cpp`
- Create: `lib/audio/audio.cpp`

- [ ] **Step 1: Write `lib/audio/audio.h` (full interface — both functions declared now to avoid churn)**

```cpp
#pragma once
// Pure audio level math. Depends ONLY on the C++ standard library -- no
// Arduino/M5/I2S headers -- so it compiles and unit-tests on the native (host)
// environment, no hardware required. The device-only mic capture and speaker
// output live in src/mic_speaker_demo/main.cpp and call into this.
#include <cstddef>
#include <cstdint>

namespace audio {

struct Level {
  float   rms;   // root-mean-square of the block (linear, 0..32767)
  int16_t peak;  // max |sample| in the block (0..32767)
  float   dbfs;  // peak in dBFS: 0 dB == full scale, more negative == quieter
  uint8_t bar;   // 0..100, ready to drive a VU bar
};

// Map a dBFS value onto a 0..100 bar. Values <= floorDb clamp to 0, values
// >= ceilDb clamp to 100, linear in between. Default window: -60..0 dBFS.
uint8_t dbToBar(float dbfs, float floorDb = -60.0f, float ceilDb = 0.0f);

// Compute peak + RMS over a block of mono int16 samples, derive dBFS from the
// peak, and map it to a 0..100 bar via dbToBar(). An empty/null block returns
// a silent Level (peak 0, rms 0, bar 0).
Level computeLevel(const int16_t* samples, size_t n);

}  // namespace audio
```

- [ ] **Step 2: Write `test/test_audio/test_audio.cpp` with the `dbToBar` test only (dual-mode harness, mirrors `test_weather`)**

```cpp
// Host-side unit tests for the pure audio level math (lib/audio).
// Run on your Mac with:  pio test -e native -f test_audio   (no board needed)
#include <unity.h>
#include "audio.h"
#include <cmath>
#ifdef ARDUINO
#include <Arduino.h>  // setup()/loop()/delay() when running on the board
#endif

void setUp(void) {}
void tearDown(void) {}

void test_dbToBar_clamps_and_scales(void) {
  TEST_ASSERT_EQUAL_UINT8(0,   audio::dbToBar(-60.0f));  // floor -> 0
  TEST_ASSERT_EQUAL_UINT8(100, audio::dbToBar(0.0f));    // ceil  -> 100
  TEST_ASSERT_EQUAL_UINT8(50,  audio::dbToBar(-30.0f));  // midpoint
  TEST_ASSERT_EQUAL_UINT8(0,   audio::dbToBar(-90.0f));  // below floor clamps
  TEST_ASSERT_EQUAL_UINT8(100, audio::dbToBar(6.0f));    // above ceil clamps
}

static void runAllTests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_dbToBar_clamps_and_scales);
  UNITY_END();
}

#ifdef ARDUINO
void setup() { delay(2000); runAllTests(); }
void loop() {}
#else
int main(int, char**) { runAllTests(); return 0; }
#endif
```

- [ ] **Step 3: Run the test to verify it FAILS (no implementation yet)**

Run:
```bash
pio test -e native -f test_audio
```
Expected: build/link FAILS — `undefined reference to audio::dbToBar(...)` (the header declares it but `audio.cpp` doesn't exist yet).

- [ ] **Step 4: Write `lib/audio/audio.cpp` with `dbToBar` only**

```cpp
#include "audio.h"
#include <cmath>

namespace audio {

uint8_t dbToBar(float dbfs, float floorDb, float ceilDb) {
  if (dbfs <= floorDb) return 0;
  if (dbfs >= ceilDb) return 100;
  float frac = (dbfs - floorDb) / (ceilDb - floorDb);
  return (uint8_t)(frac * 100.0f + 0.5f);  // round to nearest
}

}  // namespace audio
```

- [ ] **Step 5: Run the test to verify it PASSES**

Run:
```bash
pio test -e native -f test_audio
```
Expected: `test_dbToBar_clamps_and_scales` PASS; suite `OK`.

- [ ] **Step 6: Commit**

```bash
git add lib/audio/audio.h lib/audio/audio.cpp test/test_audio/test_audio.cpp
git commit -m "feat(audio): pure dbToBar level mapping + host test"
```

---

## Task 2: `lib/audio` — `computeLevel` (host TDD)

**Files:**
- Modify: `test/test_audio/test_audio.cpp` (add tests + register them)
- Modify: `lib/audio/audio.cpp` (add `computeLevel`)

- [ ] **Step 1: Add the `computeLevel` tests to `test/test_audio/test_audio.cpp`**

Insert these four test functions directly after `test_dbToBar_clamps_and_scales`:

```cpp
void test_level_silence(void) {
  int16_t buf[256] = {0};
  audio::Level lv = audio::computeLevel(buf, 256);
  TEST_ASSERT_EQUAL_INT16(0, lv.peak);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, lv.rms);
  TEST_ASSERT_EQUAL_UINT8(0, lv.bar);
}

void test_level_empty_block(void) {
  audio::Level lv = audio::computeLevel(nullptr, 0);
  TEST_ASSERT_EQUAL_INT16(0, lv.peak);
  TEST_ASSERT_EQUAL_UINT8(0, lv.bar);
}

void test_level_full_scale(void) {
  int16_t buf[64];
  for (int i = 0; i < 64; ++i) buf[i] = 32767;
  audio::Level lv = audio::computeLevel(buf, 64);
  TEST_ASSERT_EQUAL_INT16(32767, lv.peak);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, lv.dbfs);  // ~0 dBFS at full scale
  TEST_ASSERT_EQUAL_UINT8(100, lv.bar);
}

void test_level_sine_rms(void) {
  const int   N  = 256;
  const float A  = 16384.0f;             // half of full scale
  const float PI = 3.14159265358979f;
  int16_t buf[N];
  for (int i = 0; i < N; ++i) {
    buf[i] = (int16_t)lroundf(A * sinf(2.0f * PI * 4.0f * i / N));  // 4 cycles
  }
  audio::Level lv = audio::computeLevel(buf, N);
  // RMS of a sine of amplitude A is A/sqrt(2) ~= 11585.
  TEST_ASSERT_FLOAT_WITHIN(200.0f, A / sqrtf(2.0f), lv.rms);
  TEST_ASSERT_INT16_WITHIN(2, (int16_t)A, lv.peak);
}
```

Then register them in `runAllTests()` so the body reads:

```cpp
static void runAllTests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_dbToBar_clamps_and_scales);
  RUN_TEST(test_level_silence);
  RUN_TEST(test_level_empty_block);
  RUN_TEST(test_level_full_scale);
  RUN_TEST(test_level_sine_rms);
  UNITY_END();
}
```

- [ ] **Step 2: Run the tests to verify the new ones FAIL**

Run:
```bash
pio test -e native -f test_audio
```
Expected: build/link FAILS — `undefined reference to audio::computeLevel(...)` (declared in the header, not yet defined).

- [ ] **Step 3: Add `computeLevel` to `lib/audio/audio.cpp`**

Insert this function inside `namespace audio { ... }`, after `dbToBar`:

```cpp
Level computeLevel(const int16_t* samples, size_t n) {
  if (samples == nullptr || n == 0) {
    return Level{0.0f, 0, -160.0f, 0};  // silent
  }
  int32_t peak = 0;
  double  sumSquares = 0.0;
  for (size_t i = 0; i < n; ++i) {
    int32_t s = samples[i];
    int32_t a = s < 0 ? -s : s;  // |sample| in 32-bit (avoids INT16_MIN overflow)
    if (a > peak) peak = a;
    sumSquares += (double)s * (double)s;
  }
  float rms = (float)std::sqrt(sumSquares / (double)n);
  // Reference full scale = 32767; silence collapses to a low floor sentinel.
  float dbfs = peak > 0 ? 20.0f * std::log10((float)peak / 32767.0f) : -160.0f;
  int16_t peak16 = peak > 32767 ? 32767 : (int16_t)peak;
  return Level{rms, peak16, dbfs, dbToBar(dbfs)};
}
```

- [ ] **Step 4: Run the tests to verify all PASS**

Run:
```bash
pio test -e native -f test_audio
```
Expected: 5 tests, all PASS; suite `OK`.

- [ ] **Step 5: Confirm the existing weather suite still passes (no regression)**

Run:
```bash
pio test -e native
```
Expected: both `test_audio` and `test_weather` suites report `OK`.

- [ ] **Step 6: Commit**

```bash
git add lib/audio/audio.cpp test/test_audio/test_audio.cpp
git commit -m "feat(audio): computeLevel (peak/RMS/dBFS) + host tests"
```

---

## Task 3: Device rung v1 — live VU meter (flash + observe)

**Files:**
- Modify: `platformio.ini` (add the env)
- Create: `src/mic_speaker_demo/main.cpp`

- [ ] **Step 1: Add the env to `platformio.ini`**

Append after the `[env:r9_lvgl_ui]` block (and before `[env:native]`):

```ini
[env:mic_speaker_demo]
extends = device
build_src_filter = -<*> +<mic_speaker_demo/>
```

- [ ] **Step 2: Write `src/mic_speaker_demo/main.cpp` (VU meter only)**

```cpp
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
```

- [ ] **Step 3: Build (no board needed)**

Run:
```bash
pio run -e mic_speaker_demo
```
Expected: `SUCCESS`. (Confirms `lib/audio` links into a device build too.)

- [ ] **Step 4: Flash and observe**

Run:
```bash
pio run -e mic_speaker_demo -t upload -t monitor
```
Expected:
- LCD shows `VU`, an empty bordered bar, and `level 0/100` in a quiet room.
- **Talk / tap near the mic** → the bar grows green→yellow→red and the white peak tick jumps then slowly decays. `level NNN/100` tracks it.
- Serial may print the mic error line only if the mic failed to start (it shouldn't).

If the bar pins at 100 even when quiet, note it — the mic has a 16× default gain; we'll widen the `dbToBar` floor in Task 5 tuning. Exit the monitor with `Ctrl+C`.

- [ ] **Step 5: Commit**

```bash
git add platformio.ini src/mic_speaker_demo/main.cpp
git commit -m "feat(mic_speaker_demo): live VU meter from mic"
```

---

## Task 4: Device rung v2 — codec toggle + tone on BtnB (flash + observe)

**Files:**
- Modify: `src/mic_speaker_demo/main.cpp` (replace whole file)

- [ ] **Step 1: Replace `src/mic_speaker_demo/main.cpp` with the state-machine + tone version**

```cpp
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
```

- [ ] **Step 2: Build**

Run:
```bash
pio run -e mic_speaker_demo
```
Expected: `SUCCESS`.

- [ ] **Step 3: Flash and observe**

Run:
```bash
pio run -e mic_speaker_demo -t upload -t monitor
```
Expected:
- Live VU as before, with `A: (soon)   B: tone` hint at the bottom.
- Press **BtnB** (KEY2): a ~200 ms tone plays through the speaker; the screen flashes `TONE` (magenta) then returns to `VU`. Serial prints `TONE` then `IDLE_VU`.
- Press BtnB three times: the pitch cycles C5 → E5 → G5.
- Confirm the VU resumes responding to sound after each tone (proves the codec toggled mic→speaker→mic cleanly). Note any audible click on the toggle.

Exit with `Ctrl+C`.

- [ ] **Step 4: Commit**

```bash
git add src/mic_speaker_demo/main.cpp
git commit -m "feat(mic_speaker_demo): ES8311 codec toggle + tone on BtnB"
```

---

## Task 5: Device rung v3 — record + playback on BtnA, final polish (flash + observe)

**Files:**
- Modify: `src/mic_speaker_demo/main.cpp` (replace whole file — final version)

- [ ] **Step 1: Replace `src/mic_speaker_demo/main.cpp` with the final version (adds the 2 s PSRAM record buffer, RECORDING/PLAYBACK states, and PSRAM-alloc error handling)**

```cpp
#include <M5Unified.h>
#include "audio.h"

static constexpr uint32_t SAMPLE_RATE = 16000;            // 16 kHz mono
static constexpr size_t   VU_BLOCK    = 256;              // ~16 ms per VU read
static constexpr float    REC_SECONDS = 2.0f;
static constexpr size_t   REC_SAMPLES = (size_t)(SAMPLE_RATE * REC_SECONDS);  // 32000
static constexpr uint8_t  SPK_VOLUME  = 200;              // 0..255

enum class State { IDLE_VU, RECORDING, PLAYBACK, TONE };
static State state = State::IDLE_VU;

static M5Canvas canvas(&M5.Display);
static int16_t  vuBlock[VU_BLOCK];
static int16_t* recBuf   = nullptr;   // 2 s capture buffer (PSRAM)
static bool     recBufOk = false;
static uint32_t recStart = 0;

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
  if (progressPct >= 0) canvas.printf("rec %3d%%", progressPct);
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

  // 2 s record buffer in PSRAM (board has 8 MB OPI PSRAM).
  recBuf = (int16_t*)ps_malloc(REC_SAMPLES * sizeof(int16_t));
  recBufOk = (recBuf != nullptr);
  if (!recBufOk) Serial.println("ERROR: record buffer alloc failed (PSRAM)");

  enterMic();  // start in mic mode for the live VU
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
        M5.Mic.record(recBuf, REC_SAMPLES, SAMPLE_RATE);
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
      int pct = (int)(elapsed * 100 / (uint32_t)(REC_SECONDS * 1000));
      if (pct > 100) pct = 100;
      drawFrame("REC", TFT_RED, pct);
      // The >100 ms guard avoids a start-up race where isRecording() reads 0
      // before the capture task has begun.
      if (elapsed > 100 && M5.Mic.isRecording() == 0) {
        enterSpeaker();
        M5.Speaker.playRaw(recBuf, REC_SAMPLES, SAMPLE_RATE);
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
```

- [ ] **Step 2: Build**

Run:
```bash
pio run -e mic_speaker_demo
```
Expected: `SUCCESS`.

- [ ] **Step 3: Flash and observe the full demo**

Run:
```bash
pio run -e mic_speaker_demo -t upload -t monitor
```
Expected:
- Idle: live `VU` with hint `A: rec+play   B: tone`.
- Press **BtnA** (KEY1): screen shows `REC` (red) with `rec 0%→100%` filling over ~2 s while you speak; then automatically `PLAY` (yellow) and your ~2 s clip plays back through the speaker; then returns to `VU`. Serial prints `RECORDING → PLAYBACK → IDLE_VU`.
- Press **BtnB**: tone still works as in Task 4.
- Pressing a button mid-record/playback does nothing (handled only in `IDLE_VU`).
- If serial printed `record buffer alloc failed` at boot, the hint shows `A: --` and BtnA is disabled (tone/VU still work) — that's the error path.

- [ ] **Step 4: Tune empirically, then re-flash if needed**

Adjust to taste and re-run Step 3:
- **VU too sensitive / pinned at 100:** widen the floor — change the live-VU bar to use a wider window, e.g. in `sampleVu()` replace `bar = lv.bar;` with `bar = audio::dbToBar(lv.dbfs, -40.0f, -3.0f);` (less headroom, lower gain feel). Pick values that look right.
- **Playback too quiet/loud:** change `SPK_VOLUME` (0..255).
- **Clip length:** change `REC_SECONDS`.

Leave the chosen values in the file.

- [ ] **Step 5: Final regression — host tests still green**

Run:
```bash
pio test -e native
```
Expected: `test_audio` + `test_weather` both `OK` (the device edits didn't touch `lib/audio`).

- [ ] **Step 6: Commit**

```bash
git add src/mic_speaker_demo/main.cpp
git commit -m "feat(mic_speaker_demo): record + playback on BtnA; final polish"
```

---

## Wrap-up & integration

- [ ] **Record hardware findings** — append a short `## Findings` note to the design doc (`docs/2026-05-31-sticky-mic-speaker-demo-design.md`) and commit:
  - Confirmed `BtnA`/`BtnB` → physical KEY1/KEY2 mapping.
  - Whether an audible click occurs on the ES8311 mic↔speaker toggle.
  - The `dbToBar` floor/ceil window and `SPK_VOLUME` that felt right.
  - Mic default sample rate/format actually used.

- [ ] **Decide branch integration** via `superpowers:finishing-a-development-branch` (open a PR for branch `mic-speaker-demo`, or keep local).

---

## Self-Review notes (author)

- **Spec coverage:** VU meter (Task 3), record→playback (Task 5), tone (Task 4), ES8311 codec toggle (Tasks 4–5 via `enterMic`/`enterSpeaker`), host-tested level math (Tasks 1–2), PSRAM buffer + alloc error handling (Task 5), `[env:mic_speaker_demo]` + no native-env change (Task 3). All design §3–§8 items map to a task. ✅
- **Placeholder scan:** No TBD/TODO; every code step shows complete content; verification steps give exact commands + expected screen/serial output. ✅
- **Type consistency:** `audio::Level{rms, peak, dbfs, bar}`, `computeLevel(const int16_t*, size_t)`, `dbToBar(float, float, float)` used identically across Tasks 1–5. `enterMic`/`enterSpeaker`, `drawFrame`, `sampleVu`, `State` enum names stable from the task they're introduced onward (v2 `drawFrame(status,color)` is intentionally superseded by v3 `drawFrame(status,color,progressPct)` — whole-file replacements, not partial edits). ✅
- **Refinement vs spec:** RECORDING uses one gap-free `record()` + `isRecording()` poll with time-based progress (no live VU during capture) rather than chunked live-VU recording — the design doc §5 was updated to match. ✅
- **Hardware verification:** every device task ends in flash + observe with concrete expected results, per the project's "observe the LCD/serial" model. ✅
