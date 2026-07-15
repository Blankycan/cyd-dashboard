#include "topbar.h"
#include "../layout.h"
#include "../state.h"
#include "../theme.h"
#include "../ui_helpers.h"

static lv_obj_t *lbl_time = nullptr;
static lv_obj_t *lbl_date = nullptr;

// Dark panel at top of screen with time and date labels
void build_topbar(lv_obj_t *scr) {
    lv_obj_t *tb = make_panel(scr, 0, 0, SCREEN_W, TOPBAR_H);
    lv_obj_set_style_bg_color(tb, COL_PANEL, 0);
    make_hdiv(scr, TOPBAR_H, 0, SCREEN_W);

    lbl_time = lv_label_create(tb);
    lv_label_set_text(lbl_time, "--:--");
    lv_obj_set_style_text_color(lbl_time, COL_TOPBAR_TEXT_PRI, 0);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_time, LV_ALIGN_LEFT_MID, 8, 0);

    lbl_date = lv_label_create(tb);
    lv_label_set_text(lbl_date, "");
    lv_obj_set_style_text_color(lbl_date, COL_TOPBAR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_date, LV_ALIGN_RIGHT_MID, -8, 0);
}

// Refresh clock and date from latest host packet
void update_topbar_ui() {
    lv_label_set_text(lbl_time, state.time_str);
    if (state.date_str[0]) lv_label_set_text(lbl_date, state.date_str);
}
