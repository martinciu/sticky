# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

**Sticky** is a learning-oriented embedded project for the **M5Stack StickS3**
(ESP32-S3). The end goal is an always-on desk clock that also shows current
local weather (Open-Meteo). The author is a strong software developer new to
hardware, so the codebase is deliberately structured as an **incremental ladder
of individually-runnable programs ("rungs")** — each one a complete, see-it-work
demo that teaches one piece, building up to the final `widget`.

Read `docs/2026-05-30-sticky-weather-clock-design.md` for the full design and
`docs/2026-05-30-sticky-plan-1-foundation-basics.md` for the rung-by-rung plan.
`docs/` is **committed** here (specs + plans live in-repo, not in `.superpowers/`).

## Commands

All commands run from the repo root via PlatformIO CLI (`pio`, a uv-managed
tool). There is **one PlatformIO environment per rung** — pick the env with `-e`.

```bash
pio run -e <env>                          # compile only (no board needed)
pio run -e <env> -t upload -t monitor     # flash over USB-C + stream serial
pio test -e native                        # host unit tests on the Mac (no board)
pio device list                           # find the /dev/cu.usbmodem… port
```

- `default_envs = r1_hello`, so a bare `pio run` builds rung 1.
- Flashing is native USB-CDC — no macOS driver; M5 boards auto-reset into the
  bootloader, so upload is normally one command. The `esp32_exception_decoder`
  monitor filter decodes crash backtraces to line numbers.
- Exit the serial monitor with `Ctrl+C`. Add `--upload-port /dev/cu.usbmodemXXXX`
  if upload can't auto-find the port.

### Environments (rungs)

`r1_hello` → `r2_screen` → `r3_buttons` → `r4_wifi` → `r5_clock` → `r6_weather`
→ `widget` (the assembled clock+weather gadget), plus `flicker_demo` (sprite vs
direct-draw comparison), `font_demo` (font catalog with spring transitions +
ping-pong scroll), `r8_lvgl` / `r9_lvgl_ui` (LVGL experiments),
`mic_speaker_demo` (ES8311 mic + speaker), `marble_demo` (BMI270 tilt game with
8-bit chiptune audio), and `native` (host tests). A simple rung is a single
`src/<env>/main.cpp`; richer rungs (`mic_speaker_demo`, `marble_demo`) split into
several files under `src/<env>/` (e.g. `app` + view + hardware-io + `main`), all
compiled by the one `build_src_filter`.

### Tooling

```bash
pio run -t compiledb     # regenerate compile_commands.json for clangd (it's gitignored)
doxygen Doxyfile         # browseable HTML API docs for M5GFX/M5Unified → .doxygen/html/
```

## Architecture

### Per-rung isolation via `build_src_filter`

`platformio.ini` has a shared `[device]` base (board, PSRAM, framework, M5
deps). Each device env does `extends = device` and sets
`build_src_filter = -<*> +<<rung>/>` so **only that rung's folder compiles** —
the rungs never see each other's `main.cpp`. The `native` env does **not**
extend `device` (no Arduino framework, no M5 deps); it builds only `test/` plus
whatever pure `lib/` modules the tests `#include` (`lib/weather`, `lib/audio`,
`lib/marble`) + ArduinoJson, so it runs on the host.

When adding a new rung: create `src/<name>/` (at least `main.cpp`; split into more
files if it grows) and add a matching `[env:<name>]` block that
`extends = device` with `build_src_filter = -<*> +<<name>/>`.

### Pure-logic-vs-hardware split (this is the testing strategy)

The deliberate seam: **device-coupled code stays thin; the brains go in `lib/`
so they unit-test on the host.** `lib/weather/` depends *only* on ArduinoJson
(no Arduino/M5/WiFi headers): `parseWeather(json) → Weather` and `wmoLabel(code)`
are pure and covered by `test/test_weather/`. The device-only HTTPS fetch
(`fetchWeather()` in `src/widget/main.cpp`) wraps that pure parser. Apply the
same pattern for any new logic worth testing — keep `draw`/`fetch` thin, put
parsing/formatting/mapping in a host-portable `lib/` module. The same seam holds
`lib/audio` (level/buffer math behind `mic_speaker_demo`) and `lib/marble` (the
tilt-marble physics, wall bounce, collisions, scoring, and deterministic spawn
placement behind `marble_demo`) — both pure, both host-tested, with the
device-only hardware (`M5.Imu`, `M5.Speaker`, rendering) kept thin in `src/`.

Hardware behavior (drawing, button feel, battery) is verified by **flashing and
observing** the LCD + serial, not by automated tests.

### Render-loop conventions (device code)

- **`M5Unified` abstracts the board** — use `M5.Display`, `M5.BtnA`/`BtnB`,
  `M5.Power`, never raw pins. `M5.update()` must run each `loop()` before reading
  buttons (`wasPressed()`). `setRotation(1)` = 240×135 landscape.
- **Flicker-free rendering:** draw the whole frame into an off-screen `M5Canvas`
  sprite, then `pushSprite(0,0)` in one shot. See `src/widget/main.cpp` and
  `flicker_demo`.
- **Non-blocking loop:** schedule periodic work with `millis()` deltas, not
  `delay()` stalls. Once NTP-synced the ESP32 keeps ticking internally, so the
  clock never freezes when WiFi or the weather API hiccups. On a failed weather
  fetch, keep the **last good reading marked stale** — the UI never blanks.
- `include/debug_stats.h` is header-only: call `debugStatsEvery(ms)` in `loop()`
  for periodic heap/stack/PSRAM serial reports.

### Config & secrets

- `include/secrets.h` (**gitignored**) holds `WIFI_SSID` / `WIFI_PASSWORD`.
  Bootstrap with `cp include/secrets.example.h include/secrets.h` then edit.
  The StickS3 is **2.4 GHz only** — use a 2.4 GHz SSID.
- `include/config.h` (committed) holds non-secret tunables: location lat/lon,
  POSIX `TZ` string, weather-refresh and NTP-resync intervals.

### LVGL rungs (r8/r9)

The LVGL envs add `-DLV_CONF_INCLUDE_SIMPLE` and `lvgl/lvgl@^9.2` on top of the
`device` base; LVGL finds the config at `include/lv_conf.h` via the include path.
The M5GFX bridge is a `flush_cb` (`setAddrWindow` + `writePixels`) plus a
`tick_cb` returning `millis()`. The two physical buttons are wired as a single
LVGL **encoder** input device (BtnA = advance focus / change value, BtnB =
press/select), driving a focus group.

### IMU + audio rungs (`mic_speaker_demo`, `marble_demo`)

- **Audio** uses `M5.Speaker` (square-wave `tone()` = chiptune) and `M5.Mic`, both
  behind the shared **ES8311 codec** — mic and speaker can't run at once, so
  `mic_speaker_demo` toggles them (`M5.Mic.end()` / `M5.Speaker.begin()`).
  `marble_demo` uses the speaker only: background music and SFX run on **separate
  speaker channels** (0 = music, 1 = SFX, 2 = bounce) driven by a small
  non-blocking note sequencer, so they mix and never stall the loop
  (`stop_current_sound` is per-channel, so an SFX won't cut the music). See
  `src/marble_demo/sound.*`.
- **IMU** (`marble_demo`) reads tilt via `M5.Imu.getAccel`; "level" is calibrated
  at start (re-zero on the side button), and the raw-axis→screen sign/mapping was
  confirmed on first flash — it's recorded in the design-doc Findings (X is
  inverted, Y normal, axes not swapped). Re-confirm if the hardware changes.
- **Buttons** in `marble_demo`: `BtnA` start/restart, `BtnB` music on/off, the
  side power button (short click via the M5PM1) re-zeroes level.

### clangd note

`.clangd` strips Xtensa-GCC-only flags clangd can't parse and suppresses three
diagnostics caused by an unresolvable Xtensa system header (so M5Canvas/LovyanGFX
members don't show false "no member" errors). Trade-off: it can also hide genuine
member/overload typos in editor diagnostics — but `pio run` (the real compiler)
still catches those at build time, so trust the build over the editor squiggles.
