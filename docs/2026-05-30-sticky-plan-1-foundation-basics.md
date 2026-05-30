# Sticky — Plan 1: Foundation + Basics (Rungs 0–3) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (inline, recommended for this plan — see note) or superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **Why inline:** every verification step requires a human to physically flash the board, watch the LCD, and press buttons. A subagent cannot observe hardware. Run these tasks inline with the user driving the device.

**Goal:** Stand up the PlatformIO toolchain for the M5Stack StickS3, get a first program flashing and visible, and learn the three "basics" — screen drawing, button input, and battery reading — each as its own runnable program.

**Architecture:** A single PlatformIO project where each learning "rung" lives in its own `src/<rung>/` folder and builds via its own `[env]` using `build_src_filter`. Code uses the `M5Unified` library, which abstracts the StickS3's pins behind `M5.Display`, `M5.BtnA/BtnB`, and `M5.Power`.

**Tech Stack:** PlatformIO Core (CLI), Arduino-ESP32 framework, `M5Unified` library, ESP32-S3.

**Verification model:** This plan has no host unit tests — every rung is hardware-interaction code, verified by flashing and observing the LCD + serial monitor. (Host unit tests begin in Plan 2 with the pure weather-JSON parser.) Each task's "verify" step lists the exact expected screen/serial result.

---

## File Structure

| File | Responsibility |
|------|----------------|
| `.gitignore` | Ignore `.pio/` build dir and `include/secrets.h`. `docs/` stays tracked. |
| `platformio.ini` | Shared board/PSRAM config + one `[env]` per rung selecting its folder. |
| `src/r1_hello/main.cpp` | Rung 1: prove the toolchain — text on LCD + serial. |
| `src/r2_screen/main.cpp` | Rung 2: text sizes/colors, coordinate system, flicker-free sprite. |
| `src/r3_buttons/main.cpp` | Rung 3: read `BtnA`/`BtnB`, show counter + battery %. |

---

## Task 1: Toolchain + repo scaffold

**Files:**
- Create: `/Users/martinciu/code/sticky/.gitignore`
- Create: `/Users/martinciu/code/sticky/platformio.ini`

- [ ] **Step 1: Confirm PlatformIO CLI is installed (install via uv if not)**

Run:
```bash
pio --version
```
Expected: prints something like `PlatformIO Core, version 6.x`.

If `pio` is not found, install it as a uv-managed tool (per this machine's Python-CLI policy — not brew, not pipx):
```bash
uv tool install platformio
pio --version
```
Expected after install: version prints successfully.

- [ ] **Step 2: Initialize the git repo on a feature branch (never commit to main)**

Run:
```bash
cd /Users/martinciu/code/sticky
git init
git checkout -b foundation-basics
```
Expected: `Initialized empty Git repository …` then `Switched to a new branch 'foundation-basics'`. (The first commit lands on `foundation-basics`, leaving `main` clean for a later PR.)

- [ ] **Step 3: Create `.gitignore`**

```gitignore
# PlatformIO build artifacts
.pio/

# Secrets (real WiFi creds live here, never committed) — arrives in Plan 2
include/secrets.h

# macOS
.DS_Store
```

- [ ] **Step 4: Create `platformio.ini`**

```ini
[platformio]
default_envs = r1_hello

# Shared by every device environment below.
[env]
platform = espressif32@6.12.0
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.memory_type = qio_opi      ; 8 MB OPI PSRAM on the StickS3
monitor_speed = 115200
monitor_filters = esp32_exception_decoder       ; decode crash backtraces to line numbers
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1                  ; route Serial to the native USB-C port
lib_deps =
    m5stack/M5Unified

# One environment per learning rung; build_src_filter compiles only that folder.
[env:r1_hello]
build_src_filter = -<*> +<r1_hello/>

[env:r2_screen]
build_src_filter = -<*> +<r2_screen/>

[env:r3_buttons]
build_src_filter = -<*> +<r3_buttons/>
```

- [ ] **Step 5: Verify the config parses**

Run:
```bash
pio project config
```
Expected: PlatformIO prints the parsed `[platformio]`, `[env]`, and three `[env:rN_*]` sections with no error. (This validates the INI before any source exists.)

- [ ] **Step 6: Commit**

```bash
git add .gitignore platformio.ini docs/
git commit -m "chore: scaffold PlatformIO project for StickS3"
```

---

## Task 2: Rung 1 — Hello, hardware (the first flash)

**Files:**
- Create: `/Users/martinciu/code/sticky/src/r1_hello/main.cpp`

This is the milestone that proves the *entire* loop: board config, library, compile, flash over native USB, and serial monitor.

- [ ] **Step 1: Write `src/r1_hello/main.cpp`**

```cpp
#include <M5Unified.h>

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);                 // inits the StickS3: display, power, buttons, etc.

  Serial.begin(115200);
  Serial.println("Hello from StickS3!");

  M5.Display.setRotation(1);     // landscape: ~240 wide x 135 tall
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 10);
  M5.Display.println("Hello,");
  M5.Display.println("StickS3!");
}

void loop() {
  M5.update();
  Serial.printf("uptime: %lu ms\n", millis());
  delay(1000);
}
```

- [ ] **Step 2: Build it (compile only, no board needed yet)**

Run:
```bash
pio run -e r1_hello
```
Expected: PlatformIO downloads `M5Unified` + the ESP32 toolchain on first run (may take a few minutes), then ends with `SUCCESS`. If it fails to compile, fix before flashing.

- [ ] **Step 3: Plug in the StickS3 and find its port**

Connect the StickS3 to the Mac via USB-C, then run:
```bash
pio device list
```
Expected: a port like `/dev/cu.usbmodem…` appears. (Native USB-CDC — no driver needed.)

- [ ] **Step 4: Flash and open the serial monitor**

Run:
```bash
pio run -e r1_hello -t upload -t monitor
```
Expected:
- Upload reaches `100%` then `SUCCESS`.
- The LCD shows **"Hello, StickS3!"** in green on black.
- The serial monitor streams `Hello from StickS3!` then `uptime: <n> ms` once per second.

Exit the monitor with `Ctrl+C`. (If upload can't find the port, append `--upload-port /dev/cu.usbmodemXXXX` from Step 3. If upload hangs, note it — we'll document the manual bootloader button combo, but M5 boards normally auto-flash.)

- [ ] **Step 5: Commit**

```bash
git add src/r1_hello/main.cpp
git commit -m "feat(r1): hello world on LCD + serial"
```

---

## Task 3: Rung 2 — Screen & graphics (text, color, coordinates, sprites)

**Files:**
- Create: `/Users/martinciu/code/sticky/src/r2_screen/main.cpp`

Teaches the 240×135 coordinate system, text sizing/color, primitive shapes, and the **sprite** (off-screen canvas) technique that gives flicker-free animation — the foundation for the clock face later.

- [ ] **Step 1: Write `src/r2_screen/main.cpp`**

```cpp
#include <M5Unified.h>

M5Canvas canvas(&M5.Display);   // an off-screen buffer the size of the screen

int barX = 0;
int dir = 2;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  M5.Display.setRotation(1);    // try 0 later to see portrait 135x240
  Serial.printf("Display: %d x %d\n", M5.Display.width(), M5.Display.height());

  // --- Static demo: text sizes, colors, and shapes ---
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(4, 4);
  M5.Display.print("size 1 text");

  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setCursor(4, 20);
  M5.Display.print("size 2");

  M5.Display.fillRect(4, 50, 60, 24, TFT_RED);       // filled rectangle
  M5.Display.drawRect(72, 50, 60, 24, TFT_GREEN);    // outlined rectangle
  M5.Display.fillCircle(180, 62, 16, TFT_YELLOW);    // filled circle
  delay(2000);

  // Allocate the sprite buffer once (reused every frame).
  canvas.createSprite(M5.Display.width(), M5.Display.height());
}

void loop() {
  M5.update();

  // Draw the whole frame off-screen, then push it in one shot (no flicker).
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextColor(TFT_WHITE);
  canvas.setTextSize(2);
  canvas.setCursor(4, 4);
  canvas.print("sprite demo");
  canvas.fillRect(barX, 70, 30, 30, TFT_ORANGE);
  canvas.pushSprite(0, 0);

  barX += dir;
  if (barX <= 0 || barX >= M5.Display.width() - 30) dir = -dir;
  delay(16);   // ~60 fps
}
```

- [ ] **Step 2: Build**

Run:
```bash
pio run -e r2_screen
```
Expected: `SUCCESS`.

- [ ] **Step 3: Flash and observe**

Run:
```bash
pio run -e r2_screen -t upload -t monitor
```
Expected:
- Serial prints `Display: 240 x 135` (confirming the landscape dimensions).
- LCD shows static text + red/green rectangles + a yellow circle for ~2 seconds.
- Then an **orange square smoothly bounces** left↔right under the "sprite demo" label with **no flicker**.

Exit with `Ctrl+C`.

- [ ] **Step 4: (Optional learning) Flip orientation**

Change `M5.Display.setRotation(1)` to `setRotation(0)`, re-run Step 3, and watch the layout become tall (135 wide × 240 high) and the serial print `Display: 135 x 240`. Set it back to `1` before committing.

- [ ] **Step 5: Commit**

```bash
git add src/r2_screen/main.cpp
git commit -m "feat(r2): screen graphics + flicker-free sprite animation"
```

---

## Task 4: Rung 3 — Buttons & battery

**Files:**
- Create: `/Users/martinciu/code/sticky/src/r3_buttons/main.cpp`

Teaches the `M5.update()` → `M5.BtnX.wasPressed()` input pattern and reading battery state. Also **empirically resolves two open items** from the spec: which physical button maps to `BtnA` vs `BtnB`, and whether `M5.Power.getBatteryLevel()` returns a real value on this board.

- [ ] **Step 1: Write `src/r3_buttons/main.cpp`**

```cpp
#include <M5Unified.h>

int counter = 0;
const char* lastBtn = "none";

void draw() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(2);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.printf("Count: %d", counter);

  M5.Display.setCursor(4, 32);
  M5.Display.printf("Last: %s", lastBtn);

  int bat = M5.Power.getBatteryLevel();       // 0-100, or -1 if unsupported
  bool charging = M5.Power.isCharging();
  M5.Display.setCursor(4, 60);
  M5.Display.setTextColor(charging ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
  M5.Display.printf("Bat: %d%%%s", bat, charging ? " (chg)" : "");
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);
  draw();
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed()) {
    counter++;
    lastBtn = "A";
    Serial.println("BtnA pressed (which physical button is this?)");
    draw();
  }
  if (M5.BtnB.wasPressed()) {
    counter--;
    lastBtn = "B";
    Serial.println("BtnB pressed (which physical button is this?)");
    draw();
  }
  delay(10);
}
```

- [ ] **Step 2: Build**

Run:
```bash
pio run -e r3_buttons
```
Expected: `SUCCESS`.

- [ ] **Step 3: Flash and observe**

Run:
```bash
pio run -e r3_buttons -t upload -t monitor
```
Expected:
- LCD shows `Count: 0`, `Last: none`, and a battery line.
- Press each physical button: the counter goes up/down, `Last:` shows `A` or `B`, and serial logs the press. **Note which physical button is A vs B** — record it (we rely on it in Plan 2).
- The `Bat:` line shows a percentage. Plug/unplug USB and confirm the `(chg)` indicator + green color toggles.

- [ ] **Step 4: Resolve the battery open-item**

Observe the `Bat:` value:
- If it shows a plausible 0–100% that tracks charging → `M5.Power` supports the StickS3; nothing more to do.
- If it shows `-1` or an obviously wrong value → M5Unified doesn't expose the M5PM1 power chip on this board yet. Record this finding; Plan 2 will add the `m5stack/M5PM1` library (`lib_deps`) and read battery through it instead. Either way, **write the result into the plan-1 wrap-up notes below.**

- [ ] **Step 5: Commit**

```bash
git add src/r3_buttons/main.cpp
git commit -m "feat(r3): buttons + battery readout"
```

---

## Wrap-up & handoff to Plan 2

- [ ] **Record hardware findings** (append to this file under a new `## Findings` heading, then commit):
  - Which physical button = `BtnA`, which = `BtnB`.
  - Whether `M5.Power.getBatteryLevel()` works (true %) or needs the `M5PM1` library.
  - Whether `setRotation(1)` (240×135 landscape) is the orientation you want for the clock, or `setRotation(0)`.
  - The serial port path (`/dev/cu.usbmodem…`) for convenience.

- [ ] **Decide branch integration** via `superpowers:finishing-a-development-branch` (PR to a fresh GitHub repo, or keep local) — then start **Plan 2 (Weather Clock)**, which will be written using these findings.

---

## Self-Review notes (author)

- **Spec coverage:** Covers rungs 0–3 (the "basics" half of the ladder). Rungs 4–7 are deferred to Plan 2 by explicit decomposition. ✅
- **No host tests in this plan** is intentional and stated — all code is hardware I/O. The spec's TDD/host-test strategy applies to Plan 2's pure parser. ✅
- **Open items** from the spec (battery API, button mapping, RTC/orientation) are turned into concrete on-device verification steps rather than left as guesses. ✅
- **Types/symbols** used (`M5.config`, `M5.begin`, `M5.Display`, `M5Canvas`, `M5.BtnA/BtnB`, `M5.Power.getBatteryLevel/isCharging`) are all real M5Unified API, consistent across tasks. ✅
