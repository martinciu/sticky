#include "board_view.h"

// The board area is drawn below a small HUD strip. These origins plus
// cfg.width/height must keep the board on-screen (240x135 landscape).
static constexpr int BOARD_X = 4;
static constexpr int BOARD_Y = 26;

void BoardView::begin() {
  canvas_.createSprite(M5.Display.width(), M5.Display.height());
}

void BoardView::render(const marble::GameState& s, const marble::Config& cfg, int best) {
  canvas_.fillSprite(TFT_BLACK);

  if (s.phase == marble::Phase::Calibrate) {
    canvas_.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas_.setTextSize(2);
    canvas_.setCursor(8, 30);
    canvas_.print("Lay flat");
    canvas_.setTextSize(1);
    canvas_.setCursor(8, 60);
    canvas_.print("BtnA: start");
    canvas_.pushSprite(0, 0);
    return;
  }

  // HUD: time left (left) + score (right-ish).
  canvas_.setTextSize(2);
  canvas_.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas_.setCursor(4, 4);
  canvas_.printf("%4.1f", s.timeLeft);
  canvas_.setCursor(150, 4);
  canvas_.printf("S%d", s.score);

  // Board frame (flashes red briefly after a hole hit).
  uint16_t frame = s.holeFlash > 0.0f ? TFT_RED : TFT_DARKGREY;
  canvas_.drawRect(BOARD_X - 1, BOARD_Y - 1,
                   (int)cfg.width + 2, (int)cfg.height + 2, frame);

  // Holes: dark wells with a red ring.
  for (int i = 0; i < cfg.numHoles && i < marble::MAX_HOLES; ++i) {
    int hx = BOARD_X + (int)s.holes[i].pos.x;
    int hy = BOARD_Y + (int)s.holes[i].pos.y;
    canvas_.fillCircle(hx, hy, (int)cfg.holeR, TFT_BLACK);
    canvas_.drawCircle(hx, hy, (int)cfg.holeR, TFT_RED);
  }

  // Dots: bright pickups.
  for (int i = 0; i < cfg.numDots && i < marble::MAX_DOTS; ++i) {
    if (!s.dots[i].active) continue;
    canvas_.fillCircle(BOARD_X + (int)s.dots[i].pos.x,
                       BOARD_Y + (int)s.dots[i].pos.y, (int)cfg.dotR, TFT_GREENYELLOW);
  }

  // Ball + static specular highlight.
  int bx = BOARD_X + (int)s.ball.pos.x;
  int by = BOARD_Y + (int)s.ball.pos.y;
  canvas_.fillCircle(bx, by, (int)cfg.ballR, TFT_WHITE);
  canvas_.fillCircle(bx - 2, by - 2, 2, TFT_LIGHTGREY);

  // Game-over overlay.
  if (s.phase == marble::Phase::GameOver) {
    canvas_.setTextSize(2);
    canvas_.setTextColor(TFT_RED, TFT_BLACK);
    canvas_.setCursor(40, 36);
    canvas_.print("TIME!");
    canvas_.setTextSize(1);
    canvas_.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas_.setCursor(40, 62);
    canvas_.printf("score %d", s.score);
    canvas_.setCursor(40, 74);
    canvas_.printf("best  %d", best);
    canvas_.setCursor(40, 86);
    canvas_.print("BtnA: again");
  }

  canvas_.pushSprite(0, 0);
}
