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
