# Sticky 🕐

A learning-oriented embedded project for the **M5Stack StickS3** (ESP32-S3). The
end goal is an always-on desk **clock that also shows local weather**
(via [Open-Meteo](https://open-meteo.com)) — but the project is built as an
**incremental ladder of individually-runnable programs ("rungs")**, each a
complete see-it-work demo that teaches one piece on the way to the finished
widget.

> Built by a strong software developer who is new to hardware — so the codebase
> optimizes for *learning the hardware dev loop*, with the brains kept in
> host-testable modules and the device code deliberately thin.

## 🔧 Hardware

[M5Stack StickS3](https://docs.m5stack.com/en/core/StickS3) — a keychain-sized
ESP32-S3 dev kit:

| | |
|---|---|
| **MCU** | ESP32-S3-PICO-1 — WiFi 2.4 GHz, BLE 5, 8 MB flash, 8 MB PSRAM |
| **Display** | 1.14" LCD, ST7789P3, 135 × 240 px |
| **Buttons** | KEY1, KEY2 (+ power button) |
| **Audio** | ES8311 codec + MEMS mic + speaker amp |
| **Other** | BMI270 IMU, IR TX/RX, 250 mAh battery |
| **USB** | USB-C, native USB-CDC (no driver needed to flash/monitor on macOS) |

[`M5Unified`](https://github.com/m5stack/M5Unified) abstracts the board — code
uses `M5.Display`, `M5.BtnA/BtnB`, `M5.Power`, `M5.Mic`, `M5.Speaker` rather than
raw pins.

## 🪜 The rungs

Each rung is its own folder under `src/` and its own PlatformIO environment.
Build/flash any one with `-e <env>`.

| Env | What it teaches | Visible result |
|-----|-----------------|----------------|
| `r1_hello` | Toolchain + flash/monitor loop | "Hello" on LCD + serial |
| `r2_screen` | Text, color, coords, flicker-free sprites | Shapes + bouncing sprite |
| `r3_buttons` | `M5.BtnA/BtnB`, battery readout | UI reacts; battery % |
| `r4_wifi` | Join WiFi from `secrets.h` | "Connected ✓" + IP/RSSI |
| `r5_clock` | NTP sync + timezone, ticking clock | A live clock |
| `r6_weather` | HTTPS GET + JSON → struct | Real temp / conditions |
| `widget` | State machine, refresh timing, layout | Assembled clock + weather |
| `flicker_demo` | Sprite vs direct-draw comparison | Side-by-side flicker test |
| `r8_lvgl` / `r9_lvgl_ui` | LVGL on M5GFX + 2-button encoder nav | LVGL widgets |
| `mic_speaker_demo` | ES8311 mic + speaker | VU meter, push-to-talk record/playback, tone |
| `native` | Host unit tests (no board) | Runs on your Mac |

## 🚀 Quick start

**1. Install the PlatformIO CLI** (via [uv](https://docs.astral.sh/uv/), no global Python needed):

```bash
uv tool install platformio
pio --version
```

**2. Clone and add WiFi credentials** (only needed from `r4_wifi` onward):

```bash
git clone https://github.com/martinciu/sticky.git
cd sticky
cp include/secrets.example.h include/secrets.h   # then edit with your 2.4 GHz SSID
```

`include/secrets.h` is **gitignored** — your real credentials never get committed.
The StickS3 is **2.4 GHz only**, so use a 2.4 GHz network.

**3. Build & flash a rung** over USB-C:

```bash
pio run -e r1_hello -t upload -t monitor    # compile, flash, stream serial
```

A bare `pio run` builds `r1_hello` (the default env). Exit the serial monitor
with `Ctrl+C`. If upload can't find the port, add
`--upload-port /dev/cu.usbmodemXXXX` (find it with `pio device list`).

**4. Run the host unit tests** (no board required):

```bash
pio test -e native
```

## ⚙️ Configuration

- **`include/config.h`** (committed) — non-secret tunables: location lat/lon
  (defaults to Warsaw — change `WIDGET_LAT`/`WIDGET_LON`, find yours at
  [latlong.net](https://www.latlong.net)), POSIX `TZ` string, and refresh
  intervals.
- **`include/secrets.h`** (gitignored) — `WIFI_SSID` / `WIFI_PASSWORD`.

## 🏗️ How it's organized

- **Per-rung isolation.** `platformio.ini` has a shared `[device]` base; each rung
  env does `extends = device` and sets `build_src_filter = -<*> +<rung/>` so only
  that rung's folder compiles — rungs never see each other's `main.cpp`.
- **Testable seam.** Device-coupled code stays thin in `src/`; the pure brains go
  in `lib/` so they unit-test on the host. `lib/weather` (`parseWeather`,
  `wmoLabel`) and `lib/audio` (`computeLevel`, `dbToBar`, `recordBufferBytes`)
  depend only on the standard library + ArduinoJson, and are covered by
  `test/`. Hardware behavior (drawing, audio, battery) is verified by flashing
  and observing.
- **Design docs live in `docs/`** — the full design and rung-by-rung plans for the
  weather clock and the mic/speaker demo are committed there.

```
src/<rung>/        one folder per rung (its own PlatformIO env)
lib/weather/       pure weather JSON parser + WMO labels  (host-tested)
lib/audio/         pure audio level + buffer math         (host-tested)
include/           config.h (committed), secrets.h (gitignored), shared headers
test/              Unity host tests
docs/              design + implementation plans
```

## 🛠️ Tooling

```bash
pio run -t compiledb     # regenerate compile_commands.json for clangd
doxygen Doxyfile         # browseable HTML API docs for M5GFX/M5Unified
```

---

*A personal learning project — built incrementally, one runnable rung at a time.*
