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
