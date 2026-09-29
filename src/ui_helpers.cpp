#include "ui_helpers.h"
#include "theme.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// LVGL widget factories
// ---------------------------------------------------------------------------

lv_obj_t *make_panel(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t bg) {
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_remove_style_all(p);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_color(p, bg, 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

lv_obj_t *make_bar(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t bg) {
    lv_obj_t *bar = lv_bar_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, w, h);
    lv_obj_set_pos(bar, x, y);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, COL_OK, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    return bar;
}

void make_hdiv(lv_obj_t *parent, int y, int x, int w, lv_color_t col) {
    lv_obj_t *div = lv_obj_create(parent);
    lv_obj_remove_style_all(div);
    lv_obj_set_size(div, w, 1);
    lv_obj_set_pos(div, x, y);
    lv_obj_set_style_bg_color(div, col, 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_clear_flag(div, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *make_dot(lv_obj_t *parent, int x, int y, lv_color_t col) {
    lv_obj_t *d = lv_obj_create(parent);
    lv_obj_remove_style_all(d);
    lv_obj_set_size(d, 6, 6);
    lv_obj_set_pos(d, x, y);
    lv_obj_set_style_bg_color(d, col, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(d, 0, 0);
    lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
    return d;
}

// ---------------------------------------------------------------------------
// Formatting & color helpers
// ---------------------------------------------------------------------------

BarRow make_bar_row(lv_obj_t *parent, int x, int y, int w, const char *key,
                    lv_color_t key_col, lv_color_t val_col,
                    lv_color_t extra_col, lv_color_t bar_bg) {
    BarRow r;

    r.lbl_key = lv_label_create(parent);
    lv_label_set_text(r.lbl_key, key);
    lv_obj_set_style_text_color(r.lbl_key, key_col, 0);
    lv_obj_set_style_text_font(r.lbl_key, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(r.lbl_key, x, y);

    r.lbl_val = lv_label_create(parent);
    lv_label_set_text(r.lbl_val, "--");
    lv_obj_set_style_text_color(r.lbl_val, val_col, 0);
    lv_obj_set_style_text_font(r.lbl_val, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(r.lbl_val, x + 28, y);

    r.lbl_extra = lv_label_create(parent);
    lv_label_set_text(r.lbl_extra, "");
    lv_obj_set_style_text_color(r.lbl_extra, extra_col, 0);
    lv_obj_set_style_text_font(r.lbl_extra, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(r.lbl_extra, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(r.lbl_extra, x + w - 70, y);
    lv_obj_set_width(r.lbl_extra, 70);

    r.bar = make_bar(parent, x, y + 14, w, 5, bar_bg);

    return r;
}

void make_placeholder(lv_obj_t *parent, const char *text) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
}

lv_obj_t *make_ellipsis_label(lv_obj_t *parent, int x, int y, int w, const lv_font_t *font) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "");
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_width(lbl, w);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    return lbl;
}

void set_ellipsis_text(lv_obj_t *label, const char *text) {
    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    lv_coord_t ls  = lv_obj_get_style_text_letter_space(label, LV_PART_MAIN);
    lv_coord_t max = lv_obj_get_style_width(label, LV_PART_MAIN);
    uint32_t   len = strlen(text);

    if (lv_txt_get_width(text, len, font, ls, LV_TEXT_FLAG_NONE) <= max) {
        lv_label_set_text(label, text);
        return;
    }

    // Keep the longest whole-character prefix that still fits with the dots.
    static const char DOTS[] = "...";
    lv_coord_t avail = max - lv_txt_get_width(DOTS, 3, font, ls, LV_TEXT_FLAG_NONE);
    uint32_t fit = 0, i = 0;
    while (i < len) {
        uint32_t next = i;
        _lv_txt_encoded_next(text, &next);
        if (lv_txt_get_width(text, next, font, ls, LV_TEXT_FLAG_NONE) > avail) break;
        fit = i = next;
    }
    while (fit > 0 && text[fit - 1] == ' ') fit--;  // no "word ..." gap

    char buf[128];
    if (fit > sizeof(buf) - sizeof(DOTS)) fit = sizeof(buf) - sizeof(DOTS);
    memcpy(buf, text, fit);
    memcpy(buf + fit, DOTS, sizeof(DOTS));
    lv_label_set_text(label, buf);
}

// Distance from the top of a line box down to the vertical centre of glyph `ch`.
static float glyph_center_y(const lv_font_t *font, uint32_t ch) {
    lv_font_glyph_dsc_t g;
    if (!lv_font_get_glyph_dsc(font, &g, ch, 0)) return lv_font_get_line_height(font) / 2.0f;
    float baseline = lv_font_get_line_height(font) - font->base_line;
    return baseline - (g.ofs_y + g.box_h / 2.0f);
}

void align_dot_to_label(lv_obj_t *dot, lv_obj_t *label) {
    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    float center = (glyph_center_y(font, 'x') + glyph_center_y(font, 'H')) / 2.0f;
    lv_coord_t dot_h = lv_obj_get_style_height(dot, LV_PART_MAIN);
    lv_obj_update_layout(label);
    lv_obj_set_y(dot, lv_obj_get_y(label) + (lv_coord_t)(center - dot_h / 2.0f + 0.5f));
}

lv_color_t pct_color3(int pct, lv_color_t fill, lv_color_t warn, lv_color_t alert) {
    if (pct >= 90) return alert;
    if (pct >= 70) return warn;
    return fill;
}

lv_color_t pct_col3(int pct, lv_color_t fill, lv_color_t warn, lv_color_t alert) {
    if (pct < 0)   return fill;
    if (pct >= 90) return alert;
    if (pct >= 70) return warn;
    return fill;
}

lv_color_t pct_color(int pct) {
    return pct_color3(pct, COL_BAR_FILL, COL_BAR_WARN, COL_BAR_ALERT);
}

lv_color_t pct_col(int pct) {
    return pct_col3(pct, COL_BAR_FILL, COL_BAR_WARN, COL_BAR_ALERT);
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
