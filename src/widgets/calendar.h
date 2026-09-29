#pragma once
#include <lvgl.h>

// Calendar panel — the current or next meeting with a live countdown, the one
// after it, and a timeline of the whole day. Driven by state.cal_events and
// the firmware clock, so it keeps counting down between packets; call
// update_calendar_ui() on each packet and on each clock tick.

void build_calendar_panel(lv_obj_t *parent);
void update_calendar_ui();
