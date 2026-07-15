#include "system.h"
#include "../layout.h"
#include "../state.h"
#include "../theme.h"
#include "../ui_helpers.h"
#include <stdio.h>

static BarRow row_cpu;
static BarRow row_ram;
static BarRow row_wpm;

// Build three labeled rows: CPU, RAM, WPM — each with value + progress bar
void build_system_panel(lv_obj_t *parent) {
    struct { const char *key; lv_color_t key_col, fill_col, bar_bg; BarRow *row; } defs[] = {
        { "CPU", COL_SYSTEM_CPU_LABEL, COL_SYSTEM_CPU_BAR_FILL, COL_SYSTEM_CPU_BAR_BG, &row_cpu },
        { "RAM", COL_SYSTEM_RAM_LABEL, COL_SYSTEM_RAM_BAR_FILL, COL_SYSTEM_RAM_BAR_BG, &row_ram },
        { "WPM", COL_SYSTEM_WPM_LABEL, COL_SYSTEM_WPM_BAR_FILL, COL_SYSTEM_WPM_BAR_BG, &row_wpm },
    };
    for (int i = 0; i < 3; i++) {
        // lbl_val (near-left) unused; percentage goes in lbl_extra (right-aligned)
        *defs[i].row = make_bar_row(parent, 8, 4 + i * STATS_ROW_H, PANEL_W - 16,
                                    defs[i].key, defs[i].key_col, defs[i].fill_col,
                                    defs[i].fill_col, defs[i].bar_bg);
        lv_label_set_text(defs[i].row->lbl_val, "");
    }
}

// Push current CPU/RAM/WPM values and bar colors from state
void update_system_ui() {
    char buf[8];

    snprintf(buf, sizeof(buf), "%d%%", state.cpu);
    lv_label_set_text(row_cpu.lbl_extra, buf);
    lv_color_t ccpu = pct_color3(state.cpu, COL_SYSTEM_CPU_BAR_FILL, COL_SYSTEM_CPU_BAR_WARN, COL_SYSTEM_CPU_BAR_ALERT);
    lv_obj_set_style_text_color(row_cpu.lbl_extra, ccpu, 0);
    lv_bar_set_value(row_cpu.bar, state.cpu, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(row_cpu.bar, ccpu, LV_PART_INDICATOR);

    snprintf(buf, sizeof(buf), "%d%%", state.ram);
    lv_label_set_text(row_ram.lbl_extra, buf);
    lv_color_t cram = pct_color3(state.ram, COL_SYSTEM_RAM_BAR_FILL, COL_SYSTEM_RAM_BAR_WARN, COL_SYSTEM_RAM_BAR_ALERT);
    lv_obj_set_style_text_color(row_ram.lbl_extra, cram, 0);
    lv_bar_set_value(row_ram.bar, state.ram, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(row_ram.bar, cram, LV_PART_INDICATOR);

    int wpm_capped = state.wpm > 100 ? 100 : state.wpm;
    snprintf(buf, sizeof(buf), "%d", state.wpm);
    lv_label_set_text(row_wpm.lbl_extra, buf);
    lv_color_t cwpm = state.active ? COL_SYSTEM_WPM_BAR_FILL : COL_SYSTEM_WPM_INACTIVE;
    lv_obj_set_style_text_color(row_wpm.lbl_extra, cwpm, 0);
    lv_bar_set_value(row_wpm.bar, wpm_capped, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(row_wpm.bar, cwpm, LV_PART_INDICATOR);
}

// Called when serial link times out — show dashes instead of stale numbers
void system_show_disconnected() {
    lv_label_set_text(row_cpu.lbl_extra, "--");
    lv_label_set_text(row_ram.lbl_extra, "--");
    lv_label_set_text(row_wpm.lbl_extra, "--");
    lv_obj_set_style_text_color(row_cpu.lbl_extra, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_color(row_ram.lbl_extra, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_color(row_wpm.lbl_extra, COL_TEXT_DIM, 0);
}
