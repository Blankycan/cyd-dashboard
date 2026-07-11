#pragma once
#include <lvgl.h>

// Now-playing panel — track info plus a small animated icon on the right.
// When nothing is playing, a fun message is shown instead (if connected to companion app).

void build_music_panel(lv_obj_t *parent);
void update_music_ui();
