#include "claude.h"
#include "../layout.h"
#include "../state.h"
#include "../theme.h"
#include "../ui_helpers.h"
#include <Arduino.h>
#include <stdio.h>

static lv_obj_t *dot_claude      = nullptr;
static lv_obj_t *lbl_claude_sess = nullptr;  // today's session count, next to the header

// Rate-limit section
static BarRow row_h5;
static BarRow row_w7;

// ---------------------------------------------------------------------------
// Working-session dots — one small dot per currently-active Claude Code
// session, right-aligned in the header row. No session identity crosses the
// wire (just a live count in state.claude_working), so slots are anonymous:
// each is independently EMPTY/ACTIVE/HOLDING, and layout_dots() right-aligns
// whichever slots are currently non-EMPTY, in index order, every time the
// visible set changes. This avoids ever needing to move which LVGL object
// backs which slot — only its on-screen x position changes.
// ---------------------------------------------------------------------------
enum WorkDotState { DOT_EMPTY, DOT_ACTIVE, DOT_HOLDING };
struct WorkDot {
    WorkDotState state;
    uint32_t     hold_start_ms;
    lv_obj_t    *obj;
};
static WorkDot     work_dots[CLAUDE_SESSION_MAX_DOTS];
static lv_obj_t    *lbl_overflow      = nullptr;
static lv_timer_t  *work_hold_timer   = nullptr;

static const int WORK_DOT_RIGHT_X = PANEL_W - 8 - 6;  // rightmost dot's x (6px dot, 8px margin)
static const int WORK_DOT_PITCH   = 12;               // px between dot centers

static void dot_breathe_cb(void *obj, int32_t val) {
    lv_obj_set_style_bg_opa((lv_obj_t*)obj, (lv_opa_t)val, 0);
}

static void start_breathe(lv_obj_t *obj) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, dot_breathe_cb);
    lv_anim_set_values(&a, LV_OPA_30, LV_OPA_COVER);
    lv_anim_set_time(&a, CLAUDE_SESSION_BREATH_MS / 2);
    lv_anim_set_playback_time(&a, CLAUDE_SESSION_BREATH_MS / 2);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

// Right-align every currently-visible (non-EMPTY) slot, in index order, and
// show/hide the "+" overflow label at the true rightmost position.
static void layout_dots(bool overflow) {
    int visible = 0;
    for (int i = 0; i < CLAUDE_SESSION_MAX_DOTS; i++)
        if (work_dots[i].state != DOT_EMPTY) visible++;

    int dots_right_x = WORK_DOT_RIGHT_X - (overflow ? WORK_DOT_PITCH : 0);
    int rank = 0;
    for (int i = 0; i < CLAUDE_SESSION_MAX_DOTS; i++) {
        if (work_dots[i].state == DOT_EMPTY) {
            lv_obj_add_flag(work_dots[i].obj, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_set_x(work_dots[i].obj, dots_right_x - (visible - 1 - rank) * WORK_DOT_PITCH);
        lv_obj_clear_flag(work_dots[i].obj, LV_OBJ_FLAG_HIDDEN);
        rank++;
    }

    lv_obj_set_x(lbl_overflow, WORK_DOT_RIGHT_X);
    if (overflow) lv_obj_clear_flag(lbl_overflow, LV_OBJ_FLAG_HIDDEN);
    else           lv_obj_add_flag(lbl_overflow, LV_OBJ_FLAG_HIDDEN);
}

// Reconcile the slot array against the live "working" count from the host.
static void sync_work_dots(int raw_target) {
    if (raw_target < 0) raw_target = 0;
    bool overflow = raw_target > CLAUDE_SESSION_MAX_DOTS;
    int  target   = overflow ? (CLAUDE_SESSION_MAX_DOTS - 1) : raw_target;

    int active_count = 0;
    for (int i = 0; i < CLAUDE_SESSION_MAX_DOTS; i++)
        if (work_dots[i].state == DOT_ACTIVE) active_count++;

    if (target > active_count) {
        int need = target - active_count;
        // Reclaim HOLDING slots first — since the wire only carries a count,
        // not identity, one session finishing and another starting in the
        // same tick is indistinguishable from "session continues." Reusing
        // a HOLDING slot collapses that back into one continuously-breathing
        // dot instead of freezing one and spawning a spurious second.
        for (int i = 0; i < CLAUDE_SESSION_MAX_DOTS && need > 0; i++) {
            if (work_dots[i].state == DOT_HOLDING) {
                work_dots[i].state = DOT_ACTIVE;
                lv_obj_set_style_bg_color(work_dots[i].obj, COL_CLAUDE_WORK_DOT_ACTIVE, 0);
                start_breathe(work_dots[i].obj);
                need--;
            }
        }
        for (int i = 0; i < CLAUDE_SESSION_MAX_DOTS && need > 0; i++) {
            if (work_dots[i].state == DOT_EMPTY) {
                work_dots[i].state = DOT_ACTIVE;
                lv_obj_set_style_bg_color(work_dots[i].obj, COL_CLAUDE_WORK_DOT_ACTIVE, 0);
                lv_obj_set_style_bg_opa(work_dots[i].obj, LV_OPA_COVER, 0);
                start_breathe(work_dots[i].obj);
                need--;
            }
        }
    } else if (target < active_count) {
        int drop = active_count - target;
        for (int i = 0; i < CLAUDE_SESSION_MAX_DOTS && drop > 0; i++) {
            if (work_dots[i].state == DOT_ACTIVE) {
                work_dots[i].state         = DOT_HOLDING;
                work_dots[i].hold_start_ms = millis();
                lv_anim_del(work_dots[i].obj, NULL);
                lv_obj_set_style_bg_opa(work_dots[i].obj, LV_OPA_COVER, 0);
                lv_obj_set_style_bg_color(work_dots[i].obj, COL_CLAUDE_WORK_DOT_IDLE, 0);
                drop--;
            }
        }
    }

    layout_dots(overflow);
}

// Expire HOLDING slots once they've shown the idle colour long enough.
static void work_dot_hold_cb(lv_timer_t *) {
    uint32_t now     = millis();
    bool     changed = false;
    for (int i = 0; i < CLAUDE_SESSION_MAX_DOTS; i++) {
        if (work_dots[i].state == DOT_HOLDING &&
            now - work_dots[i].hold_start_ms >= CLAUDE_SESSION_HOLD_MS) {
            work_dots[i].state = DOT_EMPTY;
            changed = true;
        }
    }
    if (changed) {
        bool overflow = state.connected && state.claude_working > CLAUDE_SESSION_MAX_DOTS;
        layout_dots(overflow);
    }
}

void build_claude_panel(lv_obj_t *parent) {
    // --- Header ---
    lv_obj_t *lbl_hdr = lv_label_create(parent);
    lv_label_set_text(lbl_hdr, "claude");
    lv_obj_set_style_text_color(lbl_hdr, COL_CLAUDE_TITLE, 0);
    lv_obj_set_style_text_font(lbl_hdr, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_hdr, 20, 3);

    dot_claude = make_dot(parent, 8, 0, COL_CLAUDE_DOT_IDLE);
    align_dot_to_label(dot_claude, lbl_hdr);

    // Session count trails the header text; the label's left edge is pinned,
    // so it only grows rightward toward the working dots.
    lbl_claude_sess = lv_label_create(parent);
    lv_label_set_text(lbl_claude_sess, "");
    lv_obj_set_style_text_color(lbl_claude_sess, COL_CLAUDE_SESSIONS, 0);
    lv_obj_set_style_text_font(lbl_claude_sess, &lv_font_montserrat_12, 0);
    lv_obj_align_to(lbl_claude_sess, lbl_hdr, LV_ALIGN_OUT_RIGHT_BOTTOM, 8, 0);

    // --- Working-session dots (right-aligned, same row as the header) ---
    for (int i = 0; i < CLAUDE_SESSION_MAX_DOTS; i++) {
        work_dots[i].state = DOT_EMPTY;
        work_dots[i].obj    = make_dot(parent, WORK_DOT_RIGHT_X, lv_obj_get_y(dot_claude), COL_CLAUDE_WORK_DOT_ACTIVE);
        lv_obj_add_flag(work_dots[i].obj, LV_OBJ_FLAG_HIDDEN);
    }
    lbl_overflow = lv_label_create(parent);
    lv_label_set_text(lbl_overflow, "+");
    lv_obj_set_style_text_color(lbl_overflow, COL_CLAUDE_WORK_DOT_OVERFLOW, 0);
    lv_obj_set_style_text_font(lbl_overflow, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_overflow, WORK_DOT_RIGHT_X, 3);
    lv_obj_add_flag(lbl_overflow, LV_OBJ_FLAG_HIDDEN);

    work_hold_timer = lv_timer_create(work_dot_hold_cb, 250, nullptr);

    // --- Rate-limit section ---
    row_h5 = make_bar_row(parent, 8, 24, PANEL_W - 16, "5h",
                           COL_CLAUDE_H5_LABEL, COL_CLAUDE_H5_LABEL,
                           COL_CLAUDE_H5_RESET, COL_CLAUDE_H5_BAR_BG);
    lv_obj_set_style_bg_color(row_h5.bar, COL_CLAUDE_H5_BAR_FILL, LV_PART_INDICATOR);

    row_w7 = make_bar_row(parent, 8, 48, PANEL_W - 16, "7d",
                           COL_CLAUDE_W7_LABEL, COL_CLAUDE_W7_LABEL,
                           COL_CLAUDE_W7_RESET, COL_CLAUDE_W7_BAR_BG);
    lv_obj_set_style_bg_color(row_w7.bar, COL_CLAUDE_W7_BAR_FILL, LV_PART_INDICATOR);
}

void update_claude_ui() {
    char buf[20];

    sync_work_dots(state.connected ? state.claude_working : 0);

    // Dot color: rate-limit health if available, else session presence
    lv_color_t dot_c;
    if (state.claude_h5_pct >= 0)
        dot_c = pct_col3(state.claude_h5_pct, COL_CLAUDE_DOT_OK, COL_CLAUDE_DOT_WARN, COL_CLAUDE_DOT_ALERT);
    else
        dot_c = (state.claude_sessions > 0) ? COL_CLAUDE_DOT_OK : COL_CLAUDE_DOT_IDLE;
    lv_obj_set_style_bg_color(dot_claude, dot_c, 0);

    if (!state.connected) {
        lv_label_set_text(lbl_claude_sess, "-- sessions");
    } else {
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
