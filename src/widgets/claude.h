#pragma once
#include <lvgl.h>

// Claude usage panel — header row with today's session count and live
// working-session dots, followed by the 5h / 7d rate-limit bars.

void build_claude_panel(lv_obj_t *parent);
void update_claude_ui();
