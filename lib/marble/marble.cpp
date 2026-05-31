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

bool bounceWalls(Ball& b, const Config& cfg) {
  bool hit = false;
  const float r = cfg.ballR;
  if (b.pos.x < r)               { b.pos.x = r;               if (b.vel.x < 0) b.vel.x = -b.vel.x * cfg.restitution; hit = true; }
  if (b.pos.x > cfg.width - r)   { b.pos.x = cfg.width - r;   if (b.vel.x > 0) b.vel.x = -b.vel.x * cfg.restitution; hit = true; }
  if (b.pos.y < r)               { b.pos.y = r;               if (b.vel.y < 0) b.vel.y = -b.vel.y * cfg.restitution; hit = true; }
  if (b.pos.y > cfg.height - r)  { b.pos.y = cfg.height - r;  if (b.vel.y > 0) b.vel.y = -b.vel.y * cfg.restitution; hit = true; }
  return hit;
}

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

// Place a dot clear of the ball AND every hole, so a spawned dot is always
// reachable without being swallowed. Falls back to the centre (always hole-free,
// since reset() keeps holes clear of the spawn centre) if no spot is found.
static Vec2 placeDot(GameState& s, const Config& cfg) {
  float minBall = cfg.ballR + cfg.dotR + 4.0f;
  float minHole = cfg.holeR + cfg.ballR + cfg.dotR;
  int nh = cfg.numHoles < MAX_HOLES ? cfg.numHoles : MAX_HOLES;
  for (int t = 0; t < 24; ++t) {
    Vec2 p{ rngRange(s.rng, cfg.ballR, cfg.width  - cfg.ballR),
            rngRange(s.rng, cfg.ballR, cfg.height - cfg.ballR) };
    float dxb = p.x - s.ball.pos.x, dyb = p.y - s.ball.pos.y;
    if (dxb*dxb + dyb*dyb < minBall*minBall) continue;
    bool clear = true;
    for (int i = 0; i < nh; ++i) {
      float dx = p.x - s.holes[i].pos.x, dy = p.y - s.holes[i].pos.y;
      if (dx*dx + dy*dy < minHole*minHole) { clear = false; break; }
    }
    if (clear) return p;
  }
  return Vec2{ cfg.width * 0.5f, cfg.height * 0.5f };
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
      s.dots[i].pos    = placeDot(s, cfg);
      s.dots[i].active = true;
      ++eaten;
    }
  }
  return eaten;
}

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
    s.dots[i].pos    = placeDot(s, cfg);
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

}  // namespace marble
