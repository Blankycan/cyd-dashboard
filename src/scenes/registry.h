#pragma once
#include "scene.h"

// Every available scene, and the SCENE_* names CYD_SCENES in config.h uses.

extern const Scene scene_leaves;   // falling autumn leaves; tap to blow them away
extern const Scene scene_snow;     // drifting snowflakes; tap for a gust
extern const Scene scene_life;     // Conway's Game of Life; draw cells with your finger
extern const Scene scene_stars;    // starfield flight; press to warp, drag to steer
extern const Scene scene_fish;     // fish tank; tap to drop food
extern const Scene scene_quote;    // quote of the day, typed out; tap for another

#define SCENE_LEAVES  (&scene_leaves)
#define SCENE_SNOW    (&scene_snow)
#define SCENE_LIFE    (&scene_life)
#define SCENE_STARS   (&scene_stars)
#define SCENE_FISH    (&scene_fish)
#define SCENE_QUOTE   (&scene_quote)
