#pragma once
#include <M5Unified.h>
#include "audio.h"

// All mic/speaker hardware for the demo: the shared-ES8311 codec toggle, the
// runtime-sized record buffer, chunked capture, playback, and a VU level read.
// Keeping it here keeps the hardware specifics out of the state machine (app.h).
//
// The central StickS3 fact lives in enterMic()/enterSpeaker(): the mic and the
// speaker share one ES8311 codec, so only one can be active at a time.
class AudioIo {
 public:
  // Allocate the record buffer (sized to free internal DMA RAM) and start the
  // mic for the live VU. Must run after M5.begin(). Returns false if the buffer
  // couldn't be allocated -- VU + tone still work, record/playback is disabled.
  bool begin();

  bool   bufferReady() const { return recBufOk_; }
  size_t maxSamples()  const { return recSamples_; }
  float  maxSeconds()  const { return recSeconds_; }

  // Record one VU block from the mic and return its level as a 0..100 bar.
  // Retains the previous level on a tick with no fresh block.
  uint8_t readVuLevel();

  // --- push-to-talk capture (the mic stays enabled throughout) ---
  void   startCapture();                       // zero the buffer + reset position
  void   pumpCapture();                         // keep up to 2 chunks queued (gapless)
  bool   captureFull()     const { return recPos_ + REC_CHUNK > recSamples_; }
  size_t capturedSamples() const { return recPos_; }
  float  capturedSeconds() const { return (float)recPos_ / SAMPLE_RATE; }

  // --- playback / tone (toggle to the speaker, caller returns via backToMic) ---
  void playCaptured();                          // enterSpeaker + play recPos_ samples
  void playTone(uint32_t freq, uint32_t ms);    // enterSpeaker + tone
  bool isPlaying() { return M5.Speaker.isPlaying(); }
  void backToMic() { enterMic(); }              // after playback/tone finishes

 private:
  // Mic and speaker share the one ES8311 codec, so only one can be active at a
  // time. Always end the other before begin-ing it.
  void enterMic()     { M5.Speaker.end(); M5.Mic.begin(); }
  void enterSpeaker() { M5.Mic.end();     M5.Speaker.begin(); }

  static constexpr uint32_t SAMPLE_RATE = 16000;  // 16 kHz mono
  static constexpr size_t   VU_BLOCK    = 256;    // ~16 ms per VU read
  // Small capture chunks: the mic task fills a record request to completion
  // before re-checking its stop flag, so a single multi-second request would
  // make Mic.end() block for the whole thing. ~64 ms keeps release responsive;
  // we keep 2 queued so capture stays gapless.
  static constexpr size_t   REC_CHUNK       = 1024;
  static constexpr float    MAX_REC_SECONDS = 20.0f;       // upper bound on clip length
  static constexpr size_t   HEAP_MARGIN     = 96 * 1024;   // headroom for stack/runtime
  static constexpr uint8_t  SPK_VOLUME      = 255;         // max (0..255)

  int16_t* recBuf_     = nullptr;   // capture buffer (internal DMA RAM)
  size_t   recSamples_ = 0;         // capacity in samples
  size_t   recPos_     = 0;         // samples captured/queued so far
  float    recSeconds_ = 0.0f;      // recSamples_ / SAMPLE_RATE
  bool     recBufOk_   = false;
  uint8_t  lastBar_    = 0;         // retained VU level
  int16_t  vuBlock_[VU_BLOCK];
};
