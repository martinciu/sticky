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

void test_bounce_right_wall_reflects_and_clamps(void) {
  marble::Config cfg;          // width 232, ballR 6, restitution 0.6
  marble::Ball b;
  b.pos.x = cfg.width;         // past the right edge
  b.pos.y = cfg.height / 2;
  b.vel.x = 50.0f; b.vel.y = 0.0f;
  bool hit = marble::bounceWalls(b, cfg);
  TEST_ASSERT_TRUE(hit);
  TEST_ASSERT_TRUE(b.vel.x < 0.0f);                                  // reflected
  TEST_ASSERT_TRUE(b.pos.x <= cfg.width - cfg.ballR + 0.001f);       // clamped inside
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f * cfg.restitution, -b.vel.x);// damped bounce
}

void test_bounce_none_in_centre(void) {
  marble::Config cfg;
  marble::Ball b;
  b.pos.x = cfg.width / 2; b.pos.y = cfg.height / 2;
  b.vel.x = 10.0f; b.vel.y = 10.0f;
  TEST_ASSERT_FALSE(marble::bounceWalls(b, cfg));
}

void test_eat_dot_scores_and_respawns_clear(void) {
  marble::Config cfg; cfg.numDots = 1;
  marble::GameState s; s.rng = 42;
  s.ball.pos = {50.0f, 50.0f};
  s.dots[0]  = { {50.0f, 50.0f}, true };          // sitting on the ball
  int eaten = marble::eatDots(s, cfg);
  TEST_ASSERT_EQUAL_INT(1, eaten);
  TEST_ASSERT_EQUAL_INT(1, s.score);
  TEST_ASSERT_TRUE(s.dots[0].active);             // respawned, still active
  TEST_ASSERT_TRUE(s.dots[0].pos.x >= 0.0f && s.dots[0].pos.x <= cfg.width);
  TEST_ASSERT_TRUE(s.dots[0].pos.y >= 0.0f && s.dots[0].pos.y <= cfg.height);
  float dx = s.dots[0].pos.x - s.ball.pos.x, dy = s.dots[0].pos.y - s.ball.pos.y;
  TEST_ASSERT_TRUE(sqrtf(dx*dx + dy*dy) > cfg.ballR + cfg.dotR);  // not on the ball
}

void test_eat_dot_far_away_no_score(void) {
  marble::Config cfg; cfg.numDots = 1;
  marble::GameState s; s.rng = 1;
  s.ball.pos = {10.0f, 10.0f};
  s.dots[0]  = { {200.0f, 90.0f}, true };
  TEST_ASSERT_EQUAL_INT(0, marble::eatDots(s, cfg));
  TEST_ASSERT_EQUAL_INT(0, s.score);
}

static void runAllTests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_rng_deterministic_and_in_range);
  RUN_TEST(test_integrate_rolls_toward_tilt);
  RUN_TEST(test_integrate_damping_slows_ball);
  RUN_TEST(test_bounce_right_wall_reflects_and_clamps);
  RUN_TEST(test_bounce_none_in_centre);
  RUN_TEST(test_eat_dot_scores_and_respawns_clear);
  RUN_TEST(test_eat_dot_far_away_no_score);
  UNITY_END();
}

#ifdef ARDUINO
void setup() { delay(2000); runAllTests(); }
void loop() {}
#else
int main(int, char**) { runAllTests(); return 0; }
#endif
