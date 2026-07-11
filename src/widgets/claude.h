#pragma once
#include <lvgl.h>

// Claude usage panel — token counts (top) and rate-limit bars (bottom),
// always visible side-by-side in a single expanded panel.

void build_claude_panel(lv_obj_t *parent);
void update_claude_ui();
