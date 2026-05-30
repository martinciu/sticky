#pragma once
// Pure weather model + parsing. Depends ONLY on ArduinoJson (which is
// host-portable) -- no Arduino/M5/WiFi headers -- so this compiles and
// unit-tests on the native (host) environment, no hardware required.
#include <ArduinoJson.h>

struct Weather {
  bool ok = false;
  float tempC = 0.0f;
  int code = -1; // WMO weather code
};

// WMO weather code -> short human-readable label.
const char *wmoLabel(int code);

// Parse an Open-Meteo JSON response into a Weather. Pure: no network/hardware.
// Returns ok=false on malformed JSON or a missing/invalid "current" object.
Weather parseWeather(const char *json);
