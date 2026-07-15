#include "status.h"
#include "../layout.h"
#include "../state.h"
#include "../theme.h"
#include "../ui_helpers.h"

static lv_obj_t *dot_status      = nullptr;
static lv_obj_t *lbl_status      = nullptr;
static lv_obj_t *lbl_last_active = nullptr;
static lv_obj_t *lbl_ip          = nullptr;

// Status dot (left), text label (center-left), IP address (right)
void build_status_panel(lv_obj_t *parent) {
    dot_status = make_dot(parent, 8, (INDICATOR_H - 6) / 2, COL_STATUS_OFFLINE);

    lbl_status = lv_label_create(parent);
    lv_label_set_text(lbl_status, "offline");
    lv_obj_set_style_text_color(lbl_status, COL_STATUS_OFFLINE, 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_status, 20, (INDICATOR_H - 14) / 2);

    lbl_last_active = lv_label_create(parent);
    lv_label_set_text(lbl_last_active, "");
    lv_obj_set_style_text_color(lbl_last_active, COL_STATUS_IDLE_TIME, 0);
    lv_obj_set_style_text_font(lbl_last_active, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_last_active, 80, (INDICATOR_H - 12) / 2);

    lbl_ip = lv_label_create(parent);
    lv_label_set_text(lbl_ip, "");
    lv_obj_set_style_text_color(lbl_ip, COL_STATUS_IP, 0);
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
        col  = COL_STATUS_OFFLINE;
        text = "offline";
    } else if (state.active) {
        col  = COL_STATUS_ACTIVE;
        text = "active";
    } else {
        col  = COL_STATUS_IDLE;
        text = "idle";
    }
    lv_obj_set_style_bg_color(dot_status, col, 0);
    lv_label_set_text(lbl_status, text);
    lv_obj_set_style_text_color(lbl_status, col, 0);

    // Show last active time when idle or offline; hide when actively typing
    if (state.active || state.last_active_str[0] == '\0') {
        lv_label_set_text(lbl_last_active, "");
    } else {
        lv_label_set_text(lbl_last_active, state.last_active_str);
    }

    lv_label_set_text(lbl_ip, state.ip_str);
}
