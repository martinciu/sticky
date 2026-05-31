#include "app.h"

namespace {
const uint32_t kToneFreqs[] = {523, 659, 784};  // C5 E5 G5
}

void App::begin() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);
  view_.begin();
  audio_.begin();  // allocates buffer, starts mic, prints boot diagnostic
}

void App::loop() {
  M5.update();
  switch (state_) {
    case State::Idle:      runIdle();      break;
    case State::Recording: runRecording(); break;
    case State::Playback:  runPlayback();  break;
    case State::Tone:      runTone();      break;
  }
  delay(5);  // pace the loop + yield to FreeRTOS
}

Frame App::frameFor(const char* status, uint16_t color, int progressPct) const {
  return Frame{status, color, progressPct, bar_, peakHold_,
               recElapsed_, audio_.maxSeconds(), audio_.bufferReady()};
}

void App::runIdle() {
  bar_ = audio_.readVuLevel();
  if (bar_ > peakHold_) peakHold_ = bar_;
  if (peakHold_ > 0) peakHold_ -= 1;  // decay the peak marker
  view_.render(frameFor("VU", TFT_GREEN, -1));

  if (audio_.bufferReady() && M5.BtnA.wasPressed()) {
    audio_.startCapture();
    recStart_ = millis();
    recElapsed_ = 0.0f;
    transitionTo(State::Recording);
  } else if (M5.BtnB.wasPressed()) {
    audio_.playTone(kToneFreqs[toneIdx_], 200);
    toneIdx_ = (toneIdx_ + 1) % 3;
    transitionTo(State::Tone);
  }
}

void App::runRecording() {
  audio_.pumpCapture();
  recElapsed_ = (millis() - recStart_) / 1000.0f;
  int pct = audio_.maxSamples()
                ? (int)(audio_.capturedSamples() * 100 / audio_.maxSamples())
                : 0;
  if (pct > 100) pct = 100;
  view_.render(frameFor("REC", TFT_RED, pct));

  // Stop on release (button level, not the one-shot edge) or when the buffer
  // has no room for another chunk.
  if (!M5.BtnA.isPressed() || audio_.captureFull()) {
    if (audio_.capturedSamples() > 0) {
      Serial.printf("PLAYBACK %.2fs\n", audio_.capturedSeconds());
      audio_.playCaptured();
      transitionTo(State::Playback);
    } else {
      transitionTo(State::Idle);  // empty take; mic is still running
    }
  }
}

void App::runPlayback() {
  view_.render(frameFor("PLAY", TFT_YELLOW, -1));
  if (!audio_.isPlaying()) {
    audio_.backToMic();
    transitionTo(State::Idle);
  }
}

void App::runTone() {
  view_.render(frameFor("TONE", TFT_MAGENTA, -1));
  if (!audio_.isPlaying()) {
    audio_.backToMic();
    transitionTo(State::Idle);
  }
}

void App::transitionTo(State s) {
  state_ = s;
  const char* name = s == State::Idle      ? "IDLE_VU"
                   : s == State::Recording ? "RECORDING"
                   : s == State::Playback  ? "PLAYBACK"
                                           : "TONE";
  Serial.println(name);
}
