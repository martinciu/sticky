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
