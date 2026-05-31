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

void test_hole_starts_fall_no_instant_penalty(void) {
  marble::Config cfg; cfg.numHoles = 1; cfg.holePenaltySec = 3.0f;
  marble::GameState s; s.timeLeft = 10.0f;
  s.ball.pos = {50.0f, 50.0f}; s.ball.vel = {20.0f, 20.0f};
  s.holes[0] = { {50.0f, 50.0f} };                 // ball centre inside the hole
  bool hit = marble::applyHoles(s, cfg);
  TEST_ASSERT_TRUE(hit);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, s.fallTimer); // fall armed for holePenaltySec
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, s.timeLeft); // NO instant time loss
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, s.ball.pos.x);// snapped into the hole
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, s.ball.pos.y);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.ball.vel.x);// velocity killed
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.ball.vel.y);
  TEST_ASSERT_TRUE(s.holeFlash > 0.0f);                // flash armed
}

void test_step_during_fall_freezes_ball_and_ticks(void) {
  marble::Config cfg; cfg.numDots = 0; cfg.numHoles = 1; cfg.holePenaltySec = 3.0f;
  marble::GameState s; marble::reset(s, cfg, 1u);
  s.timeLeft = 10.0f;
  s.holes[0] = { {60.0f, 40.0f} };
  s.ball.pos = {60.0f, 40.0f}; s.ball.vel = {0.0f, 0.0f};
  marble::applyHoles(s, cfg);                            // arm the fall
  marble::step(s, cfg, marble::Vec2{1.0f, 0.0f}, 0.5f);  // hard tilt -> must NOT move
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 60.0f, s.ball.pos.x); // frozen while falling
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 40.0f, s.ball.pos.y);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, s.fallTimer);   // 3 - 0.5
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 9.5f, s.timeLeft);    // clock ran = the penalty
}

void test_fall_completes_respawns_centre(void) {
  marble::Config cfg; cfg.numDots = 0; cfg.numHoles = 1; cfg.holePenaltySec = 3.0f;
  marble::GameState s; marble::reset(s, cfg, 1u);
  s.timeLeft = 10.0f;
  s.holes[0] = { {60.0f, 40.0f} };
  s.ball.pos = {60.0f, 40.0f};
  marble::applyHoles(s, cfg);
  marble::step(s, cfg, marble::Vec2{0.0f, 0.0f}, 5.0f);  // overshoot the fall duration
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.fallTimer);            // fall finished
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cfg.width  * 0.5f, s.ball.pos.x);// respawned at centre
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cfg.height * 0.5f, s.ball.pos.y);
}

void test_tick_clock_counts_down_and_clamps(void) {
  marble::GameState s; s.timeLeft = 1.0f;
  marble::tickClock(s, 0.4f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.6f, s.timeLeft);
  marble::tickClock(s, 1.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.timeLeft);            // clamps at 0
}

void test_reset_starts_playing_in_bounds(void) {
  marble::Config cfg;                       // 4 dots, 3 holes
  marble::GameState s;
  marble::reset(s, cfg, 7u);
  TEST_ASSERT_TRUE(s.phase == marble::Phase::Playing);
  TEST_ASSERT_EQUAL_INT(0, s.score);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, cfg.roundSeconds, s.timeLeft);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cfg.width  * 0.5f, s.ball.pos.x);  // centred
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cfg.height * 0.5f, s.ball.pos.y);
  for (int i = 0; i < cfg.numDots; ++i) {
    TEST_ASSERT_TRUE(s.dots[i].active);
    TEST_ASSERT_TRUE(s.dots[i].pos.x >= 0.0f && s.dots[i].pos.x <= cfg.width);
    TEST_ASSERT_TRUE(s.dots[i].pos.y >= 0.0f && s.dots[i].pos.y <= cfg.height);
  }
  for (int i = 0; i < cfg.numHoles; ++i) {
    TEST_ASSERT_TRUE(s.holes[i].pos.x >= 0.0f && s.holes[i].pos.x <= cfg.width);
    TEST_ASSERT_TRUE(s.holes[i].pos.y >= 0.0f && s.holes[i].pos.y <= cfg.height);
  }
}

void test_step_rolls_ball_and_ticks_clock(void) {
  marble::Config cfg; cfg.numDots = 0; cfg.numHoles = 0;
  marble::GameState s; marble::reset(s, cfg, 1u);
  float t0 = s.timeLeft, x0 = s.ball.pos.x;
  marble::step(s, cfg, marble::Vec2{0.5f, 0.0f}, 0.1f);
  TEST_ASSERT_TRUE(s.ball.pos.x > x0);   // rolled toward the tilt
  TEST_ASSERT_TRUE(s.timeLeft < t0);     // clock advanced
}

void test_step_flips_to_game_over_at_zero(void) {
  marble::Config cfg; cfg.numDots = 0; cfg.numHoles = 0; cfg.roundSeconds = 0.2f;
  marble::GameState s; marble::reset(s, cfg, 1u);
  marble::step(s, cfg, marble::Vec2{0.0f, 0.0f}, 0.5f);  // overshoot the clock
  TEST_ASSERT_TRUE(s.phase == marble::Phase::GameOver);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.timeLeft);
}

void test_step_is_noop_after_game_over(void) {
  marble::Config cfg;
  marble::GameState s; marble::reset(s, cfg, 1u);
  s.phase = marble::Phase::GameOver;
  float x = s.ball.pos.x;
  marble::step(s, cfg, marble::Vec2{1.0f, 0.0f}, 0.1f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, x, s.ball.pos.x);   // frozen
}

void test_dots_never_spawn_on_holes(void) {
  marble::Config cfg;  // 4 dots, 3 holes
  for (uint32_t seed = 1; seed <= 200; ++seed) {
    marble::GameState s;
    marble::reset(s, cfg, seed);
    for (int d = 0; d < cfg.numDots; ++d)
      for (int h = 0; h < cfg.numHoles; ++h) {
        float dx = s.dots[d].pos.x - s.holes[h].pos.x;
        float dy = s.dots[d].pos.y - s.holes[h].pos.y;
        TEST_ASSERT_TRUE(sqrtf(dx*dx + dy*dy) >= cfg.holeR + cfg.dotR);
      }
  }
}

void test_eaten_dot_respawns_off_holes(void) {
  marble::Config cfg; cfg.numDots = 1; cfg.numHoles = 3;
  for (uint32_t seed = 1; seed <= 100; ++seed) {
    marble::GameState s;
    marble::reset(s, cfg, seed);
    s.ball.pos = s.dots[0].pos;     // sit on the dot, then eat it
    marble::eatDots(s, cfg);
    for (int h = 0; h < cfg.numHoles; ++h) {
      float dx = s.dots[0].pos.x - s.holes[h].pos.x;
      float dy = s.dots[0].pos.y - s.holes[h].pos.y;
      TEST_ASSERT_TRUE(sqrtf(dx*dx + dy*dy) >= cfg.holeR + cfg.dotR);
    }
  }
}

void test_holes_stay_on_board_and_dont_overlap(void) {
  marble::Config cfg;  // 3 holes
  for (uint32_t seed = 1; seed <= 200; ++seed) {
    marble::GameState s;
    marble::reset(s, cfg, seed);
    for (int i = 0; i < cfg.numHoles; ++i) {
      // fully on the board: centre kept at least holeR from every edge
      TEST_ASSERT_TRUE(s.holes[i].pos.x >= cfg.holeR);
      TEST_ASSERT_TRUE(s.holes[i].pos.x <= cfg.width - cfg.holeR);
      TEST_ASSERT_TRUE(s.holes[i].pos.y >= cfg.holeR);
      TEST_ASSERT_TRUE(s.holes[i].pos.y <= cfg.height - cfg.holeR);
      // no overlap with any other hole (centres >= 2*holeR apart)
      for (int j = i + 1; j < cfg.numHoles; ++j) {
        float dx = s.holes[i].pos.x - s.holes[j].pos.x;
        float dy = s.holes[i].pos.y - s.holes[j].pos.y;
        TEST_ASSERT_TRUE(sqrtf(dx*dx + dy*dy) >= 2.0f * cfg.holeR);
      }
    }
  }
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
  RUN_TEST(test_hole_starts_fall_no_instant_penalty);
  RUN_TEST(test_step_during_fall_freezes_ball_and_ticks);
  RUN_TEST(test_fall_completes_respawns_centre);
  RUN_TEST(test_tick_clock_counts_down_and_clamps);
  RUN_TEST(test_reset_starts_playing_in_bounds);
  RUN_TEST(test_step_rolls_ball_and_ticks_clock);
  RUN_TEST(test_step_flips_to_game_over_at_zero);
  RUN_TEST(test_step_is_noop_after_game_over);
  RUN_TEST(test_dots_never_spawn_on_holes);
  RUN_TEST(test_eaten_dot_respawns_off_holes);
  RUN_TEST(test_holes_stay_on_board_and_dont_overlap);
  UNITY_END();
}

#ifdef ARDUINO
void setup() { delay(2000); runAllTests(); }
void loop() {}
#else
int main(int, char**) { runAllTests(); return 0; }
#endif
