// Host-side unit tests for the pure weather parser (lib/weather).
// Run on your Mac with:  pio test -e native   (no board needed)
#include <unity.h>
#include "weather.h"
#ifdef ARDUINO
#include <Arduino.h> // setup()/loop()/delay() when running on the board
#endif

void setUp(void) {}
void tearDown(void) {}

// A realistic Open-Meteo "current" response.
static const char *SAMPLE =
    "{\"latitude\":52.2,\"longitude\":21.0,"
    "\"current\":{\"time\":\"2026-05-30T20:00\",\"interval\":900,"
    "\"temperature_2m\":18.3,\"weather_code\":3}}";

void test_parses_valid_response(void) {
  Weather w = parseWeather(SAMPLE);
  TEST_ASSERT_TRUE(w.ok);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 18.3f, w.tempC);
  TEST_ASSERT_EQUAL_INT(3, w.code);
}

void test_rejects_malformed_json(void) {
  Weather w = parseWeather("this is not json");
  TEST_ASSERT_FALSE(w.ok);
}

void test_rejects_missing_current(void) {
  Weather w = parseWeather("{\"hourly\":{\"temperature_2m\":[1,2,3]}}");
  TEST_ASSERT_FALSE(w.ok);
}

void test_handles_negative_temperature(void) {
  Weather w = parseWeather(
      "{\"current\":{\"temperature_2m\":-4.5,\"weather_code\":71}}");
  TEST_ASSERT_TRUE(w.ok);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -4.5f, w.tempC);
  TEST_ASSERT_EQUAL_INT(71, w.code);
}

void test_wmo_labels(void) {
  TEST_ASSERT_EQUAL_STRING("Clear", wmoLabel(0));
  TEST_ASSERT_EQUAL_STRING("Rain", wmoLabel(63));
  TEST_ASSERT_EQUAL_STRING("Storm", wmoLabel(95));
  TEST_ASSERT_EQUAL_STRING("?", wmoLabel(424242)); // unknown code
}

static void runAllTests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_parses_valid_response);
  RUN_TEST(test_rejects_malformed_json);
  RUN_TEST(test_rejects_missing_current);
  RUN_TEST(test_handles_negative_temperature);
  RUN_TEST(test_wmo_labels);
  UNITY_END();
}

#ifdef ARDUINO
// On the board: run once at boot. The delay lets the USB-CDC port reattach
// after the reset so the serial monitor doesn't miss the results.
void setup() {
  delay(2000);
  runAllTests();
}
void loop() {}
#else
// On the host: a plain executable.
int main(int, char **) {
  runAllTests();
  return 0;
}
#endif
