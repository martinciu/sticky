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
  int               batt_       = -1;    // battery %, -1 if unknown
  bool              charging_   = false;
  uint32_t          lastBattMs_ = 0;
};
