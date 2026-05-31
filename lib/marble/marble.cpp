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

}  // namespace marble
