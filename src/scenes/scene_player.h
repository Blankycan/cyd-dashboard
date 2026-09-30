#pragma once
#include <lvgl.h>
#include "scene.h"

// Scene player — shares the calendar's slot between the calendar and the
// ambient scenes switched on in the settings menu (settings.h). With the
// calendar between scenes it alternates scene (settings.scene_min) → calendar
// (CAL_PEEK_MS) → next scene; without it, scenes follow each other directly,
// and a single switched-on scene just keeps playing. From CAL_QUIET_MIN
// before a meeting until it ends the calendar stays up, whatever the settings.
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

// Host events: updates scene_ctx() and passes the event to the running scene
// (queued, delivered on the next frame; dropped while paused).
void scene_player_event(const SceneEvent &e);
// Latest music energy numbers from the host (SCENE_EV_AUDIO)
void scene_player_audio(int intensity, int bass, int bpm);

// The settings changed: rebuild the rotation on the next frame. A running
// scene that's no longer in it (and wasn't picked by hand) makes way.
void scene_player_settings_changed();
// Play `s` now in place of whatever shows (the menu's preview). Counts as
// picked by hand, like a tap: it stays even if switched off in the menu, and
// if a meeting is already near it plays its full time rather than giving way.
void scene_player_play(const Scene *s);
// The scene showing right now, nullptr while the calendar shows
const Scene *scene_player_current();
