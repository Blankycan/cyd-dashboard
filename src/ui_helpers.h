#pragma once
#include <lvgl.h>
#include <stdint.h>

// Shared LVGL widget factories and formatting helpers used across all panels.

lv_obj_t  *make_panel(lv_obj_t *parent, int x, int y, int w, int h);  // borderless bg panel
lv_obj_t  *make_bar(lv_obj_t *parent, int x, int y, int w, int h);    // themed 0–100 progress bar
void       make_hdiv(lv_obj_t *parent, int y, int x, int w);          // 1px horizontal divider
void       make_placeholder(lv_obj_t *parent, const char *text);      // centered dim label
lv_color_t pct_color(int pct);                                        // OK / WARN / ALERT based on threshold (70/90)
lv_color_t pct_col(int pct);                                          // same, but returns dim when pct < 0 (unavailable)
void       fmt_k(char *buf, size_t sz, int32_t n);                    // e.g. 1234 → "1.2k"
void       fmt_reset(char *buf, size_t sz, int secs);                 // countdown to rate-limit reset
