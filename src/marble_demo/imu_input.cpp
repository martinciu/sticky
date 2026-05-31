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
