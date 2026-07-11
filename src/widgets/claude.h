#pragma once
#include <lvgl.h>

// Claude usage panel — token counts and rate-limit bars, rotating between views.

void build_claude_panel(lv_obj_t *parent);
void update_claude_ui();
void claude_start_timers();  // start view-rotation timers (no-op if CLAUDE_ROTATE_MS == 0)
