#pragma once
#include "scene.h"

// Every available scene. Each scene file adds itself with one line next to its
// `const Scene scene_<name>` definition:
//
//     SCENE_REGISTER(scene_lofi, "Lofi girl");
//
// and from then on it shows up in the settings menu and the rotation, with no
// list to keep up to date anywhere else. Scenes are listed alphabetically by
// title. Which ones are switched on is stored by Scene::name (see settings.h),
// so a new scene starts out switched on and doesn't disturb the others.

static const int SCENE_REGISTRY_MAX = 32;

struct SceneEntry {
    const Scene *scene;
    const char  *title;   // shown in the menu
};

int               scene_count();
const SceneEntry &scene_entry(int i);   // 0 .. scene_count()-1, sorted by title
int               scene_index(const Scene *s);   // -1 if not registered

// Implementation detail of SCENE_REGISTER: runs before setup(), from each
// scene file's static initialisation.
struct SceneRegistrar {
    SceneRegistrar(const Scene *scene, const char *title);
};
#define SCENE_REGISTER(sym, title) static SceneRegistrar sym##_registrar(&sym, title)
