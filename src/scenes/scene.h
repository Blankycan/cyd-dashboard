#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <stdint.h>

// =============================================================================
// Ambient scene plug-in interface
//
// A scene is a self-contained animation that the scene player (scene_player.h)
// runs in an area it hands over. The player knows nothing about what a scene
// draws; a scene knows nothing about the calendar, the playlist, or timing
// beyond the dt it's given. Lifecycle:
//
//   start(area, w, h)  build objects inside `area` (w×h px, origin top-left).
//                      Must cope with any size — the area may change later.
//   tick(dt_ms)        advance the animation; called every SCENE_FRAME_MS
//                      (not while the display sleeps)
//   request_stop()     the player wants the slot back: stop spawning, let
//                      what's on screen finish, then report is_done().
//                      If that takes longer than SCENE_STOP_GRACE_MS the
//                      player ends the scene anyway.
//   is_done()          true once finished — after request_stop(), or on its
//                      own (e.g. Game of Life running out of life)
//   touch(type, x, y)  optional; taps/drags inside the area, area-local coords
//   finish()           optional; free anything that isn't an LVGL object.
//                      The player has already deleted `area` and every object
//                      in it, so a scene never needs to clean up widgets.
//
// Only one scene runs at a time, so scenes keep their state in file-level
// statics and must fully reset it in start().
//
// To add a scene: create src/scenes/<name>.cpp defining `const Scene
// scene_<name>`, declare it and a SCENE_<NAME> macro in registry.h, and list
// it in CYD_SCENES in config.h. Colours go through COL_SCENE_<NAME>_* tokens
// with fallbacks in theme.h.
// =============================================================================

enum SceneTouch { SCENE_TOUCH_PRESS, SCENE_TOUCH_DRAG, SCENE_TOUCH_RELEASE };

struct Scene {
    const char *name;
    void (*start)(lv_obj_t *area, int w, int h);
    void (*tick)(uint32_t dt_ms);
    void (*request_stop)();
    bool (*is_done)();
    void (*touch)(SceneTouch type, int x, int y);   // may be nullptr
    void (*finish)();                               // may be nullptr
};

// Uniform random float in [lo, hi)
inline float scene_randf(float lo, float hi) {
    return lo + (hi - lo) * (float)random(0, 1L << 24) / (float)(1L << 24);
}
