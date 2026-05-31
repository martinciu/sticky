# Sticky — Mic + Speaker Demo (`mic_speaker_demo`) — Design

- **Date:** 2026-05-31
- **Device:** M5Stack StickS3 (ESP32-S3)
- **Goal:** A standalone demo rung that exercises the StickS3's audio hardware —
  a live VU meter from the microphone, record-then-playback, and a button-played
  tone — teaching the one non-obvious fact about this board: **the mic and the
  speaker share a single ES8311 codec, so only one can be active at a time.**
- **Author context:** Strong software developer, new to hardware. Optimize for
  learning the hardware dev loop; the language/concepts are familiar.

Audio was explicitly parked as a "future rung" in the weather-clock design
(§9 Scope). This is that rung — a self-contained experiment alongside
`flicker_demo`, not part of the clock/weather ladder.

## 1. Hardware

| Capability | Detail |
|---|---|
| Audio codec | **ES8311** (I²C control + I²S data) |
| Microphone | onboard MEMS mic → ES8311 ADC |
| Speaker amp | AW8737 → ES8311 DAC |
| Audio pins | MCLK G18, DOUT G14, BCLK G17, LRCK G15, DIN G16 (M5Unified handles these) |
| Buttons | KEY1 (G11) = `BtnA`, KEY2 (G12) = `BtnB` |
| PSRAM | 8 MB OPI (plenty of room for a record buffer) |

**M5Unified support confirmed in-tree:** `M5Unified.cpp` has board-specific
`_microphone_enabled_cb_sticks3` / `_speaker_enabled_cb_sticks3` callbacks that
program the ES8311 registers, and `board_M5StickS3` is wired into both the
`M5.Mic` and `M5.Speaker` init paths. So `M5.Mic` and `M5.Speaker` work out of
the box; no manual codec driver needed.

**The central constraint:** the mic ADC and speaker DAC sit behind the same
ES8311 / I²S peripheral. M5Unified's own `Basic/Microphone` example spells it
out: *"Since the microphone and speaker cannot be used at the same time, turn
off the speaker here"* (`M5.Speaker.end(); M5.Mic.begin();`). This demo makes
that handoff the explicit lesson.

## 2. Locked decisions (from brainstorming)

| Decision | Choice |
|---|---|
| Demo behavior | **VU meter + record/playback combo** (idle VU; one button records→plays back; another plays a tone) |
| Testing seam | **Extract pure level math to `lib/audio/` with host unit tests** (mirrors `lib/weather`) |
| Env / folder name | **`mic_speaker_demo`** |
| Loop structure | **Approach A** — non-blocking state machine, sprite rendering (matches repo conventions) |

## 3. Architecture

A new PlatformIO environment that inherits the shared `[device]` base, isolated
by `build_src_filter` exactly like every other rung. The `native` test env needs
**no change**: PlatformIO's Library Dependency Finder compiles `lib/audio/` as
soon as `test/test_audio/` `#include`s it — the same mechanism that already pulls
in `lib/weather`.

```
lib/audio/audio.h, audio.cpp     PURE level math (no M5/Arduino/WiFi)  ← host-tested
src/mic_speaker_demo/main.cpp    thin hardware wrapper: mic, speaker, codec toggle, render
test/test_audio/test_audio.cpp   Unity host tests for lib/audio
platformio.ini                   + one [env:mic_speaker_demo] block
```

```ini
[env:mic_speaker_demo]
extends = device
build_src_filter = -<*> +<mic_speaker_demo/>
```

### The pure-logic-vs-hardware split

`lib/audio` depends only on the C++ standard library (`<cstdint>`, `<cstddef>`,
`<cmath>`) — no Arduino, no M5, no WiFi headers — so it compiles and runs on the
host. The device-only mic capture, speaker output, and ES8311 toggle stay thin
in `main.cpp`, wrapping the pure level math. Same pattern as
`weather_parse` vs `weather_client`.

## 4. Components

### `lib/audio` (pure, unit-tested)

```cpp
namespace audio {

struct Level {
  float   rms;    // root-mean-square of the block (linear, 0..32767)
  int16_t peak;   // max |sample| in the block (0..32767)
  float   dbfs;   // peak in dBFS (0 dB = full scale; <= 0)
  uint8_t bar;    // 0..100, ready to drive a VU bar
};

// Compute RMS + peak over a block of mono int16 samples, derive dBFS from the
// peak, and map it to a 0..100 bar via dbToBar(). n == 0 yields a zeroed Level.
Level computeLevel(const int16_t* samples, size_t n);

// Clamp/scale a dBFS value into a 0..100 bar. Default window -60..0 dBFS.
// <= floorDb -> 0, >= ceilDb -> 100, linear in between.
uint8_t dbToBar(float dbfs, float floorDb = -60.0f, float ceilDb = 0.0f);

}  // namespace audio
```

`dbToBar` is split out so its clamping/scaling is testable independently of the
RMS/peak arithmetic.

### `src/mic_speaker_demo/main.cpp` (thin hardware)

Responsibilities, each kept small:

- **Codec handoff helpers** — the heart of the lesson:
  - `enterMic()`  → `M5.Speaker.end(); M5.Mic.begin();`
  - `enterSpeaker()` → `M5.Mic.end(); M5.Speaker.begin();`
  Each carries the "mic and speaker can't run at once on the shared ES8311"
  comment. Every state transition that changes the audio direction goes through
  one of these, so the two are never active simultaneously.
- **Buffers** — a record buffer in **internal DMA-capable RAM**
  (`heap_caps_malloc(bytes, MALLOC_CAP_8BIT)` — the I2S DMA requires internal RAM;
  PSRAM is the wrong heap here and isn't enabled in this build — see Findings),
  **sized at runtime** to the free internal RAM minus a safety margin and capped
  at `MAX_REC_SECONDS`. A small static VU block buffer (256 samples) and one
  full-screen `M5Canvas` sprite reused every frame.
- **Render** — draw the whole frame into the sprite, then `pushSprite(0, 0)`
  (the repo's flicker-free convention).
- **Loop** — non-blocking, `millis()`-paced; the state machine below.

## 5. Behavior, buttons & state machine

Buttons (physical mapping confirmed on first flash, per the Plan-1 finding
pattern): `BtnA` (KEY1) = **record → playback**, `BtnB` (KEY2) = **tone**.

```
BOOT ─▶ IDLE_VU ──(BtnA press)──▶ RECORDING ──(BtnA release / buffer full)──▶ PLAYBACK ──┐
          ▲  │                                                                            │
          │  └────────────(BtnB)──▶ TONE ──(done)──┐                                      │
          └──────────────────────────────────────── ◀────────────────────────────────────┘

  codec owner:   IDLE_VU / RECORDING = Mic        PLAYBACK / TONE = Speaker
```

- **IDLE_VU** — each loop grabs a ~256-sample block via
  `M5.Mic.record(block, N, SAMPLE_RATE)`, runs `audio::computeLevel(block, N)`, and
  draws a colored VU bar (green → yellow → red zones) with the numeric level and
  a decaying peak-hold tick. `BtnA` press → RECORDING; `BtnB` → TONE.
- **RECORDING — push-to-talk, chunked.** On `BtnA` *press*, zero the buffer (so
  any unfilled tail is silent) and arm RECORDING. Each loop keeps up to **two
  small ~64 ms chunks** (`REC_CHUNK = 1024` samples) queued via
  `M5.Mic.record(buf + recPos, REC_CHUNK, SAMPLE_RATE)`, advancing `recPos`; the
  two-deep queue keeps the DMA fed so capture is gapless. Stops on **`BtnA`
  release** — detected on the button *level* (`!isPressed()`), not the one-shot
  `wasReleased()` edge — or when there's no room for another chunk (safety net).
  Played length is exactly `recPos`. Small chunks are essential: the mic task
  fills a record request *to completion* before re-checking its stop flag, so a
  single multi-second request would make `Mic.end()` block for the whole
  remaining capture (see Findings). → PLAYBACK (or IDLE_VU if the take was empty).
- **PLAYBACK** — `enterSpeaker()`, then
  `M5.Speaker.playRaw(buf, captured, SAMPLE_RATE)`; poll `M5.Speaker.isPlaying()`
  each loop (non-blocking). When done → `enterMic()` → IDLE_VU. Shows "PLAY".
- **TONE** — `enterSpeaker()`, `M5.Speaker.tone(…, 200)` (200 ms); poll
  done → `enterMic()` → IDLE_VU. Successive presses cycle C/E/G (523/659/784 Hz).

In `RECORDING` only the stop triggers (release / buffer-full) are handled; in the
other non-idle states (`PLAYBACK`/`TONE`) presses are ignored, so a transition is
never interrupted mid-flight and the codec handoff stays clean.

Why this stays non-blocking: both `record()` (DMA double-buffered) and
`playRaw()`/`tone()` (async DMA playback polled via `isPlaying()`) let the loop
keep rendering rather than stalling on `delay()`.

## 6. Buffers & memory

| Buffer | Size | Where |
|---|---|---|
| Record buffer | runtime-sized: `min(free − margin, MAX_REC_SECONDS)`; **32 KB/s @ 16 kHz** | internal DMA RAM (`MALLOC_CAP_8BIT`) |
| VU block | 256 samples (~16 ms @ 16 kHz) | static array |
| Frame sprite | 240 × 135 × 16bpp ≈ 64 KB | as in existing rungs |

Sample format: 16-bit signed mono at **16 kHz** (crisper than 8 kHz, ~8 kHz audio
bandwidth, at the cost of ~half the max clip length). Both capture and playback
use the same `SAMPLE_RATE`, so playback pitch stays correct. Speaker at max
volume (`SPK_VOLUME = 255`).

## 7. Error handling

- **Record-buffer alloc fails** (internal RAM exhausted) → log to serial, show an
  error line (`A: --`), and disable the record/playback path; VU and tone still
  work. Never crash.
- **`M5.Mic.begin()` / `M5.Speaker.begin()` returns false** → log it, show a
  status glyph, degrade gracefully (UI never blanks).
- **Codec safety** — the `enter*` helpers always `end()` the other device before
  `begin()`, so mic and speaker are never both active. The IDLE-only button gate
  guarantees no transition starts while another is in progress.

## 8. Testing

- **Host unit tests** (`pio test -e native`, Unity) — `test/test_audio/`:
  - `computeLevel` on **silence** (all zeros) → `peak 0`, `rms 0`, `dbfs` at floor,
    `bar 0`.
  - `computeLevel` on **full-scale** (all 32767) → `peak 32767`, `dbfs ≈ 0`,
    `bar 100`.
  - `computeLevel` on a **known-amplitude sine** → `rms ≈ A/√2` (within tolerance),
    `dbfs` ≈ expected.
  - `dbToBar` boundaries/clamps: `<= floor → 0`, `>= ceil → 100`, midpoint ≈ 50,
    out-of-range clamps.
- **Hardware behavior** (flash + observe) — real mic sensitivity, speaker volume,
  audible codec-toggle clicks, button feel. Verified by ear and eye, not automated.

## 9. Scope (YAGNI)

**In (v1):** live VU meter, push-to-talk record → playback (variable length),
button tone, the ES8311 codec toggle, host-tested level math.

**Out (future / notes):** FFT/spectrum view (the bundled `Advanced/Mic_FFT`
already covers it), saving audio to flash/SD, on-the-fly encoding (WAV/ADPCM),
gain or volume UI, stereo capture, wake-on-sound.

## 10. Open items to confirm during implementation

- Confirm which physical button is `BtnA` vs `BtnB` on first flash (reuse the
  Plan-1 finding if already recorded).
- Confirm the StickS3 mic default sample rate / format from `mic_config_t`
  (assumed 16 kHz, 16-bit mono).
- Pick a comfortable default `M5.Speaker.setVolume()` and mic→VU scaling window
  (`floorDb`/`ceilDb`) empirically once flashed.
- Verify the audible click on codec handoff is acceptable; if jarring, consider a
  brief mute/ramp (noted as a possible polish, out of v1 scope).

## Findings (during implementation)

- **Record buffer must be internal DMA RAM, not PSRAM.** The first on-device run
  printed `ERROR: record buffer alloc failed (PSRAM)` — `ps_malloc()` returned
  null, so BtnA (gated on `recBufOk`) did nothing. Root cause: the I2S DMA needs
  a DMA-capable internal-RAM buffer, and PSRAM isn't initialised in this build
  (`platformio.ini` sets `memory_type=qio_opi` but no `-DBOARD_HAS_PSRAM`). Fixed
  by allocating with `MALLOC_CAP_8BIT`, matching every M5Unified audio example.
- **PSRAM is not enabled project-wide (to confirm).** A boot diagnostic now prints
  `ESP.getPsramSize()`/`getFreePsram()`. If it reports 0, anything later assuming
  PSRAM (large sprites, TLS buffers in the widget rung) is silently falling back
  to internal RAM. If PSRAM is wanted, the generic `esp32-s3-devkitc-1` board
  likely needs `-DBOARD_HAS_PSRAM` (and possibly a proper board variant) on top
  of `memory_type=qio_opi`. **Out of scope for this rung; flagged as follow-up.**
- **Spurious boot I2S error fixed.** `E I2S: i2s_driver_uninstall: port 0 has not
  installed` came from `M5.Speaker.end()` running in `setup()` before the speaker
  was ever installed. `setup()` now calls `M5.Mic.begin()` directly; the runtime
  `enterMic()`/`enterSpeaker()` toggles are unaffected (the speaker is installed
  by then).
- **Recording quality / length.** Settled on **16 kHz / 16-bit mono** (crisper,
  ~8 kHz bandwidth) with the speaker at **max volume** (`SPK_VOLUME = 255`). The
  buffer is **runtime-sized** to free internal RAM (minus a 96 KB margin, capped
  at `MAX_REC_SECONDS = 20 s`) instead of a fixed 2 s, so it grabs the longest
  clip the board can safely hold (~32 KB/s at 16 kHz); the boot diagnostic reports
  the actual seconds. (8 kHz was tried for ~2× length but 16 kHz was preferred for
  fidelity.) Internal DMA RAM is the ceiling; for much longer clips (minutes) the
  path is enabling PSRAM + chunked streaming capture — a future-rung change.
- **Recording is push-to-talk (hold BtnA), captured in small chunks.** Two
  bugs surfaced and were fixed here:
  1. **Release wasn't detected.** The one-shot `wasReleased()` edge didn't reliably
     end a *held* take, so it ran to the buffer-full safety net. Switched to the
     button *level* (`!isPressed()`), checked every loop — robust for "while held".
  2. **`Mic.end()` blocked ~17 s.** The original design issued **one** big
     `M5.Mic.record(buf, recSamples, …)` for up to 20 s. The mic task fills a
     record request *to completion* before re-checking its stop flag, so
     `Mic.end()` (via `enterSpeaker()`) blocked until the entire remaining buffer
     was captured — measured `enterSpeaker=16974 ms` on a 2.95 s take. Rewrote
     capture to **~64 ms chunks, 2 queued** (gapless): `Mic.end()` now waits only
     for one tiny chunk (`enterSpeaker=34 ms`), played length is exactly `recPos`,
     and the buffer is zeroed beforehand so any unfilled trailing chunk is silent.
  (A "second press to stop" toggle is the easy alternative for hands-free takes.)
- **Pending on-device confirmation (re-flash):** physical BtnA/BtnB → KEY1/KEY2
  mapping; the `getPsramSize()` value; mic sensitivity / `dbToBar` window + `SPK_VOLUME`
  tuning; audible click on the codec handoff.

### Sources

- M5Unified `Basic/Microphone`, `Basic/Speaker`, `Advanced/Mic_FFT` examples
  (in `.pio/libdeps/*/M5Unified/examples/`).
- M5Unified `src/M5Unified.cpp` — StickS3 ES8311 enable callbacks.
- StickS3 design doc — `docs/2026-05-30-sticky-weather-clock-design.md` (§9 parked sound).
