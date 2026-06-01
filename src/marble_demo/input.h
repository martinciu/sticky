#pragma once

// Edge-triggered game controls, abstracting the two boards' very different
// inputs: the StickS3 has BtnA / BtnB + a clickable power button; the M5Stack
// Cardputer Adv has only a 56-key keyboard. update() runs once per frame and
// latches this frame's freshly-actuated controls; the accessors report them.
// The board is selected at build time by -DCARDPUTER (set in the
// marble_cardputer PlatformIO env); everything else in the game is shared.
class Input {
 public:
  void begin();   // platform init: M5.begin (Stick) / M5Cardputer.begin (Cardputer)
  void update();  // refresh hardware + latch this frame's control edges

  bool startPressed() const { return start_; }  // begin / restart a game
  bool musicToggled() const { return music_; }  // toggle the background music
  bool recalibrate()  const { return recal_; }  // re-zero "level"

 private:
  bool start_ = false, music_ = false, recal_ = false;
};

// On-screen control hints shown on the start / game-over UI (board-specific).
#ifdef CARDPUTER
#define MARBLE_BTN_START "ENT:START"
#define MARBLE_BTN_AGAIN "ENT:AGAIN"
#else
#define MARBLE_BTN_START "BTNA:START"
#define MARBLE_BTN_AGAIN "BTNA:AGAIN"
#endif
