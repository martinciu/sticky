# Tilt Marble Game (`marble_demo`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (inline — see note) or superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Why mostly inline:** Tasks 1–5 build the pure `lib/marble` physics + game core and are real host-side TDD (`pio test -e native`) — a subagent *could* do these. Tasks 6–8 are on-device code whose only verification is **a human flashing the board, tilting it, and watching the LCD** — a subagent cannot observe hardware. Run those inline with the user driving the device.

**Goal:** A standalone `marble_demo` rung for the StickS3 that turns the IMU into a tilt-controlled rolling marble — hold the stick flat, tilt to roll a ball (real momentum, wall bounce), collect dots and dodge holes against a 60-second clock.

**Architecture:** All physics, collisions, scoring, and the timer live in a pure `lib/marble/` module (a deterministic `step(state, cfg, tilt, dt)` plus small testable helpers) — no M5/Arduino headers, host-unit-tested like `lib/weather` and `lib/audio`. The device-only IMU read + calibration (`imu_input`), flicker-free sprite rendering (`board_view`), and the `millis()`-paced state machine (`app`) stay thin in `src/marble_demo/`. A new `[env:marble_demo]` isolates the rung via `build_src_filter`; the `native` test env needs no change (PlatformIO's LDF compiles `lib/marble` as soon as the test `#include`s it).

**Tech Stack:** PlatformIO Core (CLI), Arduino-ESP32, `M5Unified` (`M5.Imu`, `M5Canvas`, `M5.BtnA/BtnB`), Unity (host tests), ESP32-S3 + BMI270 IMU.

---

## File Structure

| File | Responsibility |
|------|----------------|
| `lib/marble/marble.h` | Pure interface: `Vec2`, `Ball`, `Dot`, `Hole`, `Phase`, `Config`, `GameState`, and the pure functions (`rngNext`, `rngRange`, `integrate`, `bounceWalls`, `eatDots`, `applyHoles`, `tickClock`, `reset`, `step`). No M5/Arduino deps. |
| `lib/marble/marble.cpp` | Implementation of the physics integrator, wall bounce, collisions, scoring, timer, deterministic spawn placement. |
| `test/test_marble/test_marble.cpp` | Unity host tests for `lib/marble` (dual-mode harness, like `test_weather`/`test_audio`). |
| `src/marble_demo/imu_input.{h,cpp}` | Device: read `M5.Imu.getAccel`, subtract boot calibration → `marble::Vec2` tilt; axis/sign mapping constants. |
| `src/marble_demo/board_view.{h,cpp}` | Device: render a `GameState` into one full-screen `M5Canvas` → `pushSprite`. Holds no model state. |
| `src/marble_demo/app.{h,cpp}` | Device: the `millis()`-paced state machine; owns `ImuInput` + `BoardView` + `marble::GameState`/`Config`. |
| `src/marble_demo/main.cpp` | Thin wiring: `App app; setup(){app.begin();} loop(){app.loop();}`. |
| `platformio.ini` | Add one `[env:marble_demo]` block (`extends = device`). |

Reference (read first): `lib/weather/weather.h` + `test/test_weather/test_weather.cpp` for the lib/test conventions; `src/mic_speaker_demo/` (`app`/`audio_io`/`vu_view`/`main` split) for the device file split; `src/widget/main.cpp` and `src/flicker_demo/` for the sprite render loop; M5Unified `examples/Basic/Imu/Imu.ino` under `.pio/libdeps/*/M5Unified/` for `getAccel`.

**Note on `damping` (resolves the design's flagged friction ambiguity):** the design doc §6 sketched friction as a per-*step* multiplier (~0.98), which is frame-rate-dependent. This plan uses `cfg.damping` as a per-*second* exponential decay (`vel *= exp(-damping*dt)`) so feel is independent of frame rate. The design doc is updated to match.

---

## Task 1: `lib/marble` scaffold — header, PRNG, and `integrate` (host TDD)

**Files:**
- Create: `lib/marble/marble.h`
- Create: `test/test_marble/test_marble.cpp`
- Create: `lib/marble/marble.cpp`

- [ ] **Step 1: Write `lib/marble/marble.h` (full interface up front to avoid churn)**

```cpp
#pragma once
// Pure tilt-marble physics + game logic. Depends ONLY on the C++ standard
// library -- no Arduino/M5/WiFi headers -- so it compiles and unit-tests on the
// native (host) environment, no hardware required. The device-only IMU read,
// rendering, and state machine live in src/marble_demo/ and call into this.
#include <cstddef>
#include <cstdint>

namespace marble {

struct Vec2 { float x = 0.0f; float y = 0.0f; };

struct Ball { Vec2 pos; Vec2 vel; };
struct Dot  { Vec2 pos; bool active = false; };
struct Hole { Vec2 pos; };

enum class Phase : uint8_t { Calibrate, Playing, GameOver };

constexpr int MAX_DOTS  = 8;
constexpr int MAX_HOLES = 8;

// All tunables. Lengths/positions are in board pixels (the play area, not the
// full 240x135 screen). The device picks width/height to match the drawn board.
struct Config {
  float width        = 232.0f;  // board area width  (px)
  float height       = 104.0f;  // board area height (px)
  float ballR        = 6.0f;
  float dotR         = 3.0f;
  float holeR        = 8.0f;
  float gravity      = 900.0f;  // px/s^2 per g of tilt
  float damping      = 1.5f;    // velocity decay per second (frame-rate independent)
  float restitution  = 0.6f;    // wall-bounce energy retained (0..1)
  float roundSeconds   = 60.0f;
  float holePenaltySec = 3.0f;
  float holeFlashSec   = 0.4f;  // how long the "just hit a hole" flash lingers
  int   numDots        = 4;
  int   numHoles       = 3;
};

struct GameState {
  Phase phase = Phase::Calibrate;
  Ball  ball;
  Dot   dots[MAX_DOTS];
  Hole  holes[MAX_HOLES];
  int   score    = 0;
  float timeLeft = 0.0f;
  float holeFlash = 0.0f;   // >0 == just hit a hole (render hint), seconds
  float rollAngle = 0.0f;   // accumulated roll for the spin animation (radians)
  uint32_t rng = 1u;        // xorshift32 state (deterministic spawns)
};

// --- pure helpers (each independently testable) ---

// xorshift32: advance state, return next value. Treats a 0 state as 1.
uint32_t rngNext(uint32_t& state);
// Uniform float in [lo, hi) drawn from the PRNG.
float rngRange(uint32_t& state, float lo, float hi);

// Integrate one ball for dt seconds: gravity from tilt, exponential velocity
// damping (frame-rate independent), then move by velocity.
void integrate(Ball& ball, Vec2 tilt, const Config& cfg, float dt);

// Reflect the ball off the 4 walls, clamp it inside, scale the bounced velocity
// component by restitution. Returns true if any wall was hit.
bool bounceWalls(Ball& ball, const Config& cfg);

// Eat every overlapping active dot: score++ and respawn it clear of the ball.
// Returns how many were eaten this call.
int eatDots(GameState& s, const Config& cfg);

// If the ball's centre is inside any hole: subtract the time penalty (clamped
// >= 0), reset the ball to centre with zero velocity, set holeFlash. Returns
// true if a hole was hit.
bool applyHoles(GameState& s, const Config& cfg);

// Count the round clock down by dt; clamp at 0.
void tickClock(GameState& s, float dt);

// Start a fresh round: seed the PRNG, centre the ball, place numHoles holes
// (clear of the centre) then numDots dots (clear of the centre), reset score
// and timeLeft, set phase = Playing.
void reset(GameState& s, const Config& cfg, uint32_t seed);

// Advance one frame when Playing: integrate + bounceWalls + eatDots + applyHoles
// + tickClock + roll/flash decay, then flip to GameOver at timeLeft 0. No-op in
// any other phase.
void step(GameState& s, const Config& cfg, Vec2 tilt, float dt);

}  // namespace marble
```

- [ ] **Step 2: Write `test/test_marble/test_marble.cpp` with the PRNG + `integrate` tests only (dual-mode harness, mirrors `test_weather`)**

```cpp
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
```

- [ ] **Step 3: Run the test to verify it FAILS (no implementation yet)**

Run:
```bash
pio test -e native -f test_marble
```
Expected: build/link FAILS — `undefined reference to marble::rngNext(...)` / `marble::integrate(...)` (the header declares them but `marble.cpp` doesn't exist yet).

- [ ] **Step 4: Write `lib/marble/marble.cpp` with the PRNG + `integrate` only**

```cpp
#include "marble.h"
#include <cmath>

namespace marble {

uint32_t rngNext(uint32_t& state) {
  uint32_t x = state ? state : 1u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state = x;
  return x;
}

float rngRange(uint32_t& state, float lo, float hi) {
  float u = (rngNext(state) & 0xFFFFFFu) / (float)0x1000000;  // [0,1)
  return lo + u * (hi - lo);
}

void integrate(Ball& b, Vec2 tilt, const Config& cfg, float dt) {
  b.vel.x += cfg.gravity * tilt.x * dt;
  b.vel.y += cfg.gravity * tilt.y * dt;
  float decay = std::exp(-cfg.damping * dt);  // frame-rate-independent friction
  b.vel.x *= decay;
  b.vel.y *= decay;
  b.pos.x += b.vel.x * dt;
  b.pos.y += b.vel.y * dt;
}

}  // namespace marble
```

- [ ] **Step 5: Run the test to verify it PASSES**

Run:
```bash
pio test -e native -f test_marble
```
Expected: 3 tests, all PASS; suite `OK`.

- [ ] **Step 6: Commit**

```bash
git add lib/marble/marble.h lib/marble/marble.cpp test/test_marble/test_marble.cpp
git commit -m "feat(marble): pure core scaffold — PRNG + ball integrate + host tests"
```

---

## Task 2: `lib/marble` — `bounceWalls` (host TDD)

**Files:**
- Modify: `test/test_marble/test_marble.cpp` (add tests + register them)
- Modify: `lib/marble/marble.cpp` (add `bounceWalls`)

- [ ] **Step 1: Add the `bounceWalls` tests to `test/test_marble/test_marble.cpp`**

Insert directly after `test_integrate_damping_slows_ball`:

```cpp
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
```

Then register them in `runAllTests()` (add after the integrate lines):

```cpp
  RUN_TEST(test_bounce_right_wall_reflects_and_clamps);
  RUN_TEST(test_bounce_none_in_centre);
```

- [ ] **Step 2: Run the tests to verify the new ones FAIL**

Run:
```bash
pio test -e native -f test_marble
```
Expected: build/link FAILS — `undefined reference to marble::bounceWalls(...)`.

- [ ] **Step 3: Add `bounceWalls` to `lib/marble/marble.cpp`**

Insert inside `namespace marble { ... }`, after `integrate`:

```cpp
bool bounceWalls(Ball& b, const Config& cfg) {
  bool hit = false;
  const float r = cfg.ballR;
  if (b.pos.x < r)               { b.pos.x = r;               if (b.vel.x < 0) b.vel.x = -b.vel.x * cfg.restitution; hit = true; }
  if (b.pos.x > cfg.width - r)   { b.pos.x = cfg.width - r;   if (b.vel.x > 0) b.vel.x = -b.vel.x * cfg.restitution; hit = true; }
  if (b.pos.y < r)               { b.pos.y = r;               if (b.vel.y < 0) b.vel.y = -b.vel.y * cfg.restitution; hit = true; }
  if (b.pos.y > cfg.height - r)  { b.pos.y = cfg.height - r;  if (b.vel.y > 0) b.vel.y = -b.vel.y * cfg.restitution; hit = true; }
  return hit;
}
```

- [ ] **Step 4: Run the tests to verify all PASS**

Run:
```bash
pio test -e native -f test_marble
```
Expected: 5 tests, all PASS; suite `OK`.

- [ ] **Step 5: Commit**

```bash
git add lib/marble/marble.cpp test/test_marble/test_marble.cpp
git commit -m "feat(marble): wall bounce with restitution + host tests"
```

---

## Task 3: `lib/marble` — `eatDots` + spawn placement (host TDD)

**Files:**
- Modify: `test/test_marble/test_marble.cpp` (add tests + register them)
- Modify: `lib/marble/marble.cpp` (add the static `placeClear` helper + `eatDots`)

- [ ] **Step 1: Add the `eatDots` tests to `test/test_marble/test_marble.cpp`**

Insert after `test_bounce_none_in_centre`:

```cpp
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
```

Then register in `runAllTests()`:

```cpp
  RUN_TEST(test_eat_dot_scores_and_respawns_clear);
  RUN_TEST(test_eat_dot_far_away_no_score);
```

- [ ] **Step 2: Run the tests to verify the new ones FAIL**

Run:
```bash
pio test -e native -f test_marble
```
Expected: build/link FAILS — `undefined reference to marble::eatDots(...)`.

- [ ] **Step 3: Add the static `placeClear` helper and `eatDots` to `lib/marble/marble.cpp`**

Insert inside `namespace marble { ... }`, after `bounceWalls`. `placeClear` is file-local (not in the header) and must appear before `eatDots` uses it:

```cpp
// Pick a board position at least minDist from `avoid`, within the wall margins.
// Best-effort: gives up after a few tries and returns the last candidate.
static Vec2 placeClear(GameState& s, const Config& cfg, Vec2 avoid, float minDist) {
  Vec2 p{ cfg.width * 0.5f, cfg.height * 0.5f };
  for (int t = 0; t < 16; ++t) {
    p.x = rngRange(s.rng, cfg.ballR, cfg.width  - cfg.ballR);
    p.y = rngRange(s.rng, cfg.ballR, cfg.height - cfg.ballR);
    float dx = p.x - avoid.x, dy = p.y - avoid.y;
    if (dx*dx + dy*dy >= minDist*minDist) break;
  }
  return p;
}

int eatDots(GameState& s, const Config& cfg) {
  int eaten = 0;
  float reach = cfg.ballR + cfg.dotR;
  int n = cfg.numDots < MAX_DOTS ? cfg.numDots : MAX_DOTS;
  for (int i = 0; i < n; ++i) {
    if (!s.dots[i].active) continue;
    float dx = s.dots[i].pos.x - s.ball.pos.x;
    float dy = s.dots[i].pos.y - s.ball.pos.y;
    if (dx*dx + dy*dy <= reach*reach) {
      s.score += 1;
      s.dots[i].pos    = placeClear(s, cfg, s.ball.pos, cfg.ballR + cfg.dotR + 4.0f);
      s.dots[i].active = true;
      ++eaten;
    }
  }
  return eaten;
}
```

- [ ] **Step 4: Run the tests to verify all PASS**

Run:
```bash
pio test -e native -f test_marble
```
Expected: 7 tests, all PASS; suite `OK`.

- [ ] **Step 5: Commit**

```bash
git add lib/marble/marble.cpp test/test_marble/test_marble.cpp
git commit -m "feat(marble): dot eating + scoring + clear respawn + host tests"
```

---

## Task 4: `lib/marble` — `applyHoles` + `tickClock` (host TDD)

**Files:**
- Modify: `test/test_marble/test_marble.cpp` (add tests + register them)
- Modify: `lib/marble/marble.cpp` (add `applyHoles` + `tickClock`)

- [ ] **Step 1: Add the hole + clock tests to `test/test_marble/test_marble.cpp`**

Insert after `test_eat_dot_far_away_no_score`:

```cpp
void test_hole_penalty_resets_ball_and_flashes(void) {
  marble::Config cfg; cfg.numHoles = 1; cfg.holePenaltySec = 3.0f;
  marble::GameState s; s.timeLeft = 10.0f;
  s.ball.pos = {50.0f, 50.0f}; s.ball.vel = {20.0f, 20.0f};
  s.holes[0] = { {50.0f, 50.0f} };                 // ball centre inside the hole
  bool hit = marble::applyHoles(s, cfg);
  TEST_ASSERT_TRUE(hit);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.0f, s.timeLeft);              // -3 s
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cfg.width  * 0.5f, s.ball.pos.x);// reset to centre
  TEST_ASSERT_FLOAT_WITHIN(0.01f, cfg.height * 0.5f, s.ball.pos.y);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.ball.vel.x);           // velocity killed
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.ball.vel.y);
  TEST_ASSERT_TRUE(s.holeFlash > 0.0f);                           // flash armed
}

void test_hole_penalty_clamps_time_at_zero(void) {
  marble::Config cfg; cfg.numHoles = 1; cfg.holePenaltySec = 3.0f;
  marble::GameState s; s.timeLeft = 2.0f;
  s.ball.pos = {50.0f, 50.0f};
  s.holes[0] = { {50.0f, 50.0f} };
  marble::applyHoles(s, cfg);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.timeLeft);             // not negative
}

void test_tick_clock_counts_down_and_clamps(void) {
  marble::GameState s; s.timeLeft = 1.0f;
  marble::tickClock(s, 0.4f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.6f, s.timeLeft);
  marble::tickClock(s, 1.0f);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.timeLeft);            // clamps at 0
}
```

Then register in `runAllTests()`:

```cpp
  RUN_TEST(test_hole_penalty_resets_ball_and_flashes);
  RUN_TEST(test_hole_penalty_clamps_time_at_zero);
  RUN_TEST(test_tick_clock_counts_down_and_clamps);
```

- [ ] **Step 2: Run the tests to verify the new ones FAIL**

Run:
```bash
pio test -e native -f test_marble
```
Expected: build/link FAILS — `undefined reference to marble::applyHoles(...)` / `marble::tickClock(...)`.

- [ ] **Step 3: Add `applyHoles` + `tickClock` to `lib/marble/marble.cpp`**

Insert inside `namespace marble { ... }`, after `eatDots`:

```cpp
bool applyHoles(GameState& s, const Config& cfg) {
  float reach = cfg.holeR;  // ball centre must enter the hole
  int n = cfg.numHoles < MAX_HOLES ? cfg.numHoles : MAX_HOLES;
  for (int i = 0; i < n; ++i) {
    float dx = s.holes[i].pos.x - s.ball.pos.x;
    float dy = s.holes[i].pos.y - s.ball.pos.y;
    if (dx*dx + dy*dy <= reach*reach) {
      s.timeLeft -= cfg.holePenaltySec;
      if (s.timeLeft < 0.0f) s.timeLeft = 0.0f;
      s.ball.pos = { cfg.width * 0.5f, cfg.height * 0.5f };
      s.ball.vel = { 0.0f, 0.0f };
      s.holeFlash = cfg.holeFlashSec;
      return true;
    }
  }
  return false;
}

void tickClock(GameState& s, float dt) {
  s.timeLeft -= dt;
  if (s.timeLeft < 0.0f) s.timeLeft = 0.0f;
}
```

- [ ] **Step 4: Run the tests to verify all PASS**

Run:
```bash
pio test -e native -f test_marble
```
Expected: 10 tests, all PASS; suite `OK`.

- [ ] **Step 5: Commit**

```bash
git add lib/marble/marble.cpp test/test_marble/test_marble.cpp
git commit -m "feat(marble): hole time-penalty + ball reset + clock countdown + host tests"
```

---

## Task 5: `lib/marble` — `reset` + `step` composition (host TDD)

**Files:**
- Modify: `test/test_marble/test_marble.cpp` (add tests + register them)
- Modify: `lib/marble/marble.cpp` (add `reset` + `step`)

- [ ] **Step 1: Add the `reset` + `step` tests to `test/test_marble/test_marble.cpp`**

Insert after `test_tick_clock_counts_down_and_clamps`:

```cpp
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
```

Then register in `runAllTests()`:

```cpp
  RUN_TEST(test_reset_starts_playing_in_bounds);
  RUN_TEST(test_step_rolls_ball_and_ticks_clock);
  RUN_TEST(test_step_flips_to_game_over_at_zero);
  RUN_TEST(test_step_is_noop_after_game_over);
```

- [ ] **Step 2: Run the tests to verify the new ones FAIL**

Run:
```bash
pio test -e native -f test_marble
```
Expected: build/link FAILS — `undefined reference to marble::reset(...)` / `marble::step(...)`.

- [ ] **Step 3: Add `reset` + `step` to `lib/marble/marble.cpp`**

Insert inside `namespace marble { ... }`, after `tickClock` (the closing `}  // namespace marble` stays last):

```cpp
void reset(GameState& s, const Config& cfg, uint32_t seed) {
  s.rng       = seed ? seed : 1u;
  s.phase     = Phase::Playing;
  s.score     = 0;
  s.timeLeft  = cfg.roundSeconds;
  s.holeFlash = 0.0f;
  s.rollAngle = 0.0f;
  s.ball.pos  = { cfg.width * 0.5f, cfg.height * 0.5f };
  s.ball.vel  = { 0.0f, 0.0f };
  for (int i = 0; i < MAX_DOTS;  ++i) s.dots[i].active = false;
  // Holes well clear of the centre (where the ball spawns), then dots clear too.
  int nh = cfg.numHoles < MAX_HOLES ? cfg.numHoles : MAX_HOLES;
  for (int i = 0; i < nh; ++i)
    s.holes[i].pos = placeClear(s, cfg, s.ball.pos, cfg.holeR + cfg.ballR * 4.0f);
  int nd = cfg.numDots < MAX_DOTS ? cfg.numDots : MAX_DOTS;
  for (int i = 0; i < nd; ++i) {
    s.dots[i].pos    = placeClear(s, cfg, s.ball.pos, cfg.ballR + cfg.dotR + 4.0f);
    s.dots[i].active = true;
  }
}

void step(GameState& s, const Config& cfg, Vec2 tilt, float dt) {
  if (s.phase != Phase::Playing) return;
  integrate(s.ball, tilt, cfg, dt);
  bounceWalls(s.ball, cfg);
  eatDots(s, cfg);
  applyHoles(s, cfg);
  tickClock(s, dt);
  // Accumulate roll for the spin animation: angle = distance / radius.
  float speed = std::sqrt(s.ball.vel.x * s.ball.vel.x + s.ball.vel.y * s.ball.vel.y);
  if (cfg.ballR > 0.0f) s.rollAngle += speed * dt / cfg.ballR;
  if (s.holeFlash > 0.0f) { s.holeFlash -= dt; if (s.holeFlash < 0.0f) s.holeFlash = 0.0f; }
  if (s.timeLeft <= 0.0f) s.phase = Phase::GameOver;
}
```

- [ ] **Step 4: Run the tests to verify all PASS**

Run:
```bash
pio test -e native -f test_marble
```
Expected: 14 tests, all PASS; suite `OK`.

- [ ] **Step 5: Confirm the existing suites still pass (no regression)**

Run:
```bash
pio test -e native
```
Expected: `test_marble`, `test_audio`, and `test_weather` all report `OK`.

- [ ] **Step 6: Commit**

```bash
git add lib/marble/marble.cpp test/test_marble/test_marble.cpp
git commit -m "feat(marble): reset + step composition (full pure game core) + host tests"
```

---

## Task 6: Device rung v1 — IMU calibrate + rolling ball + walls (flash + observe)

Goal: isolate the riskiest unknown — the **IMU axis/sign mapping** — and the
physics feel, with **no dots, holes, or timer yet**.

**Files:**
- Modify: `platformio.ini` (add the env)
- Create: `src/marble_demo/imu_input.h`
- Create: `src/marble_demo/imu_input.cpp`
- Create: `src/marble_demo/board_view.h`
- Create: `src/marble_demo/board_view.cpp`
- Create: `src/marble_demo/app.h`
- Create: `src/marble_demo/app.cpp`
- Create: `src/marble_demo/main.cpp`

- [ ] **Step 1: Add the env to `platformio.ini`**

Append after the `[env:mic_speaker_demo]` block (and before `[env:native]`):

```ini
[env:marble_demo]
extends = device
build_src_filter = -<*> +<marble_demo/>
```

- [ ] **Step 2: Create `src/marble_demo/imu_input.h`**

```cpp
#pragma once
#include "marble.h"  // marble::Vec2

// Reads the StickS3 BMI270 accelerometer and turns it into a calibrated 2D tilt
// vector for the marble physics. "Level" is captured at calibrate() time, so any
// comfortable resting angle becomes the zero. The axis/sign mapping to the screen
// is CONFIRMED ON FIRST FLASH (see this task's observe step).
class ImuInput {
 public:
  void begin();          // check the IMU is running
  void calibrate();      // sample the resting accel as the new "level" zero
  marble::Vec2 read();   // calibrated tilt (g units; level => {0,0})
  bool ok() const { return ok_; }
 private:
  float ax0_ = 0.0f, ay0_ = 0.0f;  // captured "level" offsets
  bool  ok_  = false;
};
```

- [ ] **Step 3: Create `src/marble_demo/imu_input.cpp`**

```cpp
#include "imu_input.h"
#include <M5Unified.h>

// --- Axis/sign mapping (CONFIRM ON FIRST FLASH) -------------------------------
// Held flat, gravity sits on Z; tilting spills a component onto the in-plane
// axes. We map raw accel x -> screen x (right = +) and y -> screen y (down = +).
// If the ball rolls the WRONG way on first flash, flip the matching sign below;
// if the axes feel SWAPPED, swap which raw axis feeds X vs Y in read().
static constexpr float SCREEN_X_SIGN = 1.0f;
static constexpr float SCREEN_Y_SIGN = 1.0f;

void ImuInput::begin() {
  ok_ = M5.Imu.isEnabled();
  if (!ok_) Serial.println("ERROR: IMU not enabled");
}

void ImuInput::calibrate() {
  float sx = 0.0f, sy = 0.0f;
  int got = 0;
  for (int i = 0; i < 32; ++i) {
    float ax, ay, az;
    if (M5.Imu.getAccel(&ax, &ay, &az)) { sx += ax; sy += ay; ++got; }
    delay(5);
  }
  if (got > 0) { ax0_ = sx / got; ay0_ = sy / got; }
  Serial.printf("calibrated level: ax0=%.3f ay0=%.3f (n=%d)\n", ax0_, ay0_, got);
}

marble::Vec2 ImuInput::read() {
  float ax, ay, az;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) return marble::Vec2{0.0f, 0.0f};
  return marble::Vec2{ SCREEN_X_SIGN * (ax - ax0_), SCREEN_Y_SIGN * (ay - ay0_) };
}
```

- [ ] **Step 4: Create `src/marble_demo/board_view.h`**

```cpp
#pragma once
#include <M5Unified.h>
#include "marble.h"

// Flicker-free renderer: draws the whole frame into one off-screen M5Canvas and
// pushes it in a single shot (repo convention). Holds no game state. `best` is
// the best score this power-cycle, shown on the game-over overlay.
class BoardView {
 public:
  void begin();  // create the full-screen sprite
  void render(const marble::GameState& s, const marble::Config& cfg, int best);
 private:
  M5Canvas canvas_{&M5.Display};
};
```

- [ ] **Step 5: Create `src/marble_demo/board_view.cpp` (v1 — calibrate prompt, ball, walls, debug readout)**

```cpp
#include "board_view.h"

// The board area is drawn below a small HUD strip. These origins plus
// cfg.width/height must keep the board on-screen (240x135 landscape).
static constexpr int BOARD_X = 4;
static constexpr int BOARD_Y = 26;

void BoardView::begin() {
  canvas_.createSprite(M5.Display.width(), M5.Display.height());
}

void BoardView::render(const marble::GameState& s, const marble::Config& cfg, int best) {
  (void)best;  // unused until the game-over overlay (Task 7)
  canvas_.fillSprite(TFT_BLACK);

  if (s.phase == marble::Phase::Calibrate) {
    canvas_.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas_.setTextSize(2);
    canvas_.setCursor(8, 30);
    canvas_.print("Lay flat");
    canvas_.setTextSize(1);
    canvas_.setCursor(8, 60);
    canvas_.print("BtnA: start");
    canvas_.pushSprite(0, 0);
    return;
  }

  // Board frame.
  canvas_.drawRect(BOARD_X - 1, BOARD_Y - 1,
                   (int)cfg.width + 2, (int)cfg.height + 2, TFT_DARKGREY);

  // Ball + static specular highlight.
  int bx = BOARD_X + (int)s.ball.pos.x;
  int by = BOARD_Y + (int)s.ball.pos.y;
  canvas_.fillCircle(bx, by, (int)cfg.ballR, TFT_WHITE);
  canvas_.fillCircle(bx - 2, by - 2, 2, TFT_LIGHTGREY);

  // Debug readout (helps confirm the axis mapping on first flash): tilt the
  // right edge down -> vel.x and pos.x should grow, and the ball moves right.
  canvas_.setTextSize(1);
  canvas_.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas_.setCursor(4, 4);
  canvas_.printf("vel %4d,%4d", (int)s.ball.vel.x, (int)s.ball.vel.y);
  canvas_.setCursor(4, 14);
  canvas_.printf("pos %3d,%3d", (int)s.ball.pos.x, (int)s.ball.pos.y);

  canvas_.pushSprite(0, 0);
}
```

- [ ] **Step 6: Create `src/marble_demo/app.h`**

```cpp
#pragma once
#include "imu_input.h"
#include "board_view.h"
#include "marble.h"

// Non-blocking, millis()-paced state machine for the tilt marble game. Owns the
// IMU input, the renderer, and the pure game state. main.cpp just forwards
// setup()->begin() and loop()->loop().
class App {
 public:
  void begin();
  void loop();
 private:
  ImuInput          imu_;
  BoardView         view_;
  marble::Config    cfg_;
  marble::GameState state_;
  int               best_   = 0;
  uint32_t          lastMs_ = 0;
};
```

- [ ] **Step 7: Create `src/marble_demo/app.cpp` (v1 — no dots/holes; tilt + roll + walls)**

```cpp
#include "app.h"
#include <M5Unified.h>

void App::begin() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);   // 240x135 landscape
  view_.begin();
  imu_.begin();

  // Task 6 isolates the tilt/physics feel -- no dots, holes, or timer yet.
  cfg_.numDots  = 0;
  cfg_.numHoles = 0;

  state_.phase = marble::Phase::Calibrate;
  lastMs_ = millis();
}

void App::loop() {
  M5.update();
  uint32_t now = millis();
  float dt = (now - lastMs_) / 1000.0f;
  lastMs_ = now;
  if (dt > 0.05f) dt = 0.05f;  // clamp after a hiccup so the ball can't tunnel

  switch (state_.phase) {
    case marble::Phase::Calibrate:
      if (M5.BtnA.wasPressed()) {
        imu_.calibrate();
        marble::reset(state_, cfg_, micros());  // -> Phase::Playing
        Serial.println("PLAYING");
      }
      break;

    case marble::Phase::Playing: {
      marble::Vec2 tilt = imu_.read();
      marble::step(state_, cfg_, tilt, dt);
      if (M5.BtnB.wasPressed()) {                // re-zero "level" mid-play
        imu_.calibrate();
        Serial.println("recalibrated");
      }
      break;
    }

    case marble::Phase::GameOver:
      if (M5.BtnA.wasPressed()) {
        marble::reset(state_, cfg_, micros());
        Serial.println("PLAYING");
      }
      break;
  }

  view_.render(state_, cfg_, best_);
  delay(5);
}
```

- [ ] **Step 8: Create `src/marble_demo/main.cpp`**

```cpp
#include "app.h"

App app;

void setup() { app.begin(); }
void loop()  { app.loop(); }
```

- [ ] **Step 9: Build (no board needed)**

Run:
```bash
pio run -e marble_demo
```
Expected: `SUCCESS`. (Confirms `lib/marble` links into a device build and the file split compiles.)

- [ ] **Step 10: Flash and observe — CONFIRM THE AXIS MAPPING**

Run:
```bash
pio run -e marble_demo -t upload -t monitor
```
Expected and what to check:
- Boot shows `Lay flat` / `BtnA: start`. Lay the stick flat, screen up; press **BtnA** (KEY1). Serial prints the calibrated `ax0/ay0` and `PLAYING`.
- A white ball sits in the centre. **Tilt the right edge down → the ball should roll right** and the `vel`/`pos` readout grows in +x. Tilt away from you (top edge down) → ball rolls up the screen (`pos.y` toward 0... or toward the bottom — see below).
- **If the ball rolls the wrong direction** on an axis, edit `src/marble_demo/imu_input.cpp`: flip `SCREEN_X_SIGN` and/or `SCREEN_Y_SIGN` to `-1.0f`. **If tilting left/right moves the ball up/down instead**, the raw axes are swapped — in `ImuInput::read()` feed `ay` into the x slot and `ax` into the y slot. Re-flash and re-check until tilt direction matches ball direction.
- The ball should **bounce off all four walls** and keep rolling with momentum, slowing to a stop when level (damping). Press **BtnB** to re-zero level.

- [ ] **Step 11: Tune the feel, then re-flash if needed**

In `lib/marble/marble.h` `Config` defaults (these are the physics knobs):
- Ball too sluggish / too twitchy → raise/lower `gravity` (default 900).
- Rolls forever / stops too fast → lower/raise `damping` (default 1.5).
- Wall bounce too bouncy / too dead → lower/raise `restitution` (default 0.6).

Re-run Step 9–10 until it feels like a marble. Leave the chosen values in `Config`.

- [ ] **Step 12: Commit**

```bash
git add platformio.ini src/marble_demo/
git commit -m "feat(marble_demo): IMU tilt -> rolling ball with wall bounce (axis-mapping rung)"
```

---

## Task 7: Device rung v2 — dots, holes, timer, score, game over (flash + observe)

Bring the full game online: enable dots/holes, draw the HUD + entities + game-over overlay.

**Files:**
- Modify: `src/marble_demo/app.cpp` (replace whole file — enable counts, track best)
- Modify: `src/marble_demo/board_view.cpp` (replace whole file — HUD, holes, dots, overlay)

- [ ] **Step 1: Replace `src/marble_demo/app.cpp` with the full-game version**

```cpp
#include "app.h"
#include <M5Unified.h>

void App::begin() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);   // 240x135 landscape
  view_.begin();
  imu_.begin();

  // Full game: dots to collect, holes to dodge, on the 60 s clock (Config).
  cfg_.numDots  = 4;
  cfg_.numHoles = 3;

  state_.phase = marble::Phase::Calibrate;
  lastMs_ = millis();
}

void App::loop() {
  M5.update();
  uint32_t now = millis();
  float dt = (now - lastMs_) / 1000.0f;
  lastMs_ = now;
  if (dt > 0.05f) dt = 0.05f;  // clamp after a hiccup so the ball can't tunnel

  switch (state_.phase) {
    case marble::Phase::Calibrate:
      if (M5.BtnA.wasPressed()) {
        imu_.calibrate();
        marble::reset(state_, cfg_, micros());  // -> Phase::Playing
        Serial.println("PLAYING");
      }
      break;

    case marble::Phase::Playing: {
      marble::Vec2 tilt = imu_.read();
      marble::step(state_, cfg_, tilt, dt);
      if (state_.phase == marble::Phase::GameOver) {       // step flipped it
        if (state_.score > best_) best_ = state_.score;
        Serial.printf("GAME OVER score=%d best=%d\n", state_.score, best_);
      } else if (M5.BtnB.wasPressed()) {                   // re-zero "level"
        imu_.calibrate();
        Serial.println("recalibrated");
      }
      break;
    }

    case marble::Phase::GameOver:
      if (M5.BtnA.wasPressed()) {
        marble::reset(state_, cfg_, micros());
        Serial.println("PLAYING");
      }
      break;
  }

  view_.render(state_, cfg_, best_);
  delay(5);
}
```

- [ ] **Step 2: Replace `src/marble_demo/board_view.cpp` with the full-game renderer**

```cpp
#include "board_view.h"

// The board area is drawn below a small HUD strip. These origins plus
// cfg.width/height must keep the board on-screen (240x135 landscape).
static constexpr int BOARD_X = 4;
static constexpr int BOARD_Y = 26;

void BoardView::begin() {
  canvas_.createSprite(M5.Display.width(), M5.Display.height());
}

void BoardView::render(const marble::GameState& s, const marble::Config& cfg, int best) {
  canvas_.fillSprite(TFT_BLACK);

  if (s.phase == marble::Phase::Calibrate) {
    canvas_.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas_.setTextSize(2);
    canvas_.setCursor(8, 30);
    canvas_.print("Lay flat");
    canvas_.setTextSize(1);
    canvas_.setCursor(8, 60);
    canvas_.print("BtnA: start");
    canvas_.pushSprite(0, 0);
    return;
  }

  // HUD: time left (left) + score (right-ish).
  canvas_.setTextSize(2);
  canvas_.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas_.setCursor(4, 4);
  canvas_.printf("%4.1f", s.timeLeft);
  canvas_.setCursor(150, 4);
  canvas_.printf("S%d", s.score);

  // Board frame (flashes red briefly after a hole hit).
  uint16_t frame = s.holeFlash > 0.0f ? TFT_RED : TFT_DARKGREY;
  canvas_.drawRect(BOARD_X - 1, BOARD_Y - 1,
                   (int)cfg.width + 2, (int)cfg.height + 2, frame);

  // Holes: dark wells with a red ring.
  for (int i = 0; i < cfg.numHoles && i < marble::MAX_HOLES; ++i) {
    int hx = BOARD_X + (int)s.holes[i].pos.x;
    int hy = BOARD_Y + (int)s.holes[i].pos.y;
    canvas_.fillCircle(hx, hy, (int)cfg.holeR, TFT_BLACK);
    canvas_.drawCircle(hx, hy, (int)cfg.holeR, TFT_RED);
  }

  // Dots: bright pickups.
  for (int i = 0; i < cfg.numDots && i < marble::MAX_DOTS; ++i) {
    if (!s.dots[i].active) continue;
    canvas_.fillCircle(BOARD_X + (int)s.dots[i].pos.x,
                       BOARD_Y + (int)s.dots[i].pos.y, (int)cfg.dotR, TFT_GREENYELLOW);
  }

  // Ball + static specular highlight.
  int bx = BOARD_X + (int)s.ball.pos.x;
  int by = BOARD_Y + (int)s.ball.pos.y;
  canvas_.fillCircle(bx, by, (int)cfg.ballR, TFT_WHITE);
  canvas_.fillCircle(bx - 2, by - 2, 2, TFT_LIGHTGREY);

  // Game-over overlay.
  if (s.phase == marble::Phase::GameOver) {
    canvas_.setTextSize(2);
    canvas_.setTextColor(TFT_RED, TFT_BLACK);
    canvas_.setCursor(40, 36);
    canvas_.print("TIME!");
    canvas_.setTextSize(1);
    canvas_.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas_.setCursor(40, 62);
    canvas_.printf("score %d", s.score);
    canvas_.setCursor(40, 74);
    canvas_.printf("best  %d", best);
    canvas_.setCursor(40, 86);
    canvas_.print("BtnA: again");
  }

  canvas_.pushSprite(0, 0);
}
```

- [ ] **Step 3: Build**

Run:
```bash
pio run -e marble_demo
```
Expected: `SUCCESS`.

- [ ] **Step 4: Flash and observe the full game**

Run:
```bash
pio run -e marble_demo -t upload -t monitor
```
Expected:
- `Lay flat` → BtnA → the board shows the HUD (`60.0` counting down, `S0`), a few green dots, and dark red-ringed holes.
- Tilt to roll the ball over a **dot** → `S` increments and the dot reappears elsewhere.
- Roll the ball into a **hole** → the frame flashes red, the time drops by 3 s, and the ball resets to centre. Serial is quiet here (penalty handled in `lib/marble`).
- When the clock reaches `0.0` → `TIME!` overlay with `score`/`best`; serial prints `GAME OVER score=… best=…`. Press **BtnA** → a fresh round; `best` persists until power-off.

- [ ] **Step 5: Tune gameplay, then re-flash if needed**

In `lib/marble/marble.h` `Config`: adjust `numDots` (4), `numHoles` (3), `holePenaltySec` (3), `roundSeconds` (60), `dotR`/`holeR` for difficulty/readability. Re-run Step 3–4 until it plays well. Leave chosen values in `Config`.

- [ ] **Step 6: Commit**

```bash
git add src/marble_demo/app.cpp src/marble_demo/board_view.cpp
git commit -m "feat(marble_demo): dots, holes, timer, score, game over + restart"
```

---

## Task 8: Stretch — procedural rolling spin + final polish (flash + observe)

**Files:**
- Modify: `src/marble_demo/board_view.cpp` (the ball-drawing block only)

- [ ] **Step 1: Add the `<cmath>` include to `src/marble_demo/board_view.cpp`**

Change the top of the file from:

```cpp
#include "board_view.h"
```
to:
```cpp
#include "board_view.h"
#include <cmath>  // sinf/cosf for the rolling-spin animation
```

- [ ] **Step 2: Replace the ball-drawing block in `BoardView::render` with the spinning version**

Find this block (added in Task 7):

```cpp
  // Ball + static specular highlight.
  int bx = BOARD_X + (int)s.ball.pos.x;
  int by = BOARD_Y + (int)s.ball.pos.y;
  canvas_.fillCircle(bx, by, (int)cfg.ballR, TFT_WHITE);
  canvas_.fillCircle(bx - 2, by - 2, 2, TFT_LIGHTGREY);
```

and replace it with:

```cpp
  // Ball with procedural spin: a fixed specular highlight (light from top-left)
  // plus a surface spot that orbits at the accumulated rollAngle, so the ball
  // visibly rotates faster the faster it moves.
  int bx = BOARD_X + (int)s.ball.pos.x;
  int by = BOARD_Y + (int)s.ball.pos.y;
  canvas_.fillCircle(bx, by, (int)cfg.ballR, TFT_WHITE);
  float orbit = cfg.ballR * 0.55f;
  int spx = bx + (int)(orbit * cosf(s.rollAngle));
  int spy = by + (int)(orbit * sinf(s.rollAngle));
  canvas_.fillCircle(spx, spy, 2, TFT_NAVY);          // rolling surface spot
  canvas_.fillCircle(bx - 2, by - 2, 2, TFT_LIGHTGREY); // fixed highlight
```

- [ ] **Step 3: Build**

Run:
```bash
pio run -e marble_demo
```
Expected: `SUCCESS`.

- [ ] **Step 4: Flash and observe the spin**

Run:
```bash
pio run -e marble_demo -t upload -t monitor
```
Expected: the dark spot on the ball spins around — fast while the ball races, slowing as it coasts to a stop — reading as a rolling marble rather than a sliding dot. Gameplay is unchanged from Task 7.

If the spin looks too subtle, bump the spot radius (`2` → `3`) or `orbit` factor (`0.55f` → `0.65f`); if too busy, shrink them. Leave the chosen values in place.

- [ ] **Step 5: Final regression — host tests still green**

Run:
```bash
pio test -e native
```
Expected: `test_marble` + `test_audio` + `test_weather` all `OK` (the device edits didn't touch `lib/marble`).

- [ ] **Step 6: Commit**

```bash
git add src/marble_demo/board_view.cpp
git commit -m "feat(marble_demo): procedural rolling-spin animation (stretch goal)"
```

---

## Wrap-up & integration

- [ ] **Record hardware findings** — append a short `## Findings` section to the design doc (`docs/2026-05-31-sticky-marble-demo-design.md`) and commit:
  - The confirmed BMI270 **axis → screen mapping and signs** (the final `SCREEN_X_SIGN`/`SCREEN_Y_SIGN` and whether the raw axes were swapped in `read()`).
  - Confirmed `BtnA`/`BtnB` → physical KEY1/KEY2 mapping.
  - The tuned `gravity` / `damping` / `restitution` and the `numDots`/`numHoles`/`holePenaltySec` that felt right.
  - Any surprises (IMU rest orientation, drift requiring frequent BtnB re-calibrate).

```bash
git add docs/2026-05-31-sticky-marble-demo-design.md
git commit -m "docs(marble_demo): record hardware findings"
```

- [ ] **Decide branch integration** via `superpowers:finishing-a-development-branch` — open a PR for branch `marble-demo` (emoji title + body per the user's PR style), or keep it local.

---

## Self-Review notes (author)

- **Spec coverage:** pure core in `lib/marble` host-tested (Tasks 1–5: PRNG, integrate, bounceWalls, eatDots, applyHoles, tickClock, reset, step — design §3, §4, §9); flat-tray tilt with boot calibration + mid-play re-zero (Task 6, `ImuInput`; design §1, §5); momentum + damped wall bounce (Tasks 1–2, 6; design §3); dots collect+respawn, holes time-penalty+reset, 60 s clock, game over + restart, HUD (Tasks 4, 7; design §5, §6, §7); IMU axis-mapping confirmed on flash (Task 6 Step 10; design §1, §11); `[env:marble_demo]` + no native-env change (Task 6 Step 1; design §3); procedural rolling spin (Task 8; design §7 stretch). All design §3–§11 items map to a task. ✅
- **Placeholder scan:** no TBD/TODO; every code step shows complete content; verification steps give exact commands + expected screen/serial output. ✅
- **Type consistency:** `marble::Vec2/Ball/Dot/Hole/Phase/Config/GameState` and the free functions `rngNext/rngRange/integrate/bounceWalls/eatDots/applyHoles/tickClock/reset/step` are declared once in Task 1's header and used with identical signatures in Tasks 2–8. Device classes `ImuInput` (`begin/calibrate/read/ok`), `BoardView` (`begin/render(state,cfg,best)` — the 3-arg signature is fixed from Task 6 so Task 7's overlay needs no signature change), and `App` (`begin/loop`) are stable from introduction. The static `placeClear` is file-local and defined before its first use. ✅
- **Refinement vs spec:** `Config.damping` (per-second exponential decay) replaces the design's per-step `friction` to remove frame-rate dependence; hole trigger uses ball-centre-inside-`holeR` (not `ballR+holeR`) for a forgiving "fall in" feel; `best` score is RAM-only (design §6). The design doc is updated to match (`damping`, §3/§4/§6). ✅
- **Hardware verification:** every device task ends in flash + observe with concrete expected results and an explicit axis-mapping correction procedure, per the project's "observe the LCD/serial" model. ✅
- **Granularity:** lib tasks are strict red→green→commit TDD cycles; device tasks are incremental flashes (roll/bounce → full game → spin) mirroring `mic_speaker_demo`'s VU→tone→record progression. ✅
