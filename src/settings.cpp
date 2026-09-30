#include "settings.h"
#include "config.h"
#include "scenes/registry.h"
#include <Arduino.h>
#include <Preferences.h>

Settings settings = { SCENES_DEFAULT_ON, CAL_BETWEEN_DEFAULT, SCENE_SHUFFLE_DEFAULT, SCENE_MIN_DEFAULT };

static bool scene_on[SCENE_REGISTRY_MAX];
static bool dirty;

static const char *NVS_NS = "cyd";

void settings_load() {
    for (int i = 0; i < SCENE_REGISTRY_MAX; i++) scene_on[i] = true;

    Preferences p;
    if (!p.begin(NVS_NS, true)) return;   // nothing saved yet: defaults
    settings.scenes_on   = p.getBool("scenes", settings.scenes_on);
    settings.cal_between = p.getBool("cal_between", settings.cal_between);
    settings.shuffle     = p.getBool("shuffle", settings.shuffle);
    settings.scene_min   = constrain(p.getUChar("scene_min", settings.scene_min), 1, SCENE_MIN_MAX);

    // "fish,life," — names of the scenes switched off
    String off = p.getString("off", "");
    p.end();
    for (int i = 0; i < scene_count(); i++) {
        String key = String(scene_entry(i).scene->name) + ",";
        if (off.startsWith(key) || off.indexOf("," + key) >= 0) scene_on[i] = false;
    }
}

void settings_save() {
    if (!dirty) return;
    dirty = false;
    String off;
    for (int i = 0; i < scene_count(); i++)
        if (!scene_on[i]) { off += scene_entry(i).scene->name; off += ","; }

    Preferences p;
    if (!p.begin(NVS_NS, false)) return;
    p.putBool("scenes", settings.scenes_on);
    p.putBool("cal_between", settings.cal_between);
    p.putBool("shuffle", settings.shuffle);
    p.putUChar("scene_min", settings.scene_min);
    p.putString("off", off);
    p.end();
    Serial.println("{\"log\":\"settings saved\"}");
}

bool settings_scene_on(int i) {
    return i >= 0 && i < SCENE_REGISTRY_MAX && scene_on[i];
}

void settings_set_scene_on(int i, bool on) {
    if (i < 0 || i >= SCENE_REGISTRY_MAX || scene_on[i] == on) return;
    scene_on[i] = on;
    dirty = true;
}

void settings_touch() {
    dirty = true;
}
