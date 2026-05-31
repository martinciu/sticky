// Latin font browser -- cycle through M5GFX's Latin GFX + bitmap typefaces,
// one representative per typeface (no size/weight dupes). BtnA = next font,
// BtnB = prev (both wrap). Each reveal slides in with a spring animation and a
// freshly-rolled random color scheme.
//
// Each sample line shows a long string (full A-Z / a-z / digits+punctuation),
// left-aligned. Any line wider than the screen scrolls left until its last
// glyph shows, then back to the first, ping-ponging in a loop so every glyph is
// readable. The lines are also spread down the card to fill the height.
//
// Rendering: the whole frame is composited into an off-screen M5Canvas (sprite)
// and pushed in one shot -- the flicker-free path the repo reserves for
// per-frame animation (see CLAUDE.md / src/widget). Because the marquee always
// moves, the loop renders every frame (time-based, so speed is frame-rate
// independent), not just during transitions.

#include <M5Unified.h>
#include <math.h>
#include "esp_random.h"

// ---- the font roster -------------------------------------------------------
// All font objects below derive from lgfx::IFont, so a single pointer type
// holds GFX, GLCD, BMP, RLE and fixed-bitmap faces alike. `numeric` flags the
// numeral-only bitmap faces (Font6 / Font7) so they get a digit/clock sample
// instead of alphabetic text they can't render.
struct FontEntry {
  const lgfx::IFont *font;
  const char *name;
  bool numeric;
};

static const FontEntry FONTS[] = {
    // --- GFX (Adafruit-style) typefaces, smallest size that fits 3 lines ---
    {&fonts::TomThumb, "TomThumb", false},
    {&fonts::FreeMono9pt7b, "FreeMono9pt7b", false},
    {&fonts::FreeSans9pt7b, "FreeSans9pt7b", false},
    {&fonts::FreeSerif9pt7b, "FreeSerif9pt7b", false},
    {&fonts::Orbitron_Light_24, "Orbitron_Light_24", false},
    {&fonts::Roboto_Thin_24, "Roboto_Thin_24", false},
    {&fonts::Satisfy_24, "Satisfy_24", false},
    {&fonts::Yellowtail_32, "Yellowtail_32", false},
    {&fonts::DejaVu12, "DejaVu12", false},
    // --- legacy bitmap faces (size variants collapsed to one each) ---
    {&fonts::Font0, "Font0 (GLCD 5x7)", false},
    {&fonts::Font2, "Font2", false},
    {&fonts::Font6, "Font6 (numerals)", true},
    {&fonts::Font7, "Font7 (7-seg)", true},
    {&fonts::Font8x8C64, "Font8x8C64", false},
    {&fonts::AsciiFont8x16, "AsciiFont8x16", false},
};
static const int N_FONTS = sizeof(FONTS) / sizeof(FONTS[0]);

// Long showcase lines -- alphabetic faces get the full character set; numeral-
// only faces (Font6/Font7) get digits + clock + decimal (no letters/space, as
// those glyphs don't exist in those faces). Lines wider than the screen scroll.
static const char *TEXT_LINES[3] = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "abcdefghijklmnopqrstuvwxyz",
    "0123456789  .,:;!?@#&*-+/=",
};
static const char *NUM_LINES[3] = {
    "0123456789",
    "12:34:56:78:90",
    "3.14159265358",
};

// ---- layout / motion tunables ----------------------------------------------
static const int MARGIN_X = 4;             // left inset; text starts here
static const float SCROLL_SPEED = 48.0f;   // px/sec the text scrolls
static const float HOLD_SEC = 1.0f;        // pause at each end before reversing
static const float HOLD_PX = SCROLL_SPEED * HOLD_SEC;
// Underdamped spring (zeta ~ 0.6) -> a little overshoot, then settle.
static const float STIFFNESS = 220.0f;
static const float DAMPING = 18.0f;

// ---- color -----------------------------------------------------------------
struct Palette {
  uint16_t bg, fg, accent;
};

// HSV -> RGB565. h in [0,360), s/v in [0,1].
static uint16_t hsv565(float h, float s, float v) {
  float c = v * s;
  float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
  float m = v - c;
  float r = 0, g = 0, b = 0;
  if (h < 60) { r = c; g = x; }
  else if (h < 120) { r = x; g = c; }
  else if (h < 180) { g = c; b = x; }
  else if (h < 240) { g = x; b = c; }
  else if (h < 300) { r = x; b = c; }
  else { r = c; b = x; }
  return M5.Display.color565((uint8_t)((r + m) * 255), (uint8_t)((g + m) * 255),
                             (uint8_t)((b + m) * 255));
}

static float frand(float lo, float hi) {
  return lo + (esp_random() % 10000) / 10000.0f * (hi - lo);
}

// A harmonious-by-construction scheme: one random hue, a rich dark background
// and a light same-hue tint for the text -> always high-contrast and never a
// clashing random-on-random pair. The header gets a punchier tint as accent.
static Palette randomPalette() {
  float h = esp_random() % 360;
  Palette p;
  p.bg = hsv565(h, frand(0.55f, 0.78f), frand(0.16f, 0.30f));
  p.fg = hsv565(h, frand(0.10f, 0.20f), frand(0.92f, 1.00f));
  p.accent = hsv565(h, frand(0.40f, 0.55f), frand(0.85f, 0.95f));
  return p;
}

// ---- state -----------------------------------------------------------------
static M5Canvas canvas(&M5.Display);
static int W, H;

static int curIdx = 0;       // resting font
static Palette curPal;       // resting palette

static bool animating = false;
static int fromIdx, toIdx;   // cards in flight
static Palette fromPal, toPal;
static int dir;              // +1 = slide up (next), -1 = slide down (prev)
static float springX, springV;

static float scrollPx = 0.0f; // marquee travel, advanced every frame
static uint32_t lastFrameUs;

// ---- rendering -------------------------------------------------------------
// Draw one sample line at vertical position y in the current font/color, always
// left-aligned. If it fits, it just sits at the left. If it's wider than the
// screen it ping-pongs: hold showing the start, scroll left until the last
// glyph is in view, hold, then scroll back to the start -- looping.
static void drawScrollLine(const char *s, int y) {
  int w = canvas.textWidth(s);
  int visibleW = W - 2 * MARGIN_X;
  int off = 0;
  if (w > visibleW) {
    float maxOff = w - visibleW;             // px needed to reveal the end
    float cycle = 2.0f * (HOLD_PX + maxOff); // hold+left + hold+right
    float p = fmodf(scrollPx, cycle);
    if (p < HOLD_PX)
      off = 0;                               // holding on the start
    else if (p < HOLD_PX + maxOff)
      off = (int)(p - HOLD_PX);              // scrolling left to the end
    else if (p < 2 * HOLD_PX + maxOff)
      off = (int)maxOff;                     // holding on the end
    else
      off = (int)(maxOff - (p - (2 * HOLD_PX + maxOff))); // scrolling back
  }
  canvas.setCursor(MARGIN_X - off, y);
  canvas.print(s);
}

// Draw one font's "card" -- a full-screen-tall band starting at yTop. During a
// transition two of these are drawn, tiled exactly H apart so they slide as a
// seamless pair.
static void drawCard(int idx, int yTop, const Palette &pal) {
  canvas.fillRect(0, yTop, W, H, pal.bg);
  canvas.setTextSize(1);

  // Header in a fixed, always-legible face so you know which font you're on
  // even when the sample is hard to read or numeric-only.
  canvas.setFont(&fonts::Font2);
  canvas.setTextColor(pal.accent);
  char hdr[48];
  snprintf(hdr, sizeof(hdr), "[%d/%d] %s", idx + 1, N_FONTS, FONTS[idx].name);
  canvas.setCursor(4, yTop + 3);
  canvas.print(hdr);
  int headerH = canvas.fontHeight() + 7;
  canvas.drawFastHLine(0, yTop + headerH - 3, W, pal.accent);

  // Sample lines in the font itself: fit as many of the 3 as the face's line
  // height allows (big faces like Font6 show 2), spread evenly down the card.
  canvas.setFont(FONTS[idx].font);
  canvas.setTextColor(pal.fg);
  const char *const *lines = FONTS[idx].numeric ? NUM_LINES : TEXT_LINES;
  int lh = canvas.fontHeight();
  int avail = H - headerH - 2;
  int maxLines = avail / lh;
  if (maxLines > 3) maxLines = 3;
  if (maxLines < 1) maxLines = 1;
  int slotH = avail / maxLines;
  for (int i = 0; i < maxLines; ++i) {
    int y = yTop + headerH + i * slotH + (slotH - lh) / 2;
    drawScrollLine(lines[i], y);
  }
}

// Composite a frame: two sliding cards mid-transition, otherwise the resting
// one. fillScreen with the incoming bg covers any overshoot gap invisibly.
static void renderScene() {
  canvas.fillScreen(animating ? toPal.bg : curPal.bg);
  if (animating) {
    int fromY = (int)lroundf(-dir * springX * H);
    int toY = fromY + dir * H; // exactly H apart -> seamless tile
    drawCard(fromIdx, fromY, fromPal);
    drawCard(toIdx, toY, toPal);
  } else {
    drawCard(curIdx, 0, curPal);
  }
  canvas.pushSprite(0, 0);
}

// ---- transitions -----------------------------------------------------------
static void startTransition(int newDir) {
  if (animating) { // snap the in-flight card to rest, then slide on from it
    curIdx = toIdx;
    curPal = toPal;
  }
  fromIdx = curIdx;
  fromPal = curPal;
  dir = newDir;
  toIdx = (curIdx + dir + N_FONTS) % N_FONTS;
  toPal = randomPalette(); // fresh color every reveal
  springX = 0;
  springV = 0;
  animating = true;
  Serial.printf("-> [%d/%d] %s\n", toIdx + 1, N_FONTS, FONTS[toIdx].name);
}

// ---- arduino ---------------------------------------------------------------
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1); // 240x135 landscape

  W = M5.Display.width();
  H = M5.Display.height();
  canvas.setColorDepth(16);
  if (!canvas.createSprite(W, H)) {
    Serial.println("createSprite failed -- out of memory");
  }
  // Don't wrap long lines onto a new row -- they run off-screen on one line and
  // the ping-pong scroll reveals the overflow. (Wrap is on by default.)
  canvas.setTextWrap(false, false);

  curIdx = 0;
  curPal = randomPalette();
  lastFrameUs = micros();
  Serial.printf("font_demo: %d fonts. BtnA=next  BtnB=prev\n", N_FONTS);
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed()) startTransition(+1); // next
  if (M5.BtnB.wasPressed()) startTransition(-1); // prev

  // One time base drives both the marquee and the spring, so motion stays
  // frame-rate independent (pushSprite paces us at ~50-70 fps).
  uint32_t now = micros();
  float dt = (now - lastFrameUs) / 1e6f;
  lastFrameUs = now;
  if (dt > 0.04f) dt = 0.04f; // clamp after a stall

  scrollPx += SCROLL_SPEED * dt;

  if (animating) {
    float accel = STIFFNESS * (1.0f - springX) - DAMPING * springV;
    springV += accel * dt;
    springX += springV * dt;
    if (fabsf(1.0f - springX) < 0.004f && fabsf(springV) < 0.02f) {
      curIdx = toIdx; // settled
      curPal = toPal;
      animating = false;
    }
  }

  renderScene();
  delay(2); // yield to FreeRTOS / USB CDC
}
