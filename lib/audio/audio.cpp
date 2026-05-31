#include "audio.h"
#include <cmath>

namespace audio {

uint8_t dbToBar(float dbfs, float floorDb, float ceilDb) {
  if (dbfs <= floorDb) return 0;
  if (dbfs >= ceilDb) return 100;
  float frac = (dbfs - floorDb) / (ceilDb - floorDb);
  return (uint8_t)(frac * 100.0f + 0.5f);  // round to nearest
}

Level computeLevel(const int16_t* samples, size_t n) {
  if (samples == nullptr || n == 0) {
    return Level{0.0f, 0, -160.0f, 0};  // silent
  }
  int32_t peak = 0;
  double  sumSquares = 0.0;
  for (size_t i = 0; i < n; ++i) {
    int32_t s = samples[i];
    int32_t a = s < 0 ? -s : s;  // |sample| in 32-bit (avoids INT16_MIN overflow)
    if (a > peak) peak = a;
    sumSquares += (double)s * (double)s;
  }
  float rms = (float)std::sqrt(sumSquares / (double)n);
  // Reference full scale = 32767; silence collapses to a low floor sentinel.
  float dbfs = peak > 0 ? 20.0f * std::log10((float)peak / 32767.0f) : -160.0f;
  int16_t peak16 = peak > 32767 ? 32767 : (int16_t)peak;
  return Level{rms, peak16, dbfs, dbToBar(dbfs)};
}

}  // namespace audio
