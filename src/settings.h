#pragma once
#include <stdint.h>

// Settings changed on the board in the menu (widgets/menu.cpp), kept in the
// ESP32's NVS flash so they survive reboots and reflashes (a normal upload
// doesn't erase NVS). First-boot defaults come from config.h.
//
// Scenes are stored by Scene::name as a list of the ones switched *off*, so a
// newly added scene starts out on and renaming a title changes nothing.

struct Settings {
    bool    scenes_on;     // play scenes at all
    bool    cal_between;   // calendar for CAL_PEEK_MS between scenes
    bool    shuffle;
    uint8_t scene_min;     // minutes per scene, 1 .. SCENE_MIN_MAX
};
extern Settings settings;

void settings_load();                          // once in setup()
void settings_save();                          // writes only if something changed
bool settings_scene_on(int registry_idx);
void settings_set_scene_on(int registry_idx, bool on);
void settings_touch();                         // mark changed after editing `settings` directly
