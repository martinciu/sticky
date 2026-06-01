#include "input.h"

#ifdef CARDPUTER
// ---- M5Stack Cardputer Adv: 56-key keyboard (no A/B/PWR buttons) -------------
#include <M5Cardputer.h>

void Input::begin() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);  // also runs M5.begin(cfg) + starts the keyboard
}

void Input::update() {
  M5Cardputer.update();          // refreshes M5 + rescans the keyboard
  start_ = music_ = recal_ = false;
  // isChange() + isPressed() => fire once on the press-down edge (a held key
  // doesn't repeat), matching BtnX.wasPressed() semantics on the Stick.
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    auto& st = M5Cardputer.Keyboard.keysState();
    if (st.enter) start_ = true;                    // Enter: start / restart
    for (char c : st.word) {
      // The Cardputer keyboard has no dedicated Esc key; its top-left key (where
      // Esc physically sits) is the backtick/tilde key, so that IS the "Esc".
      if (c == '`' || c == '~') music_ = true;      // Esc (top-left ` ~) key: toggle music
      if (c == 'c' || c == 'C') recal_ = true;      // C: re-zero "level"
    }
  }
}

#else
// ---- M5StickC S3: BtnA / BtnB + clickable side power button ------------------
#include <M5Unified.h>

void Input::begin() {
  auto cfg = M5.config();
  M5.begin(cfg);
}

void Input::update() {
  M5.update();
  start_ = M5.BtnA.wasPressed();    // BtnA: start / restart
  music_ = M5.BtnB.wasPressed();    // BtnB: toggle music
  recal_ = M5.BtnPWR.wasClicked();  // side power button: re-zero "level"
}
#endif
