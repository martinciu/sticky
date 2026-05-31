#pragma once
#include "audio_io.h"
#include "vu_view.h"

// The demo's state machine. Owns the audio I/O and the renderer; main.cpp's
// setup()/loop() just forward to begin()/loop().
//
//   IDLE_VU --(hold BtnA)--> RECORDING --(release / full)--> PLAYBACK --> IDLE_VU
//      \--(BtnB)--> TONE --> IDLE_VU
class App {
 public:
  void begin();   // setup(): bring up board, view, audio
  void loop();    // one tick: sample inputs, dispatch on state

 private:
  enum class State { Idle, Recording, Playback, Tone };

  void runIdle();
  void runRecording();
  void runPlayback();
  void runTone();
  void transitionTo(State s);
  Frame frameFor(const char* status, uint16_t color, int progressPct) const;

  AudioIo  audio_;
  VuView   view_;
  State    state_      = State::Idle;
  uint8_t  bar_        = 0;     // current VU level 0..100
  uint8_t  peakHold_   = 0;     // decaying peak marker
  uint32_t recStart_   = 0;     // millis() at record start
  float    recElapsed_ = 0.0f;  // seconds held (push-to-talk readout)
  size_t   toneIdx_    = 0;     // which tone in the C/E/G cycle
};
