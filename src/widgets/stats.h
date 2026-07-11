#pragma once
#include <lvgl.h>

// System stats panel — CPU, RAM, and typing speed (WPM) with progress bars.

void build_stats_panel(lv_obj_t *parent);
void update_stats_ui();
void stats_show_disconnected();  // reset labels to "--" when host stops sending data
