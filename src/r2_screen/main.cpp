#include <M5Unified.h>

M5Canvas canvas(&M5.Display); // an off-screen buffer the size of the screen

int barX = 0;
int dir = 2;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);

  M5.Display.setRotation(1); // try 0 later to see portrait 135x240
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

  M5.Display.fillRect(4, 50, 60, 24, TFT_RED);    // filled rectangle
  M5.Display.drawRect(72, 50, 60, 24, TFT_GREEN); // outlined rectangle
  M5.Display.fillCircle(180, 62, 16, TFT_YELLOW); // filled circle
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
  if (barX <= 0 || barX >= M5.Display.width() - 30)
    dir = -dir;
  delay(16); // ~60 fps
}
