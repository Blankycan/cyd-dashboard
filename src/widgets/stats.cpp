#include "stats.h"
#include "../layout.h"
#include "../state.h"
#include "../theme.h"
#include "../ui_helpers.h"
#include <stdio.h>

static lv_obj_t *lbl_cpu_val = nullptr;
static lv_obj_t *bar_cpu     = nullptr;
static lv_obj_t *lbl_ram_val = nullptr;
static lv_obj_t *bar_ram     = nullptr;
static lv_obj_t *lbl_wpm_val = nullptr;
static lv_obj_t *bar_wpm     = nullptr;

// Build three labeled rows: CPU, RAM, WPM — each with value + progress bar
void build_stats_panel(lv_obj_t *parent) {
    struct { const char *key; lv_obj_t **val; lv_obj_t **bar; } rows[3] = {
        { "CPU", &lbl_cpu_val, &bar_cpu },
        { "RAM", &lbl_ram_val, &bar_ram },
        { "WPM", &lbl_wpm_val, &bar_wpm },
    };

    for (int i = 0; i < 3; i++) {
        int ty = 4 + i * STATS_ROW_H;
        int by = ty + 12;

        lv_obj_t *k = lv_label_create(parent);
        lv_label_set_text(k, rows[i].key);
        lv_obj_set_style_text_color(k, COL_TEXT_DIM, 0);
        lv_obj_set_style_text_font(k, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(k, 8, ty);

        lv_obj_t *v = lv_label_create(parent);
        lv_label_set_text(v, "0");
        lv_obj_set_style_text_color(v, COL_OK, 0);
        lv_obj_set_style_text_font(v, &lv_font_montserrat_10, 0);
        lv_obj_align(v, LV_ALIGN_TOP_RIGHT, -8, ty);
        *rows[i].val = v;

        *rows[i].bar = make_bar(parent, 8, by, PANEL_W - 16, 4);
    }
}

// Push current CPU/RAM/WPM values and bar colors from state
void update_stats_ui() {
    char buf[8];

    snprintf(buf, sizeof(buf), "%d%%", state.cpu);
    lv_label_set_text(lbl_cpu_val, buf);
    lv_color_t ccpu = pct_color(state.cpu);
    lv_obj_set_style_text_color(lbl_cpu_val, ccpu, 0);
    lv_bar_set_value(bar_cpu, state.cpu, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_cpu, ccpu, LV_PART_INDICATOR);

    snprintf(buf, sizeof(buf), "%d%%", state.ram);
    lv_label_set_text(lbl_ram_val, buf);
    lv_color_t cram = pct_color(state.ram);
    lv_obj_set_style_text_color(lbl_ram_val, cram, 0);
    lv_bar_set_value(bar_ram, state.ram, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_ram, cram, LV_PART_INDICATOR);

    int wpm_capped = state.wpm > 100 ? 100 : state.wpm;
    snprintf(buf, sizeof(buf), "%d", state.wpm);
    lv_label_set_text(lbl_wpm_val, buf);
    lv_color_t cwpm = state.active ? COL_OK : COL_TEXT_DIM;
    lv_obj_set_style_text_color(lbl_wpm_val, cwpm, 0);
    lv_bar_set_value(bar_wpm, wpm_capped, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_wpm, cwpm, LV_PART_INDICATOR);
}

// Called when serial link times out — show dashes instead of stale numbers
void stats_show_disconnected() {
    lv_label_set_text(lbl_cpu_val, "--");
    lv_label_set_text(lbl_ram_val, "--");
    lv_label_set_text(lbl_wpm_val, "--");
    lv_obj_set_style_text_color(lbl_cpu_val, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_color(lbl_ram_val, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_color(lbl_wpm_val, COL_TEXT_DIM, 0);
}
