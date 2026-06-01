#include "app.h"
#include <M5Unified.h>

void App::begin() {
  input_.begin();              // platform init: M5.begin (Stick) / M5Cardputer.begin (Cardputer)
  Serial.begin(115200);
  M5.Display.setRotation(1);   // 240x135 landscape (both boards are 240x135)
  view_.begin();
  imu_.begin();
  sound_.begin();
  setTrack(Sound::Track::Menu, millis());  // start-screen music

  // Full game: dots to collect, holes to dodge, on the 60 s clock (Config).
  cfg_.numDots  = 4;
  cfg_.numHoles = 3;

  state_.phase = marble::Phase::Calibrate;
  lastMs_ = millis();
}

// Record the track the game wants playing, and start it only if music is enabled.
// Keeps the BtnB on/off toggle authoritative across phase transitions.
void App::setTrack(Sound::Track t, uint32_t now) {
  desiredTrack_ = t;
  if (musicOn_) sound_.setMusic(t, now);
}

void App::loop() {
  input_.update();   // refresh inputs (the Cardputer also rescans its keyboard)
  uint32_t now = millis();
  float dt = (now - lastMs_) / 1000.0f;
  lastMs_ = now;
  if (dt > 0.05f) dt = 0.05f;  // clamp after a hiccup so the ball can't tunnel

  // Read tilt once per frame: it drives the physics AND the hole parallax.
  marble::Vec2 view = imu_.read();

  // Poll the battery ~1 Hz so the on-screen readout works on battery power too
  // (there is no serial monitor once USB is unplugged).
  if (lastBattMs_ == 0 || now - lastBattMs_ >= 1000) {
    lastBattMs_ = now;
    batt_ = M5.Power.getBatteryLevel();
    charging_ =
        (M5.Power.isCharging() == m5::Power_Class::is_charging_t::is_charging);
  }

  // Toggle the background music (SFX keep playing): BtnB on the Stick, Esc on the Cardputer.
  if (input_.musicToggled()) {
    musicOn_ = !musicOn_;
    sound_.setMusic(musicOn_ ? desiredTrack_ : Sound::Track::None, now);
    Serial.printf("music %s\n", musicOn_ ? "ON" : "OFF");
  }

  switch (state_.phase) {
    case marble::Phase::Calibrate:
      if (input_.startPressed()) {
        imu_.calibrate();
        marble::reset(state_, cfg_, micros());  // -> Phase::Playing
        sound_.sfxStart(now);
        setTrack(Sound::Track::Game, now);
        Serial.println("PLAYING");
      }
      break;

    case marble::Phase::Playing: {
      int  prevScore  = state_.score;
      bool wasFalling = state_.fallTimer > 0.0f;
      marble::step(state_, cfg_, view, dt);
      if (state_.score > prevScore) sound_.sfxEat(now);                // ate a dot
      if (!wasFalling && state_.fallTimer > 0.0f) sound_.sfxFall(now); // fell in a hole
      if (state_.bounced && !wasBounced_) sound_.sfxBounce();          // wall impact (edge)
      wasBounced_ = state_.bounced;
      if (state_.phase == marble::Phase::GameOver) {       // step flipped it
        if (state_.score > best_) best_ = state_.score;
        sound_.sfxGameOver(now);
        setTrack(Sound::Track::Menu, now);
        Serial.printf("GAME OVER score=%d best=%d\n", state_.score, best_);
      } else if (input_.recalibrate()) {                  // re-zero "level" (Stick: side button; Cardputer: C)
        imu_.calibrate();
        Serial.println("recalibrated");
      }
      break;
    }

    case marble::Phase::GameOver:
      if (input_.startPressed()) {
        marble::reset(state_, cfg_, micros());
        sound_.sfxStart(now);
        setTrack(Sound::Track::Game, now);
        Serial.println("PLAYING");
      }
      break;
  }

  sound_.update(now);  // advance the non-blocking music + SFX sequencers
  view_.render(state_, cfg_, best_, view, batt_, charging_);
  delay(5);
}
