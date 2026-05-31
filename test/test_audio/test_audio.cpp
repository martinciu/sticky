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

// --- recordBufferBytes: free RAM - margin, capped at maxSeconds, even bytes ---
// 16 kHz * 2 bytes = 32000 bytes/s; 96 KB margin; 20 s cap => 640000-byte cap.
void test_recbuf_capped_at_max_seconds(void) {
  size_t b = audio::recordBufferBytes(2 * 1024 * 1024, 96 * 1024, 32000, 20.0f);
  TEST_ASSERT_EQUAL_UINT32(640000, b);  // 20 s, not the whole 2 MB
}

void test_recbuf_limited_by_free_ram(void) {
  // free 256 KB - margin 96 KB = 160 KB budget, well under the 20 s cap.
  size_t b = audio::recordBufferBytes(256 * 1024, 96 * 1024, 32000, 20.0f);
  TEST_ASSERT_EQUAL_UINT32(160 * 1024, b);
}

void test_recbuf_zero_when_below_margin(void) {
  size_t b = audio::recordBufferBytes(50 * 1024, 96 * 1024, 32000, 20.0f);
  TEST_ASSERT_EQUAL_UINT32(0, b);
}

void test_recbuf_rounds_down_to_even(void) {
  // budget = 101 bytes -> must round down to 100 (whole int16 samples).
  size_t b = audio::recordBufferBytes(96 * 1024 + 101, 96 * 1024, 32000, 20.0f);
  TEST_ASSERT_EQUAL_UINT32(100, b);
}

static void runAllTests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_dbToBar_clamps_and_scales);
  RUN_TEST(test_level_silence);
  RUN_TEST(test_level_empty_block);
  RUN_TEST(test_level_full_scale);
  RUN_TEST(test_level_sine_rms);
  RUN_TEST(test_recbuf_capped_at_max_seconds);
  RUN_TEST(test_recbuf_limited_by_free_ram);
  RUN_TEST(test_recbuf_zero_when_below_margin);
  RUN_TEST(test_recbuf_rounds_down_to_even);
  UNITY_END();
}

#ifdef ARDUINO
void setup() { delay(2000); runAllTests(); }
void loop() {}
#else
int main(int, char**) { runAllTests(); return 0; }
#endif
