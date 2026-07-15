#pragma once
#include <lvgl.h>

// System panel — CPU, RAM, and typing speed (WPM) with progress bars.

void build_system_panel(lv_obj_t *parent);
void update_system_ui();
void system_show_disconnected();  // reset labels to "--" when host stops sending data
