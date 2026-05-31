#include "board_view.h"
#include <cmath> // sinf/cosf for the rolling-spin animation

// The board area is drawn below a small HUD strip. These origins plus
// cfg.width/height must keep the board on-screen (240x135 landscape).
static constexpr int BOARD_X = 4;
static constexpr int BOARD_Y = 26;

void BoardView::begin() {
  canvas_.createSprite(M5.Display.width(), M5.Display.height());
}

// Draw a recessed hole as a smooth gradient funnel with motion parallax. Many
// concentric rings fade from a lit rim to a small near-black core (t*t falloff,
// so the dark centre stays small); each deeper ring is shifted further toward
// (ox, oy) so the shaft leans with the tilt. The outermost ring stays centred on
// the opening, so the dark can never spill past the rim however far you tilt.
static void drawHole(M5Canvas &c, int hx, int hy, int hr, int ox, int oy) {
  for (int r = hr; r >= 1; --r) {
    float t = (float)r / hr;            // 1 at rim .. ~0 at centre
    uint8_t v = (uint8_t)(100 * t * t); // lit rim -> small black core
    int sx = (int)(ox * (1.0f - t));    // deeper rings lean toward the tilt
    int sy = (int)(oy * (1.0f - t));
    c.fillCircle(hx + sx, hy + sy, r, c.color565(v, v, v));
  }
  c.drawCircle(hx, hy, hr, c.color565(14, 16, 22)); // crisp opening edge
}

void BoardView::render(const marble::GameState &s, const marble::Config &cfg,
                       int best, marble::Vec2 view, int batt, bool charging) {
  const uint16_t BOARD_COLOR = canvas_.color565(36, 44, 58); // slate tray
  const uint16_t BEVEL_LIGHT = canvas_.color565(90, 100, 122);
  const uint16_t BEVEL_DARK = canvas_.color565(12, 16, 24);
  const uint16_t DOT_COLOR = canvas_.color565(255, 205, 60); // gold coin
  const uint16_t DOT_GLINT = canvas_.color565(255, 245, 205);

  canvas_.fillSprite(TFT_BLACK);
  canvas_.setFont(&fonts::Font8x8C64); // 8-bit (Commodore 64) HUD font

  if (s.phase == marble::Phase::Calibrate) {
    canvas_.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas_.setTextSize(2);
    canvas_.setCursor(8, 30);
    canvas_.print("LAY FLAT");
    canvas_.setTextSize(1);
    canvas_.setCursor(8, 60);
    canvas_.print("BTNA:START");
    canvas_.pushSprite(0, 0);
    return;
  }

  const int W = (int)cfg.width;
  const int H = (int)cfg.height;

  // HUD: time left (red under 10 s) on the left, score on the right.
  canvas_.setTextSize(2);
  canvas_.setTextColor(s.timeLeft <= 10.0f ? TFT_RED : TFT_WHITE, TFT_BLACK);
  canvas_.setCursor(4, 4);
  canvas_.printf("%4.1f", s.timeLeft);
  canvas_.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas_.setCursor(150, 4);
  canvas_.printf("S%d", s.score);

  // Battery readout (visible on battery power, where there's no serial monitor).
  canvas_.setTextSize(1);
  uint16_t bc = charging      ? TFT_GREENYELLOW
                : (batt < 0)  ? TFT_DARKGREY
                : (batt < 20) ? TFT_RED
                              : TFT_WHITE;
  canvas_.setTextColor(bc, TFT_BLACK);
  canvas_.setCursor(92, 9);
  if (batt < 0)
    canvas_.print(charging ? "CHG" : "BAT?");
  else
    canvas_.printf("%d%%%s", batt, charging ? "+" : "");

  // Board surface + raised bevel (light top-left, dark bottom-right). The bevel
  // flashes red briefly after a hole hit.
  canvas_.fillRect(BOARD_X, BOARD_Y, W, H, BOARD_COLOR);
  uint16_t lip = s.holeFlash > 0.0f ? TFT_RED : BEVEL_LIGHT;
  uint16_t sha = s.holeFlash > 0.0f ? TFT_RED : BEVEL_DARK;
  canvas_.drawFastHLine(BOARD_X - 1, BOARD_Y - 1, W + 2, lip);
  canvas_.drawFastVLine(BOARD_X - 1, BOARD_Y - 1, H + 2, lip);
  canvas_.drawFastHLine(BOARD_X - 1, BOARD_Y + H, W + 2, sha);
  canvas_.drawFastVLine(BOARD_X + W, BOARD_Y - 1, H + 2, sha);

  // Holes (recessed, with motion parallax). The whole board tilts uniformly, so
  // every funnel leans by the same tilt-driven offset -- that lean is the depth
  // cue. VIEW_SIGN flips direction; PARALLAX scales it; maxoff caps the lean.
  const float VIEW_SIGN = 1.0f;
  const float PARALLAX = cfg.holeR * 1.6f;  // px of lean per g of tilt
  const int hr = (int)cfg.holeR;
  const int maxoff = (hr * 6) / 10;         // cap so the funnel stays sensible
  int ox = (int)(VIEW_SIGN * view.x * PARALLAX);
  int oy = (int)(VIEW_SIGN * view.y * PARALLAX);
  if (ox > maxoff) ox = maxoff;
  if (ox < -maxoff) ox = -maxoff;
  if (oy > maxoff) oy = maxoff;
  if (oy < -maxoff) oy = -maxoff;
  for (int i = 0; i < cfg.numHoles && i < marble::MAX_HOLES; ++i) {
    drawHole(canvas_, BOARD_X + (int)s.holes[i].pos.x,
             BOARD_Y + (int)s.holes[i].pos.y, hr, ox, oy);
  }

  // Dots: gold coins with a glint.
  for (int i = 0; i < cfg.numDots && i < marble::MAX_DOTS; ++i) {
    if (!s.dots[i].active)
      continue;
    int dx = BOARD_X + (int)s.dots[i].pos.x;
    int dy = BOARD_Y + (int)s.dots[i].pos.y;
    canvas_.fillCircle(dx, dy, (int)cfg.dotR, DOT_COLOR);
    canvas_.fillCircle(dx - 1, dy - 1, 1, DOT_GLINT);
  }

  // Ball with procedural spin: a fixed specular highlight (light from top-left)
  // plus a surface spot that orbits at the accumulated rollAngle, so the ball
  // visibly rotates faster the faster it moves.
  int bx = BOARD_X + (int)s.ball.pos.x;
  int by = BOARD_Y + (int)s.ball.pos.y;
  canvas_.fillCircle(bx, by, (int)cfg.ballR, TFT_WHITE);
  float orbit = cfg.ballR * 0.55f;
  int spx = bx + (int)(orbit * cosf(s.rollAngle));
  int spy = by + (int)(orbit * sinf(s.rollAngle));
  canvas_.fillCircle(spx, spy, 2, TFT_NAVY);            // rolling surface spot
  canvas_.fillCircle(bx - 2, by - 2, 2, TFT_LIGHTGREY); // fixed highlight

  // Game-over overlay on a rounded panel so the text reads over the board.
  if (s.phase == marble::Phase::GameOver) {
    int pw = 150, ph = 66;
    int px = (M5.Display.width() - pw) / 2;
    int py = (M5.Display.height() - ph) / 2;
    canvas_.fillRoundRect(px, py, pw, ph, 5, TFT_BLACK);
    canvas_.drawRoundRect(px, py, pw, ph, 5, BEVEL_LIGHT);
    canvas_.setTextSize(2);
    canvas_.setTextColor(TFT_RED, TFT_BLACK);
    canvas_.setCursor(px + 12, py + 8);
    canvas_.print("TIME!");
    canvas_.setTextSize(1);
    canvas_.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas_.setCursor(px + 12, py + 30);
    canvas_.printf("SCORE %d", s.score);
    canvas_.setCursor(px + 12, py + 42);
    canvas_.printf("BEST  %d", best);
    canvas_.setCursor(px + 12, py + 54);
    canvas_.print("BTNA:AGAIN");
  }

  canvas_.pushSprite(0, 0);
}
