#pragma once
// Pure tilt-marble physics + game logic. Depends ONLY on the C++ standard
// library -- no Arduino/M5/WiFi headers -- so it compiles and unit-tests on the
// native (host) environment, no hardware required. The device-only IMU read,
// rendering, and state machine live in src/marble_demo/ and call into this.
#include <cstddef>
#include <cstdint>

namespace marble {

// Explicit constructors so `Vec2{x, y}` works under the device toolchain's
// older C++ mode too (a struct with default member initializers is NOT an
// aggregate before C++14, so brace-init would otherwise need a constructor).
struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
  Vec2() = default;
  Vec2(float x_, float y_) : x(x_), y(y_) {}
};

struct Ball {
  Vec2 pos;
  Vec2 vel;
};
struct Dot {
  Vec2 pos;
  bool active = false;
};
struct Hole {
  Vec2 pos;
};

enum class Phase : uint8_t { Calibrate, Playing, GameOver };

constexpr int MAX_DOTS = 8;
constexpr int MAX_HOLES = 8;

// All tunables. Lengths/positions are in board pixels (the play area, not the
// full 240x135 screen). The device picks width/height to match the drawn board.
struct Config {
  float width = 232.0f;  // board area width  (px)
  float height = 104.0f; // board area height (px)
  float ballR = 7.0f;
  float dotR = 3.0f;
  float holeR = 12.0f;
  float gravity = 900.0f; // px/s^2 per g of tilt
  float damping = 1.5f;   // velocity decay per second (frame-rate independent)
  float restitution = 0.6f; // wall-bounce energy retained (0..1)
  float roundSeconds = 60.0f;
  float holePenaltySec = 3.0f;
  float holeFlashSec = 0.4f; // how long the "just hit a hole" flash lingers
  int numDots = 4;
  int numHoles = 3;
};

struct GameState {
  Phase phase = Phase::Calibrate;
  Ball ball;
  Dot dots[MAX_DOTS];
  Hole holes[MAX_HOLES];
  int score = 0;
  float timeLeft = 0.0f;
  float holeFlash = 0.0f; // >0 == just hit a hole (render hint), seconds
  float rollAngle = 0.0f; // accumulated roll for the spin animation (radians)
  uint32_t rng = 1u;      // xorshift32 state (deterministic spawns)
};

// --- pure helpers (each independently testable) ---

// xorshift32: advance state, return next value. Treats a 0 state as 1.
uint32_t rngNext(uint32_t &state);
// Uniform float in [lo, hi) drawn from the PRNG.
float rngRange(uint32_t &state, float lo, float hi);

// Integrate one ball for dt seconds: gravity from tilt, exponential velocity
// damping (frame-rate independent), then move by velocity.
void integrate(Ball &ball, Vec2 tilt, const Config &cfg, float dt);

// Reflect the ball off the 4 walls, clamp it inside, scale the bounced velocity
// component by restitution. Returns true if any wall was hit.
bool bounceWalls(Ball &ball, const Config &cfg);

// Eat every overlapping active dot: score++ and respawn it clear of the ball.
// Returns how many were eaten this call.
int eatDots(GameState &s, const Config &cfg);

// If the ball's centre is inside any hole: subtract the time penalty (clamped
// >= 0), reset the ball to centre with zero velocity, set holeFlash. Returns
// true if a hole was hit.
bool applyHoles(GameState &s, const Config &cfg);

// Count the round clock down by dt; clamp at 0.
void tickClock(GameState &s, float dt);

// Start a fresh round: seed the PRNG, centre the ball, place numHoles holes
// (clear of the centre) then numDots dots (clear of the centre), reset score
// and timeLeft, set phase = Playing.
void reset(GameState &s, const Config &cfg, uint32_t seed);

// Advance one frame when Playing: integrate + bounceWalls + eatDots +
// applyHoles
// + tickClock + roll/flash decay, then flip to GameOver at timeLeft 0. No-op in
// any other phase.
void step(GameState &s, const Config &cfg, Vec2 tilt, float dt);

} // namespace marble
