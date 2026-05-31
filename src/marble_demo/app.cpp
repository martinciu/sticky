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
