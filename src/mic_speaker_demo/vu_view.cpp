#include "vu_view.h"

void VuView::begin() {
  canvas_.createSprite(M5.Display.width(), M5.Display.height());
}

void VuView::render(const Frame& f) {
  canvas_.fillSprite(TFT_BLACK);

  // Status label.
  canvas_.setTextColor(f.color, TFT_BLACK);
  canvas_.setTextSize(2);
  canvas_.setCursor(4, 4);
  canvas_.print(f.status);

  // Bar: capture progress while recording, otherwise the live level.
  const int x = 4, y = 40, w = M5.Display.width() - 8, h = 28;
  canvas_.drawRect(x, y, w, h, TFT_DARKGREY);
  int val  = f.progressPct >= 0 ? f.progressPct : f.bar;
  int fill = (w - 2) * val / 100;
  uint16_t c = val < 60 ? TFT_GREEN : (val < 85 ? TFT_YELLOW : TFT_RED);
  canvas_.fillRect(x + 1, y + 1, fill, h - 2, c);
  if (f.progressPct < 0) {  // peak-hold tick only in live-VU mode
    int px = x + 1 + (w - 2) * f.peakHold / 100;
    canvas_.drawFastVLine(px, y + 1, h - 2, TFT_WHITE);
  }

  // Numeric readout.
  canvas_.setTextSize(1);
  canvas_.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas_.setCursor(4, y + h + 6);
  if (f.progressPct >= 0) canvas_.printf("rec %.1fs / %.0fs", f.recElapsed, f.recSeconds);
  else                    canvas_.printf("level %3d/100", f.bar);

  // Button hints.
  canvas_.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas_.setCursor(4, M5.Display.height() - 12);
  canvas_.print(f.recAvailable ? "A hold=rec  B=tone" : "A: --   B=tone");

  canvas_.pushSprite(0, 0);
}
