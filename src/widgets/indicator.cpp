#include "indicator.h"
#include "../layout.h"
#include "../state.h"
#include "../theme.h"

static lv_obj_t *dot_status = nullptr;
static lv_obj_t *lbl_status = nullptr;
static lv_obj_t *lbl_ip     = nullptr;

// Status dot (left), text label (center-left), IP address (right)
void build_indicator_panel(lv_obj_t *parent) {
    dot_status = lv_obj_create(parent);
    lv_obj_remove_style_all(dot_status);
    lv_obj_set_size(dot_status, 6, 6);
    lv_obj_set_pos(dot_status, 8, (INDICATOR_H - 6) / 2);
    lv_obj_set_style_bg_color(dot_status, COL_ALERT, 0);
    lv_obj_set_style_bg_opa(dot_status, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot_status, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot_status, 0, 0);
    lv_obj_clear_flag(dot_status, LV_OBJ_FLAG_SCROLLABLE);

    lbl_status = lv_label_create(parent);
    lv_label_set_text(lbl_status, "offline");
    lv_obj_set_style_text_color(lbl_status, COL_ALERT, 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_status, 20, (INDICATOR_H - 14) / 2);

    lbl_ip = lv_label_create(parent);
    lv_label_set_text(lbl_ip, "");
    lv_obj_set_style_text_color(lbl_ip, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(lbl_ip, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(lbl_ip, PANEL_W / 2, (INDICATOR_H - 12) / 2);
    lv_obj_set_width(lbl_ip, PANEL_W / 2 - 8);
}

// Update connection dot color and label: offline / idle / active
void update_status_ui() {
    lv_color_t col;
    const char *text;
    if (!state.connected) {
        col  = COL_ALERT;
        text = "offline";
    } else if (state.active) {
        col  = COL_OK;
        text = "active";
    } else {
        col  = COL_TEXT_DIM;
        text = "idle";
    }
    lv_obj_set_style_bg_color(dot_status, col, 0);
    lv_label_set_text(lbl_status, text);
    lv_obj_set_style_text_color(lbl_status, col, 0);
    lv_label_set_text(lbl_ip, state.ip_str);
}
