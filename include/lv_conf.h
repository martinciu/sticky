// Minimal LVGL v9 config for the StickS3 (RGB565 display).
// Options not set here fall back to LVGL's internal defaults
// (components/lv_conf_internal.h), so this only overrides what we care about.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16          // RGB565 — matches M5GFX / the ST7789 panel
#define LV_USE_OS 0                // no RTOS integration; we call lv_timer_handler()

// LVGL's heap (widgets, styles). 48 KB is ample for a demo on the S3.
#define LV_MEM_SIZE (48 * 1024)

// Fonts
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_LOG 0               // quiet + smaller

#endif // LV_CONF_H
