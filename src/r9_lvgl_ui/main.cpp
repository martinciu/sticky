#include <M5Unified.h>
#include <lvgl.h>

// ---------- LVGL <-> M5GFX plumbing (same as r8) ----------
static const uint32_t BUF_LINES = 40;
static uint8_t draw_buf[240 * BUF_LINES * 2];

static uint32_t tick_cb(void) { return millis(); }

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  M5.Display.startWrite();
  M5.Display.setAddrWindow(area->x1, area->y1, w, h);
  M5.Display.writePixels((uint16_t *)px_map, w * h, true);
  M5.Display.endWrite();
  lv_display_flush_ready(disp);
}

// ---------- Two physical buttons as an LVGL ENCODER ----------
// BtnA = one detent forward (move focus, or change value while editing).
// BtnB = the encoder's press (select a widget / enter+exit edit mode).
static void encoder_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  if (M5.BtnA.wasPressed())
    data->enc_diff = 1;
  data->state =
      M5.BtnB.isPressed() ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// ---------- widgets the event callbacks update ----------
static lv_obj_t *count_label;
static lv_obj_t *switch_label;
static lv_obj_t *bright_label;
static lv_obj_t *bright_bar;
static int counter = 0;

static void btn_cb(lv_event_t *e) {
  (void)e;
  counter++;
  lv_label_set_text_fmt(count_label, "Count: %d", counter);
}

static void switch_cb(lv_event_t *e) {
  lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
  bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
  lv_label_set_text_fmt(switch_label, "Switch: %s", on ? "ON" : "OFF");
}

static void slider_cb(lv_event_t *e) {
  lv_obj_t *s = (lv_obj_t *)lv_event_get_target(e);
  int v = lv_slider_get_value(s); // 0..10
  lv_label_set_text_fmt(bright_label, "Bright: %d/10", v);
  lv_bar_set_value(bright_bar, v, LV_ANIM_OFF);
  // map 0..10 -> 20..255 so the screen never goes fully dark
  M5.Display.setBrightness(map(v, 0, 10, 20, 255));
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  M5.Display.setRotation(1);

  lv_init();
  lv_tick_set_cb(tick_cb);

  lv_display_t *disp =
      lv_display_create(M5.Display.width(), M5.Display.height());
  lv_display_set_flush_cb(disp, flush_cb);
  lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  // input device (encoder) + a focus group it drives
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_ENCODER);
  lv_indev_set_read_cb(indev, encoder_read_cb);
  lv_group_t *group = lv_group_create();
  lv_indev_set_group(indev, group);

  // scrollable vertical layout; the encoder auto-scrolls to the focused widget
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(scr, 6, 0);
  lv_obj_set_style_pad_all(scr, 6, 0);

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "LVGL UI Demo");

  lv_obj_t *hint = lv_label_create(scr);
  lv_label_set_text(hint, "BtnA: next   BtnB: select");

  // --- counter + button ---
  count_label = lv_label_create(scr);
  lv_label_set_text(count_label, "Count: 0");
  lv_obj_t *btn = lv_button_create(scr);
  lv_obj_t *btn_lbl = lv_label_create(btn);
  lv_label_set_text(btn_lbl, "Count ++");
  lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, NULL);
  lv_group_add_obj(group, btn);

  // --- switch ---
  switch_label = lv_label_create(scr);
  lv_label_set_text(switch_label, "Switch: OFF");
  lv_obj_t *sw = lv_switch_create(scr);
  lv_obj_add_event_cb(sw, switch_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_group_add_obj(group, sw);

  // --- slider -> live screen brightness + a mirroring bar ---
  bright_label = lv_label_create(scr);
  lv_label_set_text(bright_label, "Bright: 5/10");
  lv_obj_t *slider = lv_slider_create(scr);
  lv_obj_set_width(slider, 180);
  lv_slider_set_range(slider, 0, 10);
  lv_slider_set_value(slider, 5, LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_group_add_obj(group, slider);

  bright_bar = lv_bar_create(scr);
  lv_obj_set_size(bright_bar, 180, 10);
  lv_bar_set_range(bright_bar, 0, 10);
  lv_bar_set_value(bright_bar, 5, LV_ANIM_OFF);

  Serial.println("LVGL UI demo ready");
}

void loop() {
  M5.update();
  lv_timer_handler(); // render + service input
  delay(5);
}
