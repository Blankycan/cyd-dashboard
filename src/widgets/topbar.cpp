#include "topbar.h"
#include "../layout.h"
#include "../state.h"
#include "../theme.h"
#include "../ui_helpers.h"

static lv_obj_t *lbl_time = nullptr;
static lv_obj_t *lbl_date = nullptr;
static lv_obj_t *menu_btn = nullptr;
static lv_obj_t *lbl_menu = nullptr;

// Dark panel at top of screen with time and date labels
void build_topbar(lv_obj_t *scr) {
    lv_obj_t *tb = make_panel(scr, 0, 0, SCREEN_W, TOPBAR_H, COL_TOPBAR_BG);
    make_hdiv(scr, TOPBAR_H, 0, SCREEN_W, COL_DIVIDER);

    lbl_time = lv_label_create(tb);
    lv_label_set_text(lbl_time, "--:--");
    lv_obj_set_style_text_color(lbl_time, COL_TOPBAR_CLOCK, 0);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_20, 0);
    lv_obj_align(lbl_time, LV_ALIGN_LEFT_MID, 8, 0);

    lbl_date = lv_label_create(tb);
    lv_label_set_text(lbl_date, "");
    lv_obj_set_style_text_color(lbl_date, COL_TOPBAR_DATE, 0);
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_date, LV_ALIGN_RIGHT_MID, -TOPBAR_MENU_BTN_W, 0);

    // Settings menu button: the whole right end of the bar, icon centred in it
    menu_btn = lv_obj_create(tb);
    lv_obj_remove_style_all(menu_btn);
    lv_obj_set_size(menu_btn, TOPBAR_MENU_BTN_W, TOPBAR_H);
    lv_obj_align(menu_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_flag(menu_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(menu_btn, LV_OBJ_FLAG_SCROLLABLE);

    lbl_menu = lv_label_create(menu_btn);
    lv_obj_set_style_text_color(lbl_menu, COL_TOPBAR_MENU, 0);
    lv_obj_set_style_text_font(lbl_menu, &lv_font_montserrat_16, 0);
    topbar_set_menu_open(false);
}

lv_obj_t *topbar_menu_button() {
    return menu_btn;
}

void topbar_set_menu_open(bool open) {
    lv_label_set_text(lbl_menu, open ? LV_SYMBOL_CLOSE : LV_SYMBOL_SETTINGS);
    lv_obj_center(lbl_menu);
}

// Refresh clock and date from latest host packet
void update_topbar_ui() {
    lv_label_set_text(lbl_time, state.time_str);
    if (state.date_str[0]) lv_label_set_text(lbl_date, state.date_str);
}
