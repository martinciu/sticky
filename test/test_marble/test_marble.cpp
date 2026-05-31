// Host-side unit tests for the pure marble physics/game core (lib/marble).
// Run on your Mac with:  pio test -e native -f test_marble   (no board needed)
#include <unity.h>
#include "marble.h"
#include <cmath>
#ifdef ARDUINO
#include <Arduino.h>  // setup()/loop()/delay() when running on the board
#endif

void setUp(void) {}
void tearDown(void) {}

void test_rng_deterministic_and_in_range(void) {
  uint32_t a = 12345, b = 12345;
  TEST_ASSERT_EQUAL_UINT32(marble::rngNext(a), marble::rngNext(b));  // same seed, same value
  uint32_t s = 999;
  for (int i = 0; i < 1000; ++i) {
    float v = marble::rngRange(s, 10.0f, 20.0f);
    TEST_ASSERT_TRUE(v >= 10.0f && v < 20.0f);
  }
}

void test_integrate_rolls_toward_tilt(void) {
  marble::Config cfg;     // defaults
  marble::Ball b;         // at origin, zero velocity
  marble::integrate(b, marble::Vec2{0.5f, 0.0f}, cfg, 0.1f);
  TEST_ASSERT_TRUE(b.vel.x > 0.0f);                 // gravity pulled it +x
  TEST_ASSERT_TRUE(b.pos.x > 0.0f);                 // and it moved +x
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, b.vel.y);   // nothing on the y axis
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, b.pos.y);
}

void test_integrate_damping_slows_ball(void) {
  marble::Config cfg;
  marble::Ball b; b.vel.x = 100.0f;                 // moving, no tilt
  marble::integrate(b, marble::Vec2{0.0f, 0.0f}, cfg, 0.1f);
  float v1 = b.vel.x;
  marble::integrate(b, marble::Vec2{0.0f, 0.0f}, cfg, 0.1f);
  float v2 = b.vel.x;
  TEST_ASSERT_TRUE(v1 < 100.0f);   // damping bled speed
  TEST_ASSERT_TRUE(v2 < v1);       // and keeps bleeding
  TEST_ASSERT_TRUE(v2 > 0.0f);     // same direction
}

static void runAllTests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_rng_deterministic_and_in_range);
  RUN_TEST(test_integrate_rolls_toward_tilt);
  RUN_TEST(test_integrate_damping_slows_ball);
  UNITY_END();
}

#ifdef ARDUINO
void setup() { delay(2000); runAllTests(); }
void loop() {}
#else
int main(int, char**) { runAllTests(); return 0; }
#endif
