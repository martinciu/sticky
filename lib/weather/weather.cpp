#include "weather.h"

const char *wmoLabel(int code) {
  switch (code) {
  case 0:                    return "Clear";
  case 1: case 2: case 3:    return "Cloudy";
  case 45: case 48:          return "Fog";
  case 51: case 53: case 55: return "Drizzle";
  case 61: case 63: case 65: return "Rain";
  case 71: case 73: case 75: return "Snow";
  case 80: case 81: case 82: return "Showers";
  case 95: case 96: case 99: return "Storm";
  default:                   return "?";
  }
}

Weather parseWeather(const char *json) {
  Weather w;
  JsonDocument doc;
  if (deserializeJson(doc, json))
    return w; // malformed JSON -> ok stays false
  JsonObject cur = doc["current"];
  if (cur.isNull())
    return w;
  w.tempC = cur["temperature_2m"] | 0.0f;
  w.code = cur["weather_code"] | -1;
  w.ok = (w.code >= 0);
  return w;
}
