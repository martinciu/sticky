#include "audio_io.h"
#include <cstring>  // memset

bool AudioIo::begin() {
  M5.Speaker.setVolume(SPK_VOLUME);

  // Size the record buffer to free internal DMA RAM minus a safety margin,
  // capped at MAX_REC_SECONDS. The I2S DMA needs INTERNAL RAM (MALLOC_CAP_8BIT) --
  // not PSRAM, which isn't reliably DMA-accessible on the S3 and isn't enabled in
  // this build -- so free internal SRAM is the ceiling. The arithmetic is the
  // pure, host-tested audio::recordBufferBytes().
  const size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  const size_t recBytes = audio::recordBufferBytes(
      freeInternal, HEAP_MARGIN, SAMPLE_RATE * sizeof(int16_t), MAX_REC_SECONDS);

  recBuf_ = (int16_t*)heap_caps_malloc(recBytes, MALLOC_CAP_8BIT);
  recBufOk_ = (recBuf_ != nullptr);
  if (recBufOk_) {
    recSamples_ = recBytes / sizeof(int16_t);
    recSeconds_ = (float)recSamples_ / SAMPLE_RATE;
  }
  Serial.printf("PSRAM size=%u free=%u | internal free=%u | recBuf=%u B (%.1f s @ %u Hz)=%s\n",
                (unsigned)ESP.getPsramSize(), (unsigned)ESP.getFreePsram(),
                (unsigned)freeInternal, (unsigned)recBytes, recSeconds_,
                (unsigned)SAMPLE_RATE, recBufOk_ ? "OK" : "FAILED");

  // Start the mic for the live VU. Don't Speaker.end() here: the speaker isn't
  // installed at boot and ending it logs a spurious I2S error.
  M5.Mic.begin();
  if (!M5.Mic.isEnabled()) Serial.println("ERROR: mic failed to start");
  return recBufOk_;
}

uint8_t AudioIo::readVuLevel() {
  if (M5.Mic.record(vuBlock_, VU_BLOCK, SAMPLE_RATE)) {
    lastBar_ = audio::computeLevel(vuBlock_, VU_BLOCK).bar;
  }
  return lastBar_;
}

void AudioIo::startCapture() {
  // Zero the buffer so any unfilled trailing chunk plays as silence
  // (heap_caps_malloc doesn't clear, and a short take leaves an old tail).
  memset(recBuf_, 0, recSamples_ * sizeof(int16_t));
  recPos_ = 0;
}

void AudioIo::pumpCapture() {
  // recPos_ runs at most ~2 chunks ahead of what's actually filled, then stays
  // at real time as chunks complete -- it can't run away (the < 2 guard).
  while (M5.Mic.isRecording() < 2 && recPos_ + REC_CHUNK <= recSamples_) {
    M5.Mic.record(recBuf_ + recPos_, REC_CHUNK, SAMPLE_RATE);
    recPos_ += REC_CHUNK;
  }
}

void AudioIo::playCaptured() {
  enterSpeaker();  // Mic.end() finishes the small in-flight chunk, then switch
  M5.Speaker.playRaw(recBuf_, recPos_, SAMPLE_RATE);
}

void AudioIo::playTone(uint32_t freq, uint32_t ms) {
  enterSpeaker();
  M5.Speaker.tone(freq, ms);
}
