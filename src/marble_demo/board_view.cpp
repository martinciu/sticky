#include "board_view.h"

// The board area is drawn below a small HUD strip. These origins plus
// cfg.width/height must keep the board on-screen (240x135 landscape).
static constexpr int BOARD_X = 4;
static constexpr int BOARD_Y = 26;

void BoardView::begin() {
  canvas_.createSprite(M5.Display.width(), M5.Display.height());
}

void BoardView::render(const marble::GameState& s, const marble::Config& cfg, int best) {
  (void)best;  // unused until the game-over overlay (Task 7)
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

  // Board frame.
  canvas_.drawRect(BOARD_X - 1, BOARD_Y - 1,
                   (int)cfg.width + 2, (int)cfg.height + 2, TFT_DARKGREY);

  // Ball + static specular highlight.
  int bx = BOARD_X + (int)s.ball.pos.x;
  int by = BOARD_Y + (int)s.ball.pos.y;
  canvas_.fillCircle(bx, by, (int)cfg.ballR, TFT_WHITE);
  canvas_.fillCircle(bx - 2, by - 2, 2, TFT_LIGHTGREY);

  // Debug readout (helps confirm the axis mapping on first flash): tilt the
  // right edge down -> vel.x and pos.x should grow, and the ball moves right.
  canvas_.setTextSize(1);
  canvas_.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas_.setCursor(4, 4);
  canvas_.printf("vel %4d,%4d", (int)s.ball.vel.x, (int)s.ball.vel.y);
  canvas_.setCursor(4, 14);
  canvas_.printf("pos %3d,%3d", (int)s.ball.pos.x, (int)s.ball.pos.y);

  canvas_.pushSprite(0, 0);
}
