# Sticky — Tilt Marble Game (`marble_demo`) — Design

- **Date:** 2026-05-31
- **Device:** M5Stack StickS3 (ESP32-S3)
- **Goal:** A standalone demo rung that exercises the StickS3's **IMU (BMI270)** —
  a tilt-controlled rolling marble. Hold the stick flat like a tray; tilt it and
  a ball rolls across the screen with real momentum, bouncing off the walls. The
  player collects dots and dodges holes against a 60-second clock. Teaches reading
  the accelerometer, turning gravity into 2D physics, and a real-time game loop.
- **Author context:** Strong software developer, new to hardware. Optimize for
  learning the hardware dev loop; the language/concepts are familiar.

The IMU was explicitly parked as a "future rung" in the weather-clock design
(§9 Scope: "IMU/shake gestures"). This is that rung — a self-contained experiment
alongside `flicker_demo` and `mic_speaker_demo`, **not** part of the clock/weather
ladder.

## 1. Hardware

| Capability | Detail |
|---|---|
| IMU | **BMI270** 6-axis (accel + gyro), on the internal I²C bus (SCL G48 / SDA G47) |
| Access | `M5.Imu.getAccel(&ax, &ay, &az)` → acceleration in **g** (1.0 g = gravity). Only the accelerometer is needed; the gyro is unused. |
| Display | 1.14" LCD, 135 × 240, used at `setRotation(1)` = **240 × 135 landscape** |
| Buttons | KEY1 (G11) = `BtnA`, KEY2 (G12) = `BtnB` |
| PSRAM | 8 MB OPI (the 240×135×16bpp ≈ 64 KB sprite fits easily in internal RAM regardless) |

**M5Unified support:** `M5.Imu` is initialised automatically by `M5.begin()` when
`cfg.internal_imu` is true (the default), wired to the BMI270 on the internal I²C
bus. No manual driver needed.

**Axis mapping is mount-dependent (confirm on first flash).** Which physical accel
axis (ax/ay/az) maps to screen-x and screen-y, and the sign of each, depends on how
the BMI270 is mounted on the board. This is resolved empirically on the first flash
and recorded as constants — the same finding pattern as the `BtnA`/`BtnB` mapping in
`mic_speaker_demo`.

## 2. Locked decisions (from brainstorming)

| Decision | Choice |
|---|---|
| Game | Tilt-controlled marble: **collect dots, dodge holes** |
| Hold posture | **Flat like a tray**, screen up; "level" calibrated at boot |
| Physics | **Momentum + bounce** — tilt = gravity that accelerates the ball; velocity persists (inertia); friction bleeds speed; walls bounce with damping |
| Round loop | **Beat the clock** — 60 s countdown; eat a dot → score +1 & it respawns; **hit a hole → time penalty** (not instant game over); run ends at 0:00 |
| Architecture | **Pure game core in `lib/marble/`, thin device shell** (mirrors `lib/weather`, `lib/audio`) |
| Env / folder | **`marble_demo`** |
| Stretch goal | **Procedural rolling animation** — spin a highlight on the ball, driven by velocity |

## 3. Architecture

A new PlatformIO environment that inherits the shared `[device]` base, isolated by
`build_src_filter` exactly like every other rung. The `native` test env needs **no
change**: PlatformIO's Library Dependency Finder compiles `lib/marble/` as soon as
`test/test_marble/` `#include`s it — the same mechanism that already pulls in
`lib/weather` and `lib/audio`.

```
lib/marble/marble.{h,cpp}        PURE physics + game logic (no M5/Arduino/WiFi)  ← host-tested
src/marble_demo/imu_input.{h,cpp} device: read M5.Imu.getAccel, subtract boot calibration → tilt vector
src/marble_demo/board_view.{h,cpp} device: render GameState into one full-screen M5Canvas → pushSprite
src/marble_demo/app.{h,cpp}       the millis()-paced state machine; owns input + view + GameState
src/marble_demo/main.cpp          thin wiring: App app; setup(){app.begin();} loop(){app.loop();}
test/test_marble/test_marble.cpp  Unity host tests for lib/marble
platformio.ini                    + one [env:marble_demo] block
```

```ini
[env:marble_demo]
extends = device
build_src_filter = -<*> +<marble_demo/>
```

### The pure-logic-vs-hardware split

`lib/marble` depends only on the C++ standard library (`<cstdint>`, `<cstddef>`,
`<cmath>`) — no Arduino, no M5, no WiFi headers — so it compiles and runs on the
host. The deliberate seam: **the fun, fiddly part (physics feel, collisions,
scoring, timing) is pure and unit-tested**; the device-only IMU read, rendering,
and buttons stay thin. Same pattern as `weather_parse` vs `weather_client` and
`lib/audio` vs `audio_io`.

The single entry point is a deterministic step function:

```cpp
namespace marble {

struct Vec2 { float x, y; };

struct Ball { Vec2 pos; Vec2 vel; };

struct Dot  { Vec2 pos; bool active; };
struct Hole { Vec2 pos; };

enum class Phase { Calibrate, Playing, GameOver };

struct Config {           // all tunables; sane defaults below
  float width, height;    // playfield size in px (board area, not full screen)
  float ballR, dotR, holeR;
  float gravity;          // accel gain: px/s^2 per g of tilt
  float damping;          // velocity decay per second (frame-rate independent)
  float restitution;      // wall bounce energy retained (0..1)
  float roundSeconds;     // 60
  float holePenaltySec;   // 3
  int   numDots;          // 4 active at once
  int   numHoles;         // 3 fixed for the round
};

struct GameState {
  Phase phase;
  Ball  ball;
  Dot   dots[MAX_DOTS];
  Hole  holes[MAX_HOLES];
  int   score;
  float timeLeft;
  uint32_t rng;           // deterministic PRNG state for spawn placement
  // transient render hints: holeFlashMs (just-hit feedback), rollAngle (stretch)
};

// Reset to a fresh round: place holes + dots via the seeded PRNG, ball centred.
void reset(GameState&, const Config&, uint32_t seed);

// Advance one frame. `tilt` is the calibrated gravity vector (g units, level = 0).
// Integrates physics, bounces off walls, eats dots (respawning them), applies hole
// penalties (resetting the ball), counts the timer down, flips to GameOver at 0.
void step(GameState&, const Config&, Vec2 tilt, float dt);

}  // namespace marble
```

Spawn placement uses a small **deterministic PRNG** (e.g. xorshift) seeded from
`reset()`, so host tests are reproducible and the device can seed it from
`micros()` at the first button press for variety.

## 4. Components

### `lib/marble` (pure, unit-tested)

The physics integrator and all game rules. Broken into small pure helpers so each
is testable in isolation:

- `integrate(ball, tilt, cfg, dt)` — `vel += gravity*tilt*dt; vel *= exp(-damping*dt); pos += vel*dt`.
  Damping is a per-*second* exponential decay (not a per-step multiplier), so the
  feel is independent of frame rate.
- `bounceWalls(ball, cfg)` — reflect the velocity component on any of the 4 edges,
  scale by `restitution`, clamp the ball inside.
- `eatDots(state, cfg)` — centre-distance test ball↔dot; on overlap score++ and
  respawn that dot at a new PRNG position clear of the ball.
- `applyHoles(state, cfg)` — centre-distance test ball↔hole; on overlap subtract
  `holePenaltySec` (clamped so `timeLeft ≥ 0`), reset the ball to centre with zero
  velocity, and set a brief flash so the same hole can't instantly re-trigger.
- `tickClock(state, dt)` — `timeLeft -= dt`; at `≤ 0` → `Phase::GameOver`.

`step()` calls these in order. All collisions are circle-vs-circle (centre distance
< sum of radii).

### `src/marble_demo/` (device, split by responsibility)

- **`ImuInput`** (`imu_input.{h,cpp}`) — owns calibration. `calibrate()` samples
  `M5.Imu.getAccel` a few times and stores the resting `(ax0, ay0)` as the zero.
  `read() → Vec2 tilt` returns the mount-corrected, calibration-subtracted gravity
  vector `(ax−ax0, ay−ay0)` mapped to screen axes (sign/axis constants resolved on
  first flash).
- **`BoardView`** (`board_view.{h,cpp}`) — pure presentation. Takes a `GameState`
  and draws one full-screen `M5Canvas` sprite, then `pushSprite(0,0)` (the repo's
  flicker-free convention). Holds no model state. Draws the HUD (timer + score),
  the board, holes, dots, the ball (+ spin highlight for the stretch goal), and the
  game-over overlay.
- **`App`** (`app.{h,cpp}`) — the non-blocking, `millis()`-paced state machine.
  Owns an `ImuInput`, a `BoardView`, a `marble::GameState` and `Config`. Computes
  `dt` from `millis()` deltas, reads tilt, calls `marble::step`, renders, and maps
  buttons to transitions.
- **`main.cpp`** — just `App app; setup(){ app.begin(); } loop(){ app.loop(); }`.

## 5. Behavior, buttons & state machine

Buttons (physical mapping confirmed on first flash, per the finding pattern):
`BtnA` (KEY1) = **start / restart**, `BtnB` (KEY2) = **re-calibrate level**.

```
BOOT ─▶ CALIBRATE ──(BtnA)──▶ PLAYING ──(timeLeft = 0)──▶ GAME_OVER ──(BtnA)──┐
          │  sample resting    │  60s countdown                                │
          │  accel as "level"  │  tilt → roll ball (momentum + wall bounce)    │
          │  "place flat,      │  eat dot   → score++, dot respawns            │
          │   BtnA to start"   │  hit hole  → −3s, ball resets, brief flash    │
          │                    │  shows ⏱ + score HUD                          │
          └────────────────────┴── BtnB anytime = re-capture "level" ◀─────────┘
```

- **CALIBRATE** — prompt "place flat, BtnA to start"; sample the accelerometer to
  fix the neutral/zero. `BtnA` → PLAYING (seeds the PRNG from `micros()`, `reset()`).
- **PLAYING** — each loop: `dt` from `millis()`, `tilt = imu.read()`,
  `marble::step(state, cfg, tilt, dt)`, render. `BtnB` re-calibrates (hand drift).
  Reaching `timeLeft = 0` → GAME_OVER.
- **GAME_OVER** — overlay with final score and **best-this-power-cycle**. `BtnA`
  → new round.

Why non-blocking: the loop never `delay()`s — physics is `millis()`-`dt` integrated
and rendering is the per-frame sprite push, matching the repo's render-loop
conventions.

## 6. Defaults & tunables

All live in a constants block in the rung (passed into `marble::Config`), easy to
tune empirically once flashed:

| Tunable | Default | Note |
|---|---|---|
| Round length | **60 s** | the clock |
| Hole penalty | **−3 s** | clamped so `timeLeft ≥ 0` |
| Active dots | **4** | respawn on eat |
| Holes | **3** | fixed for the round |
| Ball / dot / hole radius | sized for 240×135 | e.g. ~6 / ~3 / ~8 px |
| `gravity` (accel gain) | tune on device | px/s² per g of tilt |
| `damping` | ~1.5 / s | inertia + roll-on (exp decay) |
| `restitution` | ~0.6 | damped wall bounce |

**Best score is kept in RAM only** (resets on power cycle). Persisting it across
reboots would need NVS/`Preferences` — itself an unexplored peripheral — so it is a
noted out-of-scope follow-up, not part of v1.

## 7. Rendering (240 × 135 landscape)

Full frame drawn into an off-screen `M5Canvas`, pushed in one shot (flicker-free,
per `widget` / `flicker_demo`).

- **HUD** (top strip): `⏱ 42` (seconds left) and `● 12` (score).
- **Board**: the play area below the HUD; walls = its edges.
- **Holes**: dark filled circles with a ring; **dots**: small bright circles.
- **Ball**: filled circle with a highlight. **Stretch — procedural spin:**
  accumulate a roll angle from distance travelled (`∝ |vel|·dt / (2π·ballR)`,
  direction from the velocity vector) and rotate the highlight/a texture mark so the
  ball visibly *rolls* rather than slides. No sprite-sheet assets; cheap. (Fallback
  if procedural looks poor: pre-rendered sprite frames selected by roll phase —
  more authentic shading but needs asset + memory work. Out of v1 unless needed.)
- **Game-over overlay**: dimmed board + "TIME!" + final score + best + "BtnA: again".

## 8. Error handling

- **IMU not enabled / `getAccel` fails** → log to serial, show a status line, and
  fall back to zero tilt (ball sits still) so the UI never crashes or blanks.
- **Calibration sanity** — if the resting vector is implausible (board not flat),
  still proceed; `BtnB` lets the player re-zero at any time.
- **Numeric safety** — clamp `timeLeft ≥ 0`; clamp the ball inside the board every
  frame after integration so a large `dt` (e.g. a hiccup) can't tunnel it out.

## 9. Testing

- **Host unit tests** (`pio test -e native`, Unity) — `test/test_marble/`:
  - `integrate`: zero tilt + initial velocity → friction reduces speed each step;
    position advances by `vel·dt`.
  - `bounceWalls`: a ball crossing each edge reflects the correct velocity
    component, speed scaled by `restitution`, position clamped inside.
  - `eatDots`: ball overlapping a dot → `score++` and that dot respawns within
    bounds (and not on top of the ball).
  - `applyHoles`: overlap → `timeLeft` reduced by the penalty (clamped `≥ 0`), ball
    reset to centre with zero velocity, flash set.
  - `tickClock`: accumulated `dt` reduces `timeLeft`; crossing 0 flips to
    `Phase::GameOver`.
  - PRNG/`reset`: deterministic given a seed; all spawns within bounds.
- **Hardware behavior** (flash + observe) — tilt feel and `gravity`/`friction`/
  `restitution` tuning, the IMU axis/sign mapping, calibration, the rolling
  animation, and button feel. Verified by eye and hand, not automated.

## 10. Scope (YAGNI)

**In (v1):** flat-tray tilt control with boot calibration, momentum + damped
wall-bounce physics, dot collection + respawn, holes with a time penalty + ball
reset, a 60 s beat-the-clock round, game-over/restart, the HUD, host-tested game
core, and the procedural rolling-spin stretch goal.

**Out (future / notes):** persisting best score to NVS, maze/internal walls,
levels/progression, multiple balls, sound effects (the audio codec is a separate
rung), gyro-based control, sprite-sheet ball animation, difficulty ramp.

## 11. Open items to confirm during implementation

- The BMI270 **axis → screen mapping and signs** (which of ax/ay maps to screen x/y,
  and direction). Resolve on first flash; record as constants (reuse the finding
  pattern).
- Physical **`BtnA`/`BtnB` → KEY1/KEY2** mapping (reuse the `mic_speaker_demo`
  finding if already recorded).
- Empirical tuning of `gravity`, `friction`, `restitution`, and the radii for a
  good "marble" feel.
- Confirm `M5.Imu.getAccel` units/orientation at rest (assumed ~1 g on one axis when
  flat, ~0 on the in-plane axes).

## Findings (during implementation)

- **BMI270 axis → screen mapping confirmed.** Raw accel **x → screen x** and
  **y → screen y** (axes are *not* swapped). The X axis needed **inverting**
  (`SCREEN_X_SIGN = -1.0f`); without it, tilting left rolled the ball right. Y was
  correct as-is (`SCREEN_Y_SIGN = +1.0f`). Both directions match the tilt after
  the flip. (Resolves design §1 / §11 axis-mapping open item.)
- **Cross-compiler `Vec2` brace-init bug — caught only by the device build.** The
  `native` test env compiles with `-std=gnu++17`, where a struct with default
  member initializers is an aggregate, so `Vec2{x, y}` worked and all host tests
  passed. The Arduino/Xtensa device toolchain compiles in an older C++ mode where
  it is **not** an aggregate, so `Vec2{x, y}` failed to compile ("candidate
  expects 0 arguments"). Fixed by giving `Vec2` explicit constructors
  (`Vec2() = default; Vec2(float, float)`). Lesson: host-green ≠ device-green —
  compile the device build, don't just run host tests.
- **Dot-on-hole placement bug found in playtest.** The original `placeClear` only
  avoided the ball, so dots sometimes spawned on/under a hole (uncollectable / a
  free fall). Fixed with a dedicated `placeDot` that rejects candidates within
  `holeR + ballR + dotR` of any hole, used by both `reset()` and `eatDots()`
  respawn. Added host guards over 200/100 seeds (`test_dots_never_spawn_on_holes`,
  `test_eaten_dot_respawns_off_holes`).
- **Physics feel — defaults kept.** `gravity = 900`, `damping = 1.5/s`,
  `restitution = 0.6` felt like a marble on the first tuning pass; left unchanged.
- **Difficulty — defaults kept.** `numDots = 4`, `numHoles = 3`,
  `holePenaltySec = 3`, `roundSeconds = 60` played well.
- **Buttons.** `BtnA` (start / restart) and `BtnB` (re-calibrate level) behaved as
  intended; physical KEY1/KEY2 correspondence matches the M5Unified default (and
  the `mic_speaker_demo` finding).
- **Rolling-spin stretch goal — shipped.** A navy surface spot orbiting at the
  accumulated `rollAngle` (with a fixed specular highlight) reads convincingly as
  rolling; spot radius 2 / orbit factor 0.55 looked right without tuning.

## Sources

- M5Unified `Basic/Imu` example (`.pio/libdeps/*/M5Unified/examples/Basic/Imu/`).
- M5Unified `src/utility/IMU_Class.hpp` — `getAccel(float*,float*,float*)` API.
- StickS3 design doc — `docs/2026-05-30-sticky-weather-clock-design.md` (§1 IMU
  BMI270 on internal I²C; §9 parked IMU/shake gestures).
- Repo conventions — `src/widget/main.cpp` (sprite render loop),
  `docs/2026-05-31-sticky-mic-speaker-demo-design.md` (pure-lib split + finding
  pattern).
