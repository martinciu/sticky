#pragma once
// Pure audio level math. Depends ONLY on the C++ standard library -- no
// Arduino/M5/I2S headers -- so it compiles and unit-tests on the native (host)
// environment, no hardware required. The device-only mic capture and speaker
// output live in src/mic_speaker_demo/main.cpp and call into this.
#include <cstddef>
#include <cstdint>

namespace audio {

struct Level {
  float   rms;   // root-mean-square of the block (linear, 0..32767)
  int16_t peak;  // max |sample| in the block (0..32767)
  float   dbfs;  // peak in dBFS: 0 dB == full scale, more negative == quieter
  uint8_t bar;   // 0..100, ready to drive a VU bar
};

// Map a dBFS value onto a 0..100 bar. Values <= floorDb clamp to 0, values
// >= ceilDb clamp to 100, linear in between. Default window: -60..0 dBFS.
uint8_t dbToBar(float dbfs, float floorDb = -60.0f, float ceilDb = 0.0f);

// Compute peak + RMS over a block of mono int16 samples, derive dBFS from the
// peak, and map it to a 0..100 bar via dbToBar(). An empty/null block returns
// a silent Level (peak 0, rms 0, bar 0).
Level computeLevel(const int16_t* samples, size_t n);

// Bytes to allocate for the record buffer: free RAM minus a safety margin,
// clamped to maxSeconds of audio, rounded down to a whole number of int16
// samples (even byte count). Returns 0 if free RAM doesn't exceed the margin.
size_t recordBufferBytes(size_t freeBytes, size_t marginBytes,
                         size_t bytesPerSecond, float maxSeconds);

}  // namespace audio
