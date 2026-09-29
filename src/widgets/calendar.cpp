#include "calendar.h"
#include "../layout.h"
#include "../state.h"
#include "../theme.h"
#include "../ui_helpers.h"
#include "../fonts/fonts.h"
#include <stdio.h>

// Row A: [when] [title .........] [countdown]   — "14:30 … in 23m" / "NOW … ends 18m"
static lv_obj_t *lbl_when  = nullptr;
static lv_obj_t *lbl_title = nullptr;
static lv_obj_t *lbl_cd    = nullptr;
static bool      cd_pulsing = false;

// Row B: progress through the current meeting (hidden otherwise)
static lv_obj_t *bar_progress = nullptr;

// Row C: the meeting after the one in row A — "next …" / "then …"
static lv_obj_t *lbl_after = nullptr;

// Timeline: track, one block per event, a now-marker, and hour labels
static const int TL_X0 = 8;
static const int TL_X1 = PANEL_W - 8;
static const int TL_H  = 10;
static const int TL_MAX_HOUR_LABELS = 25;   // 0..24
static int       tl_y = 0;
static lv_obj_t *tl_track = nullptr;
static lv_obj_t *tl_blocks[CAL_MAX_EVENTS];
static lv_obj_t *tl_now   = nullptr;
static lv_obj_t *tl_hours[TL_MAX_HOUR_LABELS];

static lv_obj_t *make_rect(lv_obj_t *parent, lv_color_t col) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_color(o, col, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, 2, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

void build_calendar_panel(lv_obj_t *parent) {
    // --- Row A ---
    int row_a_y = cap_top_y(&font_ui_14, PANEL_PAD_Y);

    lbl_when = lv_label_create(parent);
    lv_label_set_text(lbl_when, "");
    lv_obj_set_style_text_font(lbl_when, &font_ui_14, 0);
    lv_obj_set_pos(lbl_when, 8, row_a_y);

    lbl_cd = lv_label_create(parent);
    lv_label_set_text(lbl_cd, "");
    lv_obj_set_style_text_font(lbl_cd, &font_ui_14, 0);
    lv_obj_align(lbl_cd, LV_ALIGN_TOP_RIGHT, -8, row_a_y);

    lbl_title = make_ellipsis_label(parent, 8, row_a_y, PANEL_W - 16, &font_ui_14);

    // --- Row B --- same 5px bar as the other panels, centred in the gap below row A
    bar_progress = make_bar(parent, 8, 23, PANEL_W - 16, 5, COL_CALENDAR_PROGRESS_BG);
    lv_obj_set_style_bg_color(bar_progress, COL_CALENDAR_PROGRESS_FILL, LV_PART_INDICATOR);
    lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);

    // --- Row C ---
    lbl_after = make_ellipsis_label(parent, 8, cap_top_y(&font_ui_12, 36), PANEL_W - 16, &font_ui_12);
    lv_obj_set_style_text_color(lbl_after, COL_CALENDAR_NEXT, 0);

    // --- Timeline --- hour labels sit on the panel's bottom padding, strip just above
    const lv_font_t *hf = &lv_font_montserrat_12;
    int hour_y   = CALENDAR_H - PANEL_PAD_Y - (lv_font_get_line_height(hf) - hf->base_line);
    int hour_cap = hour_y - cap_top_y(hf, 0);
    tl_y = hour_cap - 5 - TL_H;

    tl_track = make_rect(parent, COL_CALENDAR_TRACK);
    lv_obj_set_pos(tl_track, TL_X0, tl_y);
    lv_obj_set_size(tl_track, TL_X1 - TL_X0, TL_H);

    for (int i = 0; i < CAL_MAX_EVENTS; i++) {
        tl_blocks[i] = make_rect(parent, COL_CALENDAR_EVENT);
        lv_obj_add_flag(tl_blocks[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Created after the blocks so it draws on top of them
    tl_now = make_rect(parent, COL_CALENDAR_NOW_MARK);
    lv_obj_set_style_radius(tl_now, 0, 0);
    lv_obj_set_size(tl_now, 2, TL_H + 4);
    lv_obj_set_y(tl_now, tl_y - 2);
    lv_obj_add_flag(tl_now, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < TL_MAX_HOUR_LABELS; i++) {
        tl_hours[i] = lv_label_create(parent);
        lv_obj_set_style_text_font(tl_hours[i], hf, 0);
        lv_obj_set_style_text_color(tl_hours[i], COL_CALENDAR_HOUR, 0);
        lv_obj_set_style_text_align(tl_hours[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(tl_hours[i], 20);
        lv_obj_set_y(tl_hours[i], hour_y);
        lv_obj_add_flag(tl_hours[i], LV_OBJ_FLAG_HIDDEN);
    }
}

// ---------------------------------------------------------------------------

static int tl_x(int min, int range_start, int range_end) {
    return TL_X0 + (long)(min - range_start) * (TL_X1 - TL_X0) / (range_end - range_start);
}

static void update_timeline(int now) {
    // Default working day, widened to whole hours around any event outside it
    int rs = CAL_DAY_START_H * 60, re = CAL_DAY_END_H * 60;
    for (int i = 0; i < state.cal_count; i++) {
        const CalEvent &e = state.cal_events[i];
        if (e.start_min < rs) rs = e.start_min / 60 * 60;
        if (e.end_min   > re) re = (e.end_min + 59) / 60 * 60;
    }

    for (int i = 0; i < CAL_MAX_EVENTS; i++) {
        if (i >= state.cal_count) { lv_obj_add_flag(tl_blocks[i], LV_OBJ_FLAG_HIDDEN); continue; }
        const CalEvent &e = state.cal_events[i];
        int x0 = tl_x(e.start_min, rs, re), x1 = tl_x(e.end_min, rs, re);
        if (x1 - x0 < 2) x1 = x0 + 2;
        lv_color_t col = COL_CALENDAR_EVENT;
        if (now >= 0 && e.end_min <= now)        col = COL_CALENDAR_EVENT_PAST;
        else if (now >= 0 && e.start_min <= now) col = COL_CALENDAR_EVENT_NOW;
        lv_obj_set_style_bg_color(tl_blocks[i], col, 0);
        lv_obj_set_pos(tl_blocks[i], x0, tl_y);
        lv_obj_set_size(tl_blocks[i], x1 - x0, TL_H);
        lv_obj_clear_flag(tl_blocks[i], LV_OBJ_FLAG_HIDDEN);
    }

    if (now >= rs && now <= re) {
        lv_obj_set_x(tl_now, tl_x(now, rs, re) - 1);
        lv_obj_clear_flag(tl_now, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(tl_now, LV_OBJ_FLAG_HIDDEN);
    }

    int span_h = (re - rs) / 60;
    int step   = span_h <= 12 ? 1 : span_h <= 18 ? 2 : 3;
    int n = 0;
    for (int h = rs / 60; h <= re / 60 && n < TL_MAX_HOUR_LABELS; h += step, n++) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", h);
        lv_label_set_text(tl_hours[n], buf);
        lv_obj_set_x(tl_hours[n], tl_x(h * 60, rs, re) - 10);
        lv_obj_clear_flag(tl_hours[n], LV_OBJ_FLAG_HIDDEN);
    }
    for (; n < TL_MAX_HOUR_LABELS; n++) lv_obj_add_flag(tl_hours[n], LV_OBJ_FLAG_HIDDEN);
}

static void set_countdown(const char *prefix, int mins, bool soon) {
    char dur[16], buf[24];
    fmt_reset(dur, sizeof(dur), mins * 60);
    snprintf(buf, sizeof(buf), "%s %s", prefix, dur);
    lv_label_set_text(lbl_cd, buf);
    lv_obj_set_style_text_color(lbl_cd, soon ? COL_CALENDAR_COUNTDOWN_SOON : COL_CALENDAR_COUNTDOWN, 0);
    if (soon && !cd_pulsing)      start_breathe(lbl_cd, CAL_PULSE_MS, BREATHE_TEXT);
    else if (!soon && cd_pulsing) stop_breathe(lbl_cd, BREATHE_TEXT);
    cd_pulsing = soon;
}

static void set_after(const char *prefix, const CalEvent *e) {
    if (!e) { set_ellipsis_text(lbl_after, ""); return; }
    char buf[64];
    snprintf(buf, sizeof(buf), "%s %02d:%02d  %s", prefix, e->start_min / 60, e->start_min % 60, e->title);
    set_ellipsis_text(lbl_after, buf);
}

// Fit the title between the "when" label and the countdown, then set its text.
static void set_title(const char *text, lv_color_t col) {
    lv_obj_update_layout(lbl_cd);
    bool has_when = lv_label_get_text(lbl_when)[0] != '\0';
    int  x  = has_when ? lv_obj_get_x(lbl_when) + lv_obj_get_width(lbl_when) + 8 : 8;
    bool has_cd = lv_label_get_text(lbl_cd)[0] != '\0';
    int  x1 = has_cd ? lv_obj_get_x(lbl_cd) - 8 : PANEL_W - 8;
    lv_obj_set_x(lbl_title, x);
    lv_obj_set_width(lbl_title, x1 - x);
    lv_obj_set_style_text_color(lbl_title, col, 0);
    set_ellipsis_text(lbl_title, text);
}

bool calendar_wants_focus() {
    int now = dash_now_min();
    if (now < 0) return false;
    for (int i = 0; i < state.cal_count; i++) {
        const CalEvent &e = state.cal_events[i];
        if (e.start_min <= now && now < e.end_min)           return true;
        if (e.start_min > now && e.start_min - now <= CAL_QUIET_MIN) return true;
    }
    return false;
}

void update_calendar_ui() {
    int now = dash_now_min();
    update_timeline(now);

    const CalEvent *cur = nullptr, *next = nullptr, *after_next = nullptr;
    if (now >= 0) {
        for (int i = 0; i < state.cal_count; i++) {
            const CalEvent &e = state.cal_events[i];
            if (!cur && e.start_min <= now && now < e.end_min) cur = &e;
            else if (e.start_min > now) {
                if (!next) next = &e;
                else if (!after_next) after_next = &e;
            }
        }
    }

    if (cur) {
        lv_label_set_text(lbl_when, "NOW");
        lv_obj_set_style_text_color(lbl_when, COL_CALENDAR_NOW, 0);
        set_countdown("ends", cur->end_min - now, false);
        set_title(cur->title, COL_CALENDAR_TITLE);
        lv_bar_set_value(bar_progress,
                         (now - cur->start_min) * 100 / (cur->end_min - cur->start_min), LV_ANIM_OFF);
        lv_obj_clear_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
        set_after("next", next);
        return;
    }

    lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);

    if (next) {
        char when[6];
        snprintf(when, sizeof(when), "%02d:%02d", next->start_min / 60, next->start_min % 60);
        lv_label_set_text(lbl_when, when);
        lv_obj_set_style_text_color(lbl_when, COL_CALENDAR_TIME, 0);
        int mins = next->start_min - now;
        set_countdown("in", mins, mins <= CAL_SOON_MIN);
        set_title(next->title, COL_CALENDAR_TITLE);
        set_after("then", after_next);
        return;
    }

    lv_label_set_text(lbl_when, "");
    lv_label_set_text(lbl_cd, "");
    if (cd_pulsing) { stop_breathe(lbl_cd, BREATHE_TEXT); cd_pulsing = false; }
    const char *msg = !state.cal_available ? "no calendar"
                    : now < 0              ? ""
                    : "no more meetings today";
    set_title(msg, COL_CALENDAR_DONE);
    set_after("", nullptr);
}
