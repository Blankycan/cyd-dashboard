#include "claude.h"
#include "../layout.h"
#include "../state.h"
#include "../theme.h"
#include "../ui_helpers.h"
#include <Arduino.h>

// Panel has two stacked views (token usage vs rate limits) toggled by a timer.
static lv_obj_t  *dot_claude       = nullptr;
static lv_obj_t  *claude_view_tok  = nullptr;
static lv_obj_t  *claude_view_rl   = nullptr;
static lv_timer_t *claude_rot_timer  = nullptr;
static lv_timer_t *claude_prog_timer = nullptr;
static bool        claude_show_rl    = false;
static uint32_t    claude_switched_ms = 0;

static lv_obj_t   *bar_claude_prog  = nullptr;
static lv_obj_t *lbl_claude_out  = nullptr;
static lv_obj_t *bar_claude_tok  = nullptr;
static lv_obj_t *lbl_claude_in   = nullptr;
static lv_obj_t *lbl_claude_sess = nullptr;
static lv_obj_t *lbl_h5_pct   = nullptr;
static lv_obj_t *lbl_h5_reset = nullptr;
static lv_obj_t *bar_h5       = nullptr;
static lv_obj_t *lbl_w7_pct   = nullptr;
static lv_obj_t *lbl_w7_reset = nullptr;
static lv_obj_t *bar_w7       = nullptr;

// Transparent sub-container for one of the two rotating views
static lv_obj_t *make_view(lv_obj_t *parent, int y, int h) {
    lv_obj_t *v = lv_obj_create(parent);
    lv_obj_remove_style_all(v);
    lv_obj_set_size(v, PANEL_W, h);
    lv_obj_set_pos(v, 0, y);
    lv_obj_set_style_bg_opa(v, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(v, 0, 0);
    lv_obj_clear_flag(v, LV_OBJ_FLAG_SCROLLABLE);
    return v;
}

// Swap between token and rate-limit views every CLAUDE_ROTATE_MS
static void claude_rotate_cb(lv_timer_t *) {
    if (state.claude_h5_pct < 0) return;
    claude_show_rl  = !claude_show_rl;
    claude_switched_ms = millis();
    if (claude_show_rl) {
        lv_obj_add_flag(claude_view_tok,  LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(claude_view_rl, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(claude_view_tok, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(claude_view_rl,   LV_OBJ_FLAG_HIDDEN);
    }
}

// Thin progress bar under header — fills over the rotation interval
static void claude_prog_cb(lv_timer_t *) {
    if (!bar_claude_prog || state.claude_h5_pct < 0) return;
    uint32_t elapsed = millis() - claude_switched_ms;
    int pct = (int)((float)elapsed / (float)CLAUDE_ROTATE_MS * 100.0f);
    if (pct > 100) pct = 100;
    lv_bar_set_value(bar_claude_prog, pct, LV_ANIM_OFF);
}

void build_claude_panel(lv_obj_t *parent) {
    dot_claude = lv_obj_create(parent);
    lv_obj_remove_style_all(dot_claude);
    lv_obj_set_size(dot_claude, 6, 6);
    lv_obj_set_pos(dot_claude, 8, (20 - 6) / 2);
    lv_obj_set_style_bg_color(dot_claude, COL_TEXT_DIM, 0);
    lv_obj_set_style_bg_opa(dot_claude, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot_claude, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot_claude, 0, 0);
    lv_obj_clear_flag(dot_claude, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_hdr = lv_label_create(parent);
    lv_label_set_text(lbl_hdr, "claude");
    lv_obj_set_style_text_color(lbl_hdr, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_hdr, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_hdr, 20, 3);

    bar_claude_prog = make_bar(parent, PANEL_W - 70, 9, 62, 4);
    lv_obj_set_style_bg_color(bar_claude_prog, COL_TEXT_DIM, LV_PART_INDICATOR);
    lv_bar_set_value(bar_claude_prog, 0, LV_ANIM_OFF);
    lv_obj_add_flag(bar_claude_prog, LV_OBJ_FLAG_HIDDEN);

    const int VIEW_Y = 20, VIEW_H = CLAUDE_H - VIEW_Y;

    // Token view
    claude_view_tok = make_view(parent, VIEW_Y, VIEW_H);

    lv_obj_t *lbl_out_hdr = lv_label_create(claude_view_tok);
    lv_label_set_text(lbl_out_hdr, "out today");
    lv_obj_set_style_text_color(lbl_out_hdr, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_out_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl_out_hdr, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_out_hdr, PANEL_W - 70, 2);
    lv_obj_set_width(lbl_out_hdr, 62);

    lbl_claude_out = lv_label_create(claude_view_tok);
    lv_label_set_text(lbl_claude_out, "0");
    lv_obj_set_style_text_color(lbl_claude_out, COL_GLOW, 0);
    lv_obj_set_style_text_font(lbl_claude_out, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(lbl_claude_out, 8, 2);

    bar_claude_tok = make_bar(claude_view_tok, 8, 30, PANEL_W - 16, 6);
    lv_obj_set_style_bg_color(bar_claude_tok, COL_GLOW, LV_PART_INDICATOR);

    lbl_claude_in = lv_label_create(claude_view_tok);
    lv_label_set_text(lbl_claude_in, "in  0");
    lv_obj_set_style_text_color(lbl_claude_in, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_claude_in, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_claude_in, 8, 44);

    lbl_claude_sess = lv_label_create(claude_view_tok);
    lv_label_set_text(lbl_claude_sess, "0 sess");
    lv_obj_set_style_text_color(lbl_claude_sess, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_claude_sess, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl_claude_sess, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_claude_sess, PANEL_W / 2, 44);
    lv_obj_set_width(lbl_claude_sess, PANEL_W / 2 - 8);

    // Rate-limit view
    claude_view_rl = make_view(parent, VIEW_Y, VIEW_H);
    lv_obj_add_flag(claude_view_rl, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *l5h = lv_label_create(claude_view_rl);
    lv_label_set_text(l5h, "5h");
    lv_obj_set_style_text_color(l5h, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(l5h, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(l5h, 8, 4);

    lbl_h5_pct = lv_label_create(claude_view_rl);
    lv_label_set_text(lbl_h5_pct, "--");
    lv_obj_set_style_text_font(lbl_h5_pct, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_h5_pct, 30, 4);

    lbl_h5_reset = lv_label_create(claude_view_rl);
    lv_label_set_text(lbl_h5_reset, "--");
    lv_obj_set_style_text_color(lbl_h5_reset, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_h5_reset, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl_h5_reset, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_h5_reset, PANEL_W - 78, 4);
    lv_obj_set_width(lbl_h5_reset, 70);

    bar_h5 = make_bar(claude_view_rl, 8, 18, PANEL_W - 16, 6);

    lv_obj_t *l7d = lv_label_create(claude_view_rl);
    lv_label_set_text(l7d, "7d");
    lv_obj_set_style_text_color(l7d, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(l7d, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(l7d, 8, 34);

    lbl_w7_pct = lv_label_create(claude_view_rl);
    lv_label_set_text(lbl_w7_pct, "--");
    lv_obj_set_style_text_font(lbl_w7_pct, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_w7_pct, 30, 34);

    lbl_w7_reset = lv_label_create(claude_view_rl);
    lv_label_set_text(lbl_w7_reset, "--");
    lv_obj_set_style_text_color(lbl_w7_reset, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_w7_reset, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl_w7_reset, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_w7_reset, PANEL_W - 78, 34);
    lv_obj_set_width(lbl_w7_reset, 70);

    bar_w7 = make_bar(claude_view_rl, 8, 48, PANEL_W - 16, 6);
}

// Refresh all Claude labels and bars from state (both views, even if hidden)
void update_claude_ui() {
    char buf[20], tmp[16];

    lv_color_t dot_c = (state.claude_h5_pct >= 0)
                       ? pct_col(state.claude_h5_pct)
                       : (state.claude_sessions > 0 ? COL_OK : COL_TEXT_DIM);
    lv_obj_set_style_bg_color(dot_claude, dot_c, 0);

    if (state.claude_h5_pct >= 0) {
        lv_obj_clear_flag(bar_claude_prog, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(bar_claude_prog, LV_OBJ_FLAG_HIDDEN);
    }

    fmt_k(buf, sizeof(buf), state.claude_out);
    lv_label_set_text(lbl_claude_out, buf);

    int32_t tok_pct = (int32_t)((float)state.claude_out / 100000.0f * 100.0f);
    if (tok_pct > 100) tok_pct = 100;
    lv_bar_set_value(bar_claude_tok, (int)tok_pct, LV_ANIM_OFF);

    fmt_k(tmp, sizeof(tmp), state.claude_inp);
    snprintf(buf, sizeof(buf), "in  %s", tmp);
    lv_label_set_text(lbl_claude_in, buf);

    snprintf(buf, sizeof(buf), "%d sess", state.claude_sessions);
    lv_label_set_text(lbl_claude_sess, buf);

    if (state.claude_h5_pct < 0) {
        lv_label_set_text(lbl_h5_pct, "--");
        lv_obj_set_style_text_color(lbl_h5_pct, COL_TEXT_DIM, 0);
    } else {
        snprintf(buf, sizeof(buf), "%d%%", state.claude_h5_pct);
        lv_label_set_text(lbl_h5_pct, buf);
        lv_obj_set_style_text_color(lbl_h5_pct, pct_col(state.claude_h5_pct), 0);
    }
    fmt_reset(buf, sizeof(buf), state.claude_h5_secs);
    lv_label_set_text(lbl_h5_reset, buf);
    lv_bar_set_value(bar_h5, (state.claude_h5_pct < 0) ? 0 : state.claude_h5_pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_h5, pct_col(state.claude_h5_pct), LV_PART_INDICATOR);

    if (state.claude_w7_pct < 0) {
        lv_label_set_text(lbl_w7_pct, "--");
        lv_obj_set_style_text_color(lbl_w7_pct, COL_TEXT_DIM, 0);
    } else {
        snprintf(buf, sizeof(buf), "%d%%", state.claude_w7_pct);
        lv_label_set_text(lbl_w7_pct, buf);
        lv_obj_set_style_text_color(lbl_w7_pct, pct_col(state.claude_w7_pct), 0);
    }
    fmt_reset(buf, sizeof(buf), state.claude_w7_secs);
    lv_label_set_text(lbl_w7_reset, buf);
    lv_bar_set_value(bar_w7, (state.claude_w7_pct < 0) ? 0 : state.claude_w7_pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_w7, pct_col(state.claude_w7_pct), LV_PART_INDICATOR);
}

// Call once from setup() to begin auto-rotation between views
void claude_start_timers() {
#if CLAUDE_ROTATE_MS > 0
    claude_switched_ms = millis();
    claude_rot_timer   = lv_timer_create(claude_rotate_cb, CLAUDE_ROTATE_MS, nullptr);
    claude_prog_timer  = lv_timer_create(claude_prog_cb,   200,              nullptr);
#endif
}
