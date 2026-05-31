#pragma once
#include <M5Unified.h>

// One frame's worth of UI values. Pure data passed from the state machine to
// the renderer -- VuView holds no app/model state of its own.
struct Frame {
  const char* status;        // "VU" / "REC" / "PLAY" / "TONE"
  uint16_t    color;         // status-label color
  int         progressPct;   // <0: live VU (bar + peak tick); >=0: REC progress bar
  uint8_t     bar;           // 0..100 level (drawn when progressPct < 0)
  uint8_t     peakHold;      // 0..100 peak marker (drawn when progressPct < 0)
  float       recElapsed;    // seconds held (REC readout)
  float       recSeconds;    // max clip length (REC readout)
  bool        recAvailable;  // false => record disabled (buffer alloc failed)
};

// Renders a Frame into an off-screen sprite, then pushes it (flicker-free).
class VuView {
 public:
  void begin();              // allocate the full-screen sprite (after M5.begin)
  void render(const Frame& f);

 private:
  M5Canvas canvas_{&M5.Display};
};
