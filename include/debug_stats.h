#pragma once
//
// Lightweight runtime stats over serial. Header-only (inline), safe to include
// from any rung. Usage:
//
//     #include "debug_stats.h"
//     void loop() {
//       ...
//       debugStatsEvery();   // prints at most every 5 s
//     }
//
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Print one stats line right now.
inline void printStats() {
  Serial.printf(
      "[stats] heap free=%lu min=%lu maxblk=%lu | psram free=%lu | "
      "stack-hwm=%lu B | cpu=%d MHz | up=%lus\n",
      (unsigned long)ESP.getFreeHeap(),     // free internal RAM right now
      (unsigned long)ESP.getMinFreeHeap(),  // all-time low (leak / peak detector)
      (unsigned long)ESP.getMaxAllocHeap(), // largest single block (fragmentation)
      (unsigned long)ESP.getFreePsram(),    // free PSRAM (~8 MB on the StickS3)
      (unsigned long)uxTaskGetStackHighWaterMark(NULL), // loop-task stack headroom (bytes)
      ESP.getCpuFreqMHz(),
      (unsigned long)(millis() / 1000));
}

// Call once per loop(); prints at most every intervalMs (default 5 s). The
// timer state is a function-local static, so there's nothing to declare at the
// call site.
inline void debugStatsEvery(uint32_t intervalMs = 5000) {
  static uint32_t last = 0;
  if (millis() - last >= intervalMs) {
    last = millis();
    printStats();
  }
}
