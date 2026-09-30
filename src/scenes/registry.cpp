#include "registry.h"
#include <string.h>

// Filled during static initialisation, in whatever order the linker runs the
// scene files' registrars, and sorted on first use. Plain zero-initialised
// storage, so it's ready before any registrar runs.
static SceneEntry entries[SCENE_REGISTRY_MAX];
static int        n_entries;
static bool       sorted;

SceneRegistrar::SceneRegistrar(const Scene *scene, const char *title) {
    if (n_entries < SCENE_REGISTRY_MAX) entries[n_entries++] = { scene, title };
}

static void sort_entries() {
    if (sorted) return;
    sorted = true;
    for (int i = 1; i < n_entries; i++)   // insertion sort: a dozen or so entries, once
        for (int j = i; j > 0 && strcasecmp(entries[j].title, entries[j - 1].title) < 0; j--) {
            SceneEntry t = entries[j]; entries[j] = entries[j - 1]; entries[j - 1] = t;
        }
}

int scene_count() {
    sort_entries();
    return n_entries;
}

const SceneEntry &scene_entry(int i) {
    sort_entries();
    return entries[i];
}

int scene_index(const Scene *s) {
    sort_entries();
    for (int i = 0; i < n_entries; i++)
        if (entries[i].scene == s) return i;
    return -1;
}
