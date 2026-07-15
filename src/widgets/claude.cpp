#include "claude.h"
#include "../layout.h"
#include "../state.h"
#include "../theme.h"
#include "../ui_helpers.h"
#include <stdio.h>

static lv_obj_t *dot_claude     = nullptr;

// Token section
static lv_obj_t *lbl_claude_out  = nullptr;
static lv_obj_t *bar_claude_tok  = nullptr;
static lv_obj_t *lbl_claude_in   = nullptr;
static lv_obj_t *lbl_claude_sess = nullptr;

// Rate-limit section
static BarRow row_h5;
static BarRow row_w7;

void build_claude_panel(lv_obj_t *parent) {
    // --- Header ---
    dot_claude = make_dot(parent, 8, (20 - 6) / 2, COL_CLAUDE_DOT_IDLE);

    lv_obj_t *lbl_hdr = lv_label_create(parent);
    lv_label_set_text(lbl_hdr, "claude");
    lv_obj_set_style_text_color(lbl_hdr, COL_CLAUDE_TITLE, 0);
    lv_obj_set_style_text_font(lbl_hdr, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_hdr, 20, 3);

    // --- Token section ---
    lv_obj_t *lbl_out_hdr = lv_label_create(parent);
    lv_label_set_text(lbl_out_hdr, "out today");
    lv_obj_set_style_text_color(lbl_out_hdr, COL_CLAUDE_META, 0);
    lv_obj_set_style_text_font(lbl_out_hdr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl_out_hdr, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_out_hdr, PANEL_W - 70, 22);
    lv_obj_set_width(lbl_out_hdr, 62);

    lbl_claude_out = lv_label_create(parent);
    lv_label_set_text(lbl_claude_out, "0");
    lv_obj_set_style_text_color(lbl_claude_out, COL_CLAUDE_TOKENS_OUT, 0);
    lv_obj_set_style_text_font(lbl_claude_out, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(lbl_claude_out, 8, 20);

    bar_claude_tok = make_bar(parent, 8, 44, PANEL_W - 16, 5, COL_CLAUDE_TOK_BAR_BG);
    lv_obj_set_style_bg_color(bar_claude_tok, COL_CLAUDE_TOK_BAR_FILL, LV_PART_INDICATOR);

    lbl_claude_in = lv_label_create(parent);
    lv_label_set_text(lbl_claude_in, "in  0");
    lv_obj_set_style_text_color(lbl_claude_in, COL_CLAUDE_META, 0);
    lv_obj_set_style_text_font(lbl_claude_in, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_claude_in, 8, 54);

    lbl_claude_sess = lv_label_create(parent);
    lv_label_set_text(lbl_claude_sess, "0 sess");
    lv_obj_set_style_text_color(lbl_claude_sess, COL_CLAUDE_META, 0);
    lv_obj_set_style_text_font(lbl_claude_sess, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl_claude_sess, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_claude_sess, PANEL_W / 2, 54);
    lv_obj_set_width(lbl_claude_sess, PANEL_W / 2 - 8);

    // --- Internal divider between token and rate-limit sections ---
    make_hdiv(parent, 70, 8, PANEL_W - 16, COL_CLAUDE_DIVIDER);

    // --- Rate-limit section ---
    row_h5 = make_bar_row(parent, 8, 76, PANEL_W - 16, "5h",
                           COL_CLAUDE_H5_LABEL, COL_CLAUDE_H5_LABEL,
                           COL_CLAUDE_H5_RESET, COL_CLAUDE_H5_BAR_BG);
    lv_obj_set_style_bg_color(row_h5.bar, COL_CLAUDE_H5_BAR_FILL, LV_PART_INDICATOR);

    row_w7 = make_bar_row(parent, 8, 100, PANEL_W - 16, "7d",
                           COL_CLAUDE_W7_LABEL, COL_CLAUDE_W7_LABEL,
                           COL_CLAUDE_W7_RESET, COL_CLAUDE_W7_BAR_BG);
    lv_obj_set_style_bg_color(row_w7.bar, COL_CLAUDE_W7_BAR_FILL, LV_PART_INDICATOR);
}

void update_claude_ui() {
    char buf[20], tmp[16];

    // Dot color: rate-limit health if available, else session presence
    lv_color_t dot_c;
    if (state.claude_h5_pct >= 0)
        dot_c = pct_col3(state.claude_h5_pct, COL_CLAUDE_DOT_OK, COL_CLAUDE_DOT_WARN, COL_CLAUDE_DOT_ALERT);
    else
        dot_c = (state.claude_sessions > 0) ? COL_CLAUDE_DOT_OK : COL_CLAUDE_DOT_IDLE;
    lv_obj_set_style_bg_color(dot_claude, dot_c, 0);

    // Token section
    if (!state.connected) {
        lv_label_set_text(lbl_claude_out, "--");
        lv_bar_set_value(bar_claude_tok, 0, LV_ANIM_OFF);
        lv_label_set_text(lbl_claude_in,   "in  --");
        lv_label_set_text(lbl_claude_sess, "-- sessions");
    } else {
        fmt_k(buf, sizeof(buf), state.claude_out);
        lv_label_set_text(lbl_claude_out, buf);

        int32_t tok_pct = (int32_t)((float)state.claude_out / (float)CLAUDE_TOK_MAX * 100.0f);
        if (tok_pct > 100) tok_pct = 100;
        lv_bar_set_value(bar_claude_tok, (int)tok_pct, LV_ANIM_OFF);

        fmt_k(tmp, sizeof(tmp), state.claude_inp);
        snprintf(buf, sizeof(buf), "in  %s", tmp);
        lv_label_set_text(lbl_claude_in, buf);

        snprintf(buf, sizeof(buf), "%d %s", state.claude_sessions,
                 state.claude_sessions == 1 ? "session" : "sessions");
        lv_label_set_text(lbl_claude_sess, buf);
    }

    // Rate-limit section
    if (state.claude_h5_pct < 0) {
        lv_label_set_text(row_h5.lbl_val, "--");
        lv_obj_set_style_text_color(row_h5.lbl_val, COL_CLAUDE_H5_LABEL, 0);
    } else {
        snprintf(buf, sizeof(buf), "%d%%", state.claude_h5_pct);
        lv_label_set_text(row_h5.lbl_val, buf);
        lv_obj_set_style_text_color(row_h5.lbl_val, pct_col3(state.claude_h5_pct, COL_CLAUDE_H5_BAR_FILL, COL_CLAUDE_H5_BAR_WARN, COL_CLAUDE_H5_BAR_ALERT), 0);
    }
    fmt_reset(buf, sizeof(buf), state.claude_h5_secs);
    lv_label_set_text(row_h5.lbl_extra, buf);
    lv_bar_set_value(row_h5.bar, (state.claude_h5_pct < 0) ? 0 : state.claude_h5_pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(row_h5.bar, pct_col3(state.claude_h5_pct, COL_CLAUDE_H5_BAR_FILL, COL_CLAUDE_H5_BAR_WARN, COL_CLAUDE_H5_BAR_ALERT), LV_PART_INDICATOR);

    if (state.claude_w7_pct < 0) {
        lv_label_set_text(row_w7.lbl_val, "--");
        lv_obj_set_style_text_color(row_w7.lbl_val, COL_CLAUDE_W7_LABEL, 0);
    } else {
        snprintf(buf, sizeof(buf), "%d%%", state.claude_w7_pct);
        lv_label_set_text(row_w7.lbl_val, buf);
        lv_obj_set_style_text_color(row_w7.lbl_val, pct_col3(state.claude_w7_pct, COL_CLAUDE_W7_BAR_FILL, COL_CLAUDE_W7_BAR_WARN, COL_CLAUDE_W7_BAR_ALERT), 0);
    }
    fmt_reset(buf, sizeof(buf), state.claude_w7_secs);
    lv_label_set_text(row_w7.lbl_extra, buf);
    lv_bar_set_value(row_w7.bar, (state.claude_w7_pct < 0) ? 0 : state.claude_w7_pct, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(row_w7.bar, pct_col3(state.claude_w7_pct, COL_CLAUDE_W7_BAR_FILL, COL_CLAUDE_W7_BAR_WARN, COL_CLAUDE_W7_BAR_ALERT), LV_PART_INDICATOR);
}
