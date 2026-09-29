#pragma once
#include <lvgl.h>
#include "scene.h"

// Scene player — shares the calendar's slot between the calendar and the
// ambient scenes in CYD_SCENES. While no meeting is near it alternates
// scene (SCENE_SHOW_MS) → calendar (CAL_PEEK_MS) → next scene; from
// CAL_QUIET_MIN before a meeting until it ends, the calendar stays up.
//
// Touch: while a scene shows, touches inside it go to the scene and a tap
// anywhere else flips to the calendar; while the calendar shows, a tap
// anywhere starts the next scene (so tapping twice skips a scene).

// Create the scene area as a sibling covering `calendar_panel`'s rect.
void scene_player_init(lv_obj_t *calendar_panel);

// Feed raw touch input in screen coordinates. Only queues the event, so it's
// safe to call from the LVGL touch read callback.
void scene_player_touch(SceneTouch type, int x, int y);

// Freeze scenes and the rotation timers (e.g. while the display sleeps).
void scene_player_set_paused(bool paused);
