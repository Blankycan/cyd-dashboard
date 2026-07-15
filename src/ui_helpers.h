#pragma once
#include <lvgl.h>
#include <stdint.h>

// Shared LVGL widget factories and formatting helpers used across all panels.

// A labeled progress-bar row: key label (left), value label (near-left),
// extra/reset label (right-aligned), and the bar itself. Build with
// make_bar_row(); update the four fields directly in widget update functions.
struct BarRow {
    lv_obj_t *lbl_key;
    lv_obj_t *lbl_val;
    lv_obj_t *lbl_extra;
    lv_obj_t *bar;
};

lv_obj_t  *make_panel(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t bg);  // borderless bg panel
lv_obj_t  *make_bar(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t bg);    // themed 0–100 progress bar
void       make_hdiv(lv_obj_t *parent, int y, int x, int w, lv_color_t col);         // 1px horizontal divider
lv_obj_t  *make_dot(lv_obj_t *parent, int x, int y, lv_color_t col);                 // 6×6 circle dot
// Labeled bar row: key at (x,y) | val at (x+28,y) | extra right-aligned | bar at (x,y+14)
// All labels use montserrat_12; bar is 5px tall. Widgets that don't need lbl_extra leave it empty.
BarRow     make_bar_row(lv_obj_t *parent, int x, int y, int w, const char *key,
                        lv_color_t key_col, lv_color_t val_col,
                        lv_color_t extra_col, lv_color_t bar_bg);
void       make_placeholder(lv_obj_t *parent, const char *text);                      // centered dim label
lv_color_t pct_color3(int pct, lv_color_t fill, lv_color_t warn, lv_color_t alert);  // OK / WARN / ALERT with custom colours
lv_color_t pct_col3(int pct, lv_color_t fill, lv_color_t warn, lv_color_t alert);    // same, but returns fill when pct < 0 (unavailable)
lv_color_t pct_color(int pct);                                                         // backward compat: pct_color3 with default colours
lv_color_t pct_col(int pct);                                                           // backward compat: pct_col3 with default colours
void       fmt_k(char *buf, size_t sz, int32_t n);                                    // e.g. 1234 → "1.2k"
void       fmt_reset(char *buf, size_t sz, int secs);                                 // countdown to rate-limit reset
