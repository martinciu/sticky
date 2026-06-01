#pragma once
#include "imu_input.h"
#include "board_view.h"
#include "marble.h"
#include "sound.h"
#include "input.h"

// Non-blocking, millis()-paced state machine for the tilt marble game. Owns the
// IMU input, the renderer, and the pure game state. main.cpp just forwards
// setup()->begin() and loop()->loop().
class App {
 public:
  void begin();
  void loop();
 private:
  Input             input_;   // board-specific controls: buttons (Stick) / keyboard (Cardputer)
  ImuInput          imu_;
  BoardView         view_;
  Sound             sound_;
  marble::Config    cfg_;
  marble::GameState state_;
  int               best_   = 0;
  uint32_t          lastMs_ = 0;
  int               batt_       = -1;    // battery %, -1 if unknown
  bool              charging_   = false;
  uint32_t          lastBattMs_ = 0;
  bool              wasBounced_ = false;  // edge-detect wall bounces for the SFX
  bool              musicOn_    = true;   // music toggle: BtnB on the Stick, Esc key on the Cardputer
  Sound::Track      desiredTrack_ = Sound::Track::Menu;  // track to play while music is on
  void setTrack(Sound::Track t, uint32_t now);  // record desired track; play it if music is on
};
