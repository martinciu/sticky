# Sticky — StickS3 Desk Clock + Weather Widget — Design

- **Date:** 2026-05-30
- **Device:** M5Stack StickS3 (ESP32-S3)
- **Goal:** Learn embedded development by building, on a brand-new board, a useful
  desk gadget: an always-on clock that also shows current local weather.
- **Author context:** Strong software developer, new to microcontroller/hardware
  work. Optimize for learning the *hardware dev loop*; the language/concepts are
  already familiar.

## 1. Hardware

M5Stack StickS3 — keychain-sized ESP32-S3 dev kit (announced Jan 2026).

| Capability | Detail |
|---|---|
| MCU | ESP32-S3-PICO-1-N8R8 — WiFi 2.4 GHz, BLE 5, 8 MB flash, **8 MB OPI PSRAM** |
| Display | 1.14" LCD, **ST7789P3** driver, **135 × 240 px** (portrait/tall) |
| Buttons | KEY1 (G11), KEY2 (G12) |
| IMU | BMI270 (6-axis) — *not used in v1* |
| Audio | ES8311 codec + MEMS mic + AW8737 amp — *not used in v1* |
| IR | TX G46 / RX G42 — *not used in v1* |
| Power | M5PM1 power-management chip, 250 mAh battery |
| USB | USB-OTG, native USB-CDC (no macOS driver needed for flashing/serial) |

`M5Unified` abstracts the board specifics — code uses `M5.Display`, `M5.BtnA`,
`M5.Power`, etc., rather than raw pins. Full pin map in the Appendix for reference.

## 2. Locked decisions (from brainstorming)

| Decision | Choice |
|---|---|
| Experience level | New to hardware; strong general dev |
| Learning shape | Basics first (rungs 1–3), then build the gadget (rungs 4–7) |
| Toolchain | **C++ via PlatformIO** + `M5Unified` |
| Target gadget | **Desk clock + weather widget** |
| Build strategy | **Incremental ladder** of ~7 individually-runnable rungs |

## 3. Toolchain & project setup

### platformio.ini board block (shared by all device environments; the `native` test env is separate — see §8)

```ini
platform = espressif32@6.12.0
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.memory_type = qio_opi   ; 8 MB OPI PSRAM
monitor_speed = 115200
lib_deps =
    m5stack/M5Unified
    bblanchon/ArduinoJson@^7
```

### Repo layout

```
sticky/
├── platformio.ini          # board config + one [env] per rung
├── src/
│   ├── r1_hello/main.cpp    # each rung is its own folder
│   ├── r2_screen/main.cpp
│   ├── r3_buttons/main.cpp
│   ├── r4_wifi/main.cpp
│   ├── r5_clock/main.cpp
│   ├── r6_weather/main.cpp
│   └── widget/main.cpp      # the final gadget
├── lib/                     # reusable modules (net, timekeeper, weather, ui)
├── include/
│   ├── secrets.example.h    # committed template
│   ├── secrets.h            # real WiFi creds — GITIGNORED
│   └── config.h             # tz, units, location, intervals — committed
├── test/                    # native (host) unit tests
├── docs/                    # specs + plans — COMMITTED (tracked in this repo)
└── .gitignore               # ignores .pio/ + include/secrets.h (docs/ is tracked)
```

Each rung is a separate PlatformIO environment using `build_src_filter` so only
that rung's folder compiles. Flash any rung on demand:

```
pio run -e r1_hello -t upload -t monitor
pio run -e widget   -t upload -t monitor
```

### Dev loop (new-to-hardware notes)

- Plug in via USB-C → enumerates as a serial port (native USB-CDC, no driver).
- `upload` compiles + flashes; `monitor` streams `Serial.println(...)` output —
  the hardware equivalent of `printf` debugging.
- M5 boards auto-reset into bootloader, so flashing is normally one command.
  Fallback (if a flash ever hangs): hold a button during reset — document the
  exact combo when first encountered.

## 4. Milestone ladder

Rungs 1–3 = warm-up basics; 4–7 = build the gadget. Each is a complete,
see-it-work program.

| # | Rung | Learn | Visible result |
|---|------|-------|----------------|
| 1 | Hello, hardware | Whole toolchain + dev loop | "Hello" on LCD + serial |
| 2 | Screen & graphics | Text sizes/colors, 135×240 coords, sprites | Shapes + styled text |
| 3 | Buttons & battery | `M5.BtnA/BtnB`, battery % from power chip | UI reacts; battery shown |
| 4 | WiFi connect | Join WiFi from `secrets.h`, connection state | "Connected ✓" + IP |
| 5 | NTP clock | Time sync, timezone, render loop | A ticking clock |
| 6 | Weather fetch | HTTPS GET, JSON → struct | Real temp/conditions |
| 7 | Assemble widget | State machine, refresh timing, errors, layout | Finished clock + weather |

By rung 5 there is already a working desk clock; 6–7 add weather and polish.

## 5. Final widget architecture

### Modules (each one job, clean interface)

```
include/secrets.h     WiFi SSID/password (secrets only)           [gitignored]
include/config.h      timezone, units, refresh interval, location [committed]
lib/net/              WiFi connect + auto-reconnect, status enum
lib/timekeeper/       NTP sync (configTzTime) + getLocalTime helpers
lib/weather/
  weather_client      device-only: HTTPS GET → raw JSON string
  weather_parse       PURE: JSON string → WeatherData struct   ← unit-tested
lib/ui/               render(WeatherData, time, status) → off-screen sprite → push
src/widget/main.cpp   setup() + loop(): the state machine
```

**Key split:** `weather_client` (network) vs `weather_parse` (logic). The parser
takes a `String` and returns a struct — no hardware, no network — so it is
unit-testable on the host. Same for the WMO-code→label/icon map and date/time
formatting.

### Data flow & state machine

Non-blocking loop (millis-based scheduling — no `delay()` stalls):

```
BOOT ─▶ WIFI_CONNECTING ─▶ TIME_SYNCING ─▶ RUNNING
                                             │  every 1s    → redraw clock
                                             │  every ~15m  → refresh weather
                                             │  BtnA        → force refresh now
                                             │  BtnB        → cycle view
                                             └─ WiFi drops  → back to reconnect
```

Once time is synced the ESP32 keeps ticking internally, so the **clock never
freezes** even if WiFi or the weather API hiccups.

## 6. Key tech choices

- **Weather API → Open-Meteo** (`https://api.open-meteo.com/v1/forecast`).
  Free, **no API key, no signup**. One GET with
  `?latitude=..&longitude=..&current=temperature_2m,weather_code&timezone=auto`
  returns current temp + a WMO weather code as JSON. Removes the biggest beginner
  hurdle (key management). Location lives in `config.h` (non-secret); seed value
  Warsaw `52.23, 21.01` — trivially changed.
- **HTTPS → `WiFiClientSecure` + `setInsecure()`.** Open-Meteo is HTTPS-only;
  `setInsecure()` skips cert pinning — fine for a read-only desk gadget. 8 MB
  PSRAM gives TLS plenty of buffer. (Cert pinning = documented "later" upgrade.)
- **JSON → ArduinoJson v7.** Parse just the small `current` object.
- **Time → `configTzTime` + POSIX TZ string** `CET-1CEST,M3.5.0,M10.5.0/3`
  (Poland, automatic DST), NTP `pool.ntp.org`. Re-sync hourly.
- **Secrets → gitignored `secrets.h` + committed `secrets.example.h`.** Teaches
  secrets hygiene from day one.

## 7. Error handling

- **No WiFi:** retry with backoff; show "WiFi…" status glyph; clock keeps ticking
  if already synced.
- **Weather API fail/timeout:** keep showing **last good reading marked "stale"**
  with its timestamp + warning glyph; retry next interval. UI never blanks/crashes.
- **Bad/unexpected JSON:** parser returns an error result → treated as a failed
  refresh (stale display), logged to serial.

## 8. Testing

- **Pure logic → host unit tests** (`pio test -e native`, Unity): JSON→struct
  parser (fed saved sample responses), weather-code→label mapping, date/time
  formatting. True red-green-refactor, runs on the Mac in ms — no flashing.
- **Hardware behavior → observation:** screen + serial logs. Keep device-coupled
  code (draw, fetch) thin; put the brains under test.

## 9. Scope (YAGNI)

**In (v1):** clock + current weather + battery indicator + one alternate view
(BtnB) + force-refresh (BtnA).

**Out (notes for "later," each a future rung):** multi-day forecast, multiple
cities, WiFi captive-portal config, IMU/shake gestures, TLS cert pinning, sound.

## 10. Open items to confirm during implementation

- Exact `M5.Power` API for battery % on the StickS3 (M5PM1 chip) — verify at rung 3;
  may need the `M5PM1` library if M5Unified doesn't expose it.
- Confirm `build_src_filter` syntax cleanly isolates each rung folder.
- Confirm whether the StickS3 has a hardware RTC (affects time persistence across
  reboots); if absent, re-sync NTP on every boot (already the plan).

## Appendix — StickS3 pin map (reference; M5Unified abstracts these)

- **LCD:** MOSI G39, SCK G40, RS G45, CS G41, RST G21, BL G38 (ST7789P3, 135×240)
- **Buttons:** KEY1 G11, KEY2 G12
- **IMU (BMI270):** SCL G48, SDA G47
- **IR:** TX G46, RX G42
- **Audio (ES8311):** MCLK G18, DOUT G14, BCLK G17, LRCK G15, DIN G16
- **Power/battery:** via M5PM1 chip

### Sources

- StickS3 docs — https://docs.m5stack.com/en/core/StickS3
- StickS3 Arduino/PlatformIO program guide — https://docs.m5stack.com/en/arduino/m5sticks3/program
- Store page — https://shop.m5stack.com/products/m5sticks3-esp32s3-mini-iot-dev-kit
- Open-Meteo — https://open-meteo.com
