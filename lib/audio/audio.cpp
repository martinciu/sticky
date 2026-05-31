#include "audio.h"
#include <cmath>

namespace audio {

uint8_t dbToBar(float dbfs, float floorDb, float ceilDb) {
  if (dbfs <= floorDb) return 0;
  if (dbfs >= ceilDb) return 100;
  float frac = (dbfs - floorDb) / (ceilDb - floorDb);
  return (uint8_t)(frac * 100.0f + 0.5f);  // round to nearest
}

}  // namespace audio
