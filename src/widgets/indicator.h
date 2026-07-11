#pragma once
#include <lvgl.h>

// Bottom status bar — connection state dot, idle/active label, and host IP.

void build_indicator_panel(lv_obj_t *parent);
void update_status_ui();
