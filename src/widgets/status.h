#pragma once
#include <lvgl.h>

// Status bar — connection state dot, idle/active label, last-active time, and host IP.

void build_status_panel(lv_obj_t *parent);
void update_status_ui();
