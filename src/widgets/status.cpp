#include "status.h"
#include "../layout.h"
#include "../state.h"
#include "../theme.h"
#include "../ui_helpers.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *dot_status      = nullptr;
static lv_obj_t *lbl_status      = nullptr;
static lv_obj_t *lbl_last_active = nullptr;
static lv_obj_t *lbl_keys        = nullptr;
static lv_obj_t *lbl_ip          = nullptr;

// Left cluster: status dot, state label, last-active time.
// Right cluster: keystrokes today, IP address.
// Labels in each cluster chain off their neighbour, so they're re-aligned
// whenever their text (and thus width) changes.
void build_status_panel(lv_obj_t *parent) {
    lbl_status = lv_label_create(parent);
    lv_label_set_text(lbl_status, "offline");
    lv_obj_set_style_text_color(lbl_status, COL_STATUS_OFFLINE, 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_status, LV_ALIGN_LEFT_MID, 20, 0);

    dot_status = make_dot(parent, 8, 0, COL_STATUS_OFFLINE);
    align_dot_to_label(dot_status, lbl_status);

    lbl_last_active = lv_label_create(parent);
    lv_label_set_text(lbl_last_active, "");
    lv_obj_set_style_text_color(lbl_last_active, COL_STATUS_IDLE_TIME, 0);
    lv_obj_set_style_text_font(lbl_last_active, &lv_font_montserrat_12, 0);
    lv_obj_align_to(lbl_last_active, lbl_status, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    lbl_ip = lv_label_create(parent);
    lv_label_set_text(lbl_ip, "");
    lv_obj_set_style_text_color(lbl_ip, COL_STATUS_IP, 0);
    lv_obj_set_style_text_font(lbl_ip, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl_ip, LV_ALIGN_RIGHT_MID, -8, 0);

    lbl_keys = lv_label_create(parent);
    lv_label_set_text(lbl_keys, "");
    lv_obj_set_style_text_color(lbl_keys, COL_STATUS_KEYS, 0);
    lv_obj_set_style_text_font(lbl_keys, &lv_font_montserrat_12, 0);
    lv_obj_align_to(lbl_keys, lbl_ip, LV_ALIGN_OUT_LEFT_MID, -12, 0);
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
    lv_obj_align_to(lbl_last_active, lbl_status, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    lv_label_set_text(lbl_ip, state.ip_str);

    char num[12], buf[20];
    if (state.connected) fmt_k(num, sizeof(num), state.keys_today);
    else                 strlcpy(num, "--", sizeof(num));
    snprintf(buf, sizeof(buf), LV_SYMBOL_KEYBOARD " %s", num);
    lv_label_set_text(lbl_keys, buf);
    lv_obj_align_to(lbl_keys, lbl_ip, LV_ALIGN_OUT_LEFT_MID, -12, 0);
}
