#pragma once
#include "scene.h"

// Every available scene, and the SCENE_* names CYD_SCENES in config.h uses.

extern const Scene scene_leaves;   // falling autumn leaves; tap to blow them away
extern const Scene scene_snow;     // drifting snowflakes; tap for a gust
extern const Scene scene_life;     // Conway's Game of Life; draw cells with your finger

#define SCENE_LEAVES  (&scene_leaves)
#define SCENE_SNOW    (&scene_snow)
#define SCENE_LIFE    (&scene_life)
