#pragma once
#include "scene.h"

// Every available scene, and the SCENE_* names CYD_SCENES in config.h uses.

extern const Scene scene_leaves;   // falling autumn leaves; tap to blow them away
extern const Scene scene_snow;     // drifting snowflakes; tap for a gust
extern const Scene scene_life;     // Conway's Game of Life; draw cells with your finger
extern const Scene scene_stars;    // starfield flight; press to warp, drag to steer
extern const Scene scene_fish;     // fish tank; tap to drop food
extern const Scene scene_quote;    // quote of the day, typed out; tap for another
extern const Scene scene_invaders; // Space Invaders demo; drag to move, tap to fire
extern const Scene scene_asteroids;// Asteroids demo; hold to steer, thrust and fire
extern const Scene scene_pacman;   // Pac-Man demo in a generated mini maze; tap a side to steer
extern const Scene scene_synthwave;// sunset drive on a neon grid; steer by touch, grid pulses to the beat
extern const Scene scene_lofi;     // girl writing by a rainy window; nods to music, writes as you type
extern const Scene scene_fireworks;// fireworks over a skyline; launches on the beat, tap to launch
extern const Scene scene_campfire; // campfire under the stars; flames surge with the bass, tap to add a log
extern const Scene scene_city;     // night city skyline that follows the clock; tap buildings and sky
extern const Scene scene_lava;     // lava lamp; blobs swell with the bass, tap to add heat

#define SCENE_LEAVES  (&scene_leaves)
#define SCENE_SNOW    (&scene_snow)
#define SCENE_LIFE    (&scene_life)
#define SCENE_STARS   (&scene_stars)
#define SCENE_FISH    (&scene_fish)
#define SCENE_QUOTE   (&scene_quote)
#define SCENE_INVADERS  (&scene_invaders)
#define SCENE_ASTEROIDS (&scene_asteroids)
#define SCENE_PACMAN    (&scene_pacman)
#define SCENE_SYNTHWAVE (&scene_synthwave)
#define SCENE_LOFI      (&scene_lofi)
#define SCENE_FIREWORKS (&scene_fireworks)
#define SCENE_CAMPFIRE  (&scene_campfire)
#define SCENE_CITY      (&scene_city)
#define SCENE_LAVA      (&scene_lava)
