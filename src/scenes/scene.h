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
//   event(e)           optional; things happening on the host — key presses,
//                      music starting/stopping, beats — see SceneEvent below.
//                      Events only report changes; scene_ctx() has the current
//                      state (e.g. whether music is already playing at start()).
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

// --- Events -----------------------------------------------------------------
enum SceneEventType {
    SCENE_EV_KEY,      // a key was pressed on the host: .key
    SCENE_EV_MUSIC,    // playback state changed: .music
    SCENE_EV_TRACK,    // a different track started
    SCENE_EV_BEAT,     // a beat just hit in the playing music: .strength (1-100)
    SCENE_EV_AUDIO,    // new intensity/bass/tempo numbers (~10/s) — read them from scene_ctx()
    SCENE_EV_CLAUDE,   // number of Claude sessions working changed: .value = new count
    SCENE_EV_HOUR,     // the clock ticked over into a new hour: .value = 0-23
};
// Key categories only: the host never says which key, so nothing typed leaves it
enum SceneKey   { SCENE_KEY_CHAR, SCENE_KEY_SPACE, SCENE_KEY_ENTER, SCENE_KEY_BACKSPACE,
                  SCENE_KEY_MODIFIER, SCENE_KEY_OTHER };
enum SceneMusic { SCENE_MUSIC_NONE, SCENE_MUSIC_PAUSED, SCENE_MUSIC_PLAYING };

struct SceneEvent {
    SceneEventType type;
    SceneKey       key;
    SceneMusic     music;
    uint8_t        strength;
    int            value;
};

// Current host state, kept up to date by the player whether or not a scene
// is running. Beat/tempo fields stay 0 unless the companion's audio energy
// detection is enabled and music is playing.
struct SceneContext {
    SceneMusic music;
    uint8_t    intensity;      // 0-100, relative to the song itself (volume-independent)
    uint8_t    bass;           // 0-100
    uint16_t   bpm;            // estimated tempo, 0 = unknown
    uint32_t   last_beat_ms;   // millis() of the latest beat, 0 = none yet
    bool       typing;         // a key was pressed in the last 1.5 s
    int        claude_working; // Claude sessions currently mid-turn
    int        hour, minute;   // local time, -1 until known
};
const SceneContext &scene_ctx();

struct Scene {
    const char *name;
    void (*start)(lv_obj_t *area, int w, int h);
    void (*tick)(uint32_t dt_ms);
    void (*request_stop)();
    bool (*is_done)();
    void (*touch)(SceneTouch type, int x, int y);   // may be nullptr
    void (*finish)();                               // may be nullptr
    void (*event)(const SceneEvent &e);             // may be nullptr (and may be left out)
};

// Uniform random float in [lo, hi)
inline float scene_randf(float lo, float hi) {
    return lo + (hi - lo) * (float)random(0, 1L << 24) / (float)(1L << 24);
}
