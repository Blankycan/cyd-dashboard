#pragma once
#include <lvgl.h>

// Pluggable animated companion character (e.g. poring) that reacts to activity.
// Each companion implements this vtable; swap by changing which one is wired in.

struct Companion {
    void (*build)(lv_obj_t *parent, int w, int h);
    void (*update)(int wpm, bool active, bool music_playing, bool connected);
    void (*on_touch)();   // user tapped the companion area
    void (*on_sleep)();   // display entering sleep overlay
    void (*on_wake)();    // display waking up
};
