#pragma once
#include <lvgl.h>

// Calendar panel — the current or next meeting with a live countdown, the one
// after it, and a timeline of the whole day. Driven by state.cal_events and
// the firmware clock, so it keeps counting down between packets; call
// update_calendar_ui() on each packet and on each clock tick.

void build_calendar_panel(lv_obj_t *parent);
void update_calendar_ui();

// True while a meeting is on or starts within CAL_QUIET_MIN — the scene
// player keeps the calendar up instead of playing scenes.
bool calendar_wants_focus();

// True if a meeting is still on or yet to start today. With none left the
// scene player stops showing the calendar between scenes. Also true while
// the time isn't known yet, so nothing is skipped before the first packet.
bool calendar_has_more_today();
