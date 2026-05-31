#include <M5Unified.h>
#include <lvgl.h>

// LVGL partial draw buffer: a few rows of RGB565 (2 bytes/px). ~19 KB.
static const uint32_t BUF_LINES = 40;
static uint8_t draw_buf[240 * BUF_LINES * 2];

// LVGL asks "what time is it?" -> feed it millis().
static uint32_t tick_cb(void) { return millis(); }

// LVGL hands us a rendered rectangle; blit it to the LCD via M5GFX.
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  M5.Display.startWrite();
  M5.Display.setAddrWindow(area->x1, area->y1, w, h);
  M5.Display.writePixels((uint16_t *)px_map, w * h, true); // true = swap RGB565 byte order
  M5.Display.endWrite();
  lv_display_flush_ready(disp);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1); // 240x135 landscape

  lv_init();
  lv_tick_set_cb(tick_cb);

  lv_display_t *disp =
      lv_display_create(M5.Display.width(), M5.Display.height());
  lv_display_set_flush_cb(disp, flush_cb);
  lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  // --- a tiny demo UI on the active screen ---
  lv_obj_t *scr = lv_screen_active();

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "LVGL on StickS3");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t *spinner = lv_spinner_create(scr);
  lv_obj_set_size(spinner, 70, 70);
  lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 12);
  lv_spinner_set_anim_params(spinner, 1000, 60);

  Serial.println("LVGL ready");
}

void loop() {
  M5.update();
  lv_timer_handler(); // let LVGL render + animate
  delay(5);
}
