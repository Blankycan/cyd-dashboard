#pragma once
#include <lvgl.h>

struct Companion {
    void (*build)(lv_obj_t *parent, int w, int h);
    void (*update)(int wpm, bool active, bool music_playing, bool connected);
    void (*on_touch)();
    void (*on_sleep)();
    void (*on_wake)();
};
