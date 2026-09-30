#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Starfield — flying through space: stars stream out from a vanishing point,
// brightening as they get closer and trailing streaks that stretch with the
// speed, so a boost reads as a jump to warp. They respawn in the distance
// until asked to stop, then the remaining stars fly past and it reports done.
// Touch: press for a warp boost; drag to steer the vanishing point.
// Events: surges forward as you type, and pulses on the music's beat.

static const int   MAX_STARS  = 48;
static const float BASE_SPEED = 0.22f;   // depth units per second (depth runs 1 → 0)
static const float NEAR_Z     = 0.06f;
static const float TRAIL      = 1.8f;    // streak length, in frames of travel

struct Star {
    bool  alive;
    float x, y, z;         // x, y in [-1, 1] scaled to the area at z = 1
    int   bx, by, bw, bh;  // what was drawn last frame, to erase
};

static PixFb fb;
static Star  stars[MAX_STARS];
static int   count, area_w, area_h;
static bool  stopping, oom;
static float cx, cy, target_cx, target_cy;   // vanishing point, eases toward target
static float boost;

static void spawn(Star &s, bool anywhere) {
    s.alive = true;
    s.x = scene_randf(-1, 1);
    s.y = scene_randf(-1, 1);
    s.z = anywhere ? scene_randf(0.15f, 1.0f) : 1.0f;
}

static void start(lv_obj_t *area, int w, int h) {
    area_w = w;
    area_h = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_BG);
    count = w * h / 450;
    if (count > MAX_STARS) count = MAX_STARS;
    if (count < 8) count = 8;
    cx = target_cx = w / 2.0f;
    cy = target_cy = h / 2.0f;
    boost    = 0;
    stopping = false;
    for (int i = 0; i < count; i++) {
        spawn(stars[i], true);
        stars[i].bw = 0;
    }
}

static void project(const Star &s, float z, float &px, float &py) {
    px = cx + s.x / z * area_w * 0.175f;
    py = cy + s.y / z * area_h * 0.175f;
}

static void tick(uint32_t dt_ms) {
    if (oom) return;
    float dt = dt_ms / 1000.0f;
    boost *= expf(-1.5f * dt);
    float ease = 1.0f - expf(-3.0f * dt);
    cx += (target_cx - cx) * ease;
    cy += (target_cy - cy) * ease;
    float dz = BASE_SPEED * (1.0f + boost) * dt;

    // Erase everything before drawing anything (see pixfb.h)
    lv_color_t bg = COL_SCENE_BG;
    for (int i = 0; i < count; i++) {
        Star &s = stars[i];
        if (s.bw) { pixfb_fill(fb, s.bx, s.by, s.bw, s.bh, bg); s.bw = 0; }
    }

    for (int i = 0; i < count; i++) {
        Star &s = stars[i];
        if (!s.alive) continue;
        s.z -= dz;
        float hx, hy;
        project(s, s.z, hx, hy);
        if (s.z < NEAR_Z || hx < -4 || hx > area_w + 4 || hy < -4 || hy > area_h + 4) {
            if (stopping) s.alive = false;
            else          spawn(s, false);
            continue;
        }
        // Tail: where the star was a moment ago, so the streak grows with speed
        float tx, ty;
        project(s, fminf(1.0f, s.z + dz * TRAIL), tx, ty);

        float near = 1.0f - s.z;   // 0 far → ~1 close
        lv_color_t head = near < 0.5f ? pixfb_mix(COL_SCENE_STARS_FAR, COL_SCENE_STARS_MID, near * 2)
                                      : pixfb_mix(COL_SCENE_STARS_MID, COL_SCENE_STARS_NEAR, near * 2 - 1);
        lv_color_t tail = pixfb_mix(bg, head, 0.55f);
        int ix = (int)hx, iy = (int)hy, jx = (int)tx, jy = (int)ty;
        if (jx != ix || jy != iy) pixfb_line(fb, jx, jy, ix, iy, tail);
        int size = s.z > 0.66f ? 1 : s.z > 0.33f ? 2 : 3;
        int ox = ix - size / 2, oy = iy - size / 2;
        pixfb_fill(fb, ox, oy, size, size, head);

        int x0 = LV_MIN(jx, ox), y0 = LV_MIN(jy, oy);
        int x1 = LV_MAX(jx, ox + size - 1), y1 = LV_MAX(jy, oy + size - 1);
        s.bx = x0; s.by = y0; s.bw = x1 - x0 + 1; s.bh = y1 - y0 + 1;
    }
    pixfb_flush(fb);
}

static void request_stop() { stopping = true; }

static bool is_done() {
    if (oom) return true;
    if (!stopping) return false;
    for (int i = 0; i < count; i++) if (stars[i].alive) return false;
    return true;
}

static void touch(SceneTouch type, int x, int y) {
    if (type == SCENE_TOUCH_PRESS) boost += 4.0f;
    if (type == SCENE_TOUCH_RELEASE) {
        target_cx = area_w / 2.0f;
        target_cy = area_h / 2.0f;
    } else {
        target_cx = x;
        target_cy = y;
    }
}

static void event(const SceneEvent &e) {
    if (e.type == SCENE_EV_KEY)  boost += 0.4f;
    if (e.type == SCENE_EV_BEAT) boost += 1.5f * e.strength / 100.0f;
}

static void finish() { pixfb_free(fb); }

const Scene scene_stars = { "stars", start, tick, request_stop, is_done, touch, finish, event };
