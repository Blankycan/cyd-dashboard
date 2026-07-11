#include "ui_helpers.h"
#include "theme.h"
#include <stdio.h>

// ---------------------------------------------------------------------------
// LVGL widget factories
// ---------------------------------------------------------------------------

lv_obj_t *make_panel(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_remove_style_all(p);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_color(p, COL_BG, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

lv_obj_t *make_bar(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, w, h);
    lv_obj_set_pos(bar, x, y);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, COL_BAR_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, COL_OK, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    return bar;
}

void make_hdiv(lv_obj_t *parent, int y, int x, int w) {
    lv_obj_t *div = lv_obj_create(parent);
    lv_obj_remove_style_all(div);
    lv_obj_set_size(div, w, 1);
    lv_obj_set_pos(div, x, y);
    lv_obj_set_style_bg_color(div, COL_DIVIDER, 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);
}

// ---------------------------------------------------------------------------
// Formatting & color helpers
// ---------------------------------------------------------------------------

void make_placeholder(lv_obj_t *parent, const char *text) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
}

lv_color_t pct_color(int pct) {
    if (pct >= 90) return COL_ALERT;
    if (pct >= 70) return COL_WARN;
    return COL_OK;
}

lv_color_t pct_col(int pct) {
    if (pct < 0)   return COL_TEXT_DIM;
    if (pct >= 90) return COL_ALERT;
    if (pct >= 70) return COL_WARN;
    return COL_OK;
}

void fmt_k(char *buf, size_t sz, int32_t n) {
    if      (n >= 1000000) snprintf(buf, sz, "%.1fm", n / 1000000.0f);
    else if (n >= 100000)  snprintf(buf, sz, "%.0fk", n / 1000.0f);
    else if (n >= 1000)    snprintf(buf, sz, "%.1fk", n / 1000.0f);
    else                   snprintf(buf, sz, "%d",    (int)n);
}

void fmt_reset(char *buf, size_t sz, int secs) {
    if (secs < 0)  { snprintf(buf, sz, "--");   return; }
    if (secs < 60) { snprintf(buf, sz, "< 1m"); return; }
    int m = secs / 60, h = m / 60, d = h / 24;
    if (d > 0)      snprintf(buf, sz, "%dd %dh", d, h % 24);
    else if (h > 0) snprintf(buf, sz, "%dh %dm", h, m % 60);
    else            snprintf(buf, sz, "%dm", m);
}
