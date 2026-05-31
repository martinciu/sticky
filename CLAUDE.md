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
ping-pong scroll), `r8_lvgl` / `r9_lvgl_ui` (LVGL experiments), and
`native` (host tests). Each device env maps to `src/<env>/main.cpp`.

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
extend `device` (no Arduino framework, no M5 deps); it builds only `test/` +
`lib/weather` + ArduinoJson so it can run on the host.

When adding a new rung: create `src/<name>/main.cpp` and add a matching
`[env:<name>]` block that `extends = device` with its own `build_src_filter`.

### Pure-logic-vs-hardware split (this is the testing strategy)

The deliberate seam: **device-coupled code stays thin; the brains go in `lib/`
so they unit-test on the host.** `lib/weather/` depends *only* on ArduinoJson
(no Arduino/M5/WiFi headers): `parseWeather(json) → Weather` and `wmoLabel(code)`
are pure and covered by `test/test_weather/`. The device-only HTTPS fetch
(`fetchWeather()` in `src/widget/main.cpp`) wraps that pure parser. Apply the
same pattern for any new logic worth testing — keep `draw`/`fetch` thin, put
parsing/formatting/mapping in a host-portable `lib/` module.

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

### clangd note

`.clangd` strips Xtensa-GCC-only flags clangd can't parse and suppresses three
diagnostics caused by an unresolvable Xtensa system header (so M5Canvas/LovyanGFX
members don't show false "no member" errors). Trade-off: it can also hide genuine
member/overload typos in editor diagnostics — but `pio run` (the real compiler)
still catches those at build time, so trust the build over the editor squiggles.
