#include "registry.h"
#include "../theme.h"
#include <math.h>

// Starfield — flying through space: stars stream out from a vanishing point,
// growing and brightening as they get closer. They respawn in the distance
// until asked to stop, then the remaining stars fly past and it reports done.
// Touch: press for a warp boost; drag to steer the vanishing point.

static const int   MAX_STARS  = 48;
static const float BASE_SPEED = 0.22f;   // depth units per second (depth runs 1 → 0)
static const float NEAR_Z     = 0.06f;

struct Star {
    lv_obj_t *obj;
    bool  alive;
    float x, y, z;   // x, y in [-1, 1] scaled to the area at z = 1
    int   size;
};

static Star  stars[MAX_STARS];
static int   count, area_w, area_h;
static bool  stopping;
static float cx, cy, target_cx, target_cy;   // vanishing point, eases toward target
static float boost;

static void spawn(Star &s, bool anywhere) {
    s.alive = true;
    s.x = scene_randf(-1, 1);
    s.y = scene_randf(-1, 1);
    s.z = anywhere ? scene_randf(0.15f, 1.0f) : 1.0f;
    s.size = 0;   // forces a style update on first draw
    lv_obj_clear_flag(s.obj, LV_OBJ_FLAG_HIDDEN);
}

static void start(lv_obj_t *area, int w, int h) {
    area_w = w;
    area_h = h;
    count = w * h / 450;
    if (count > MAX_STARS) count = MAX_STARS;
    if (count < 8) count = 8;
    cx = target_cx = w / 2.0f;
    cy = target_cy = h / 2.0f;
    boost    = 0;
    stopping = false;

    for (int i = 0; i < count; i++) {
        Star &s = stars[i];
        s.obj = lv_obj_create(area);
        lv_obj_remove_style_all(s.obj);
        lv_obj_set_style_bg_opa(s.obj, LV_OPA_COVER, 0);
        lv_obj_clear_flag(s.obj, LV_OBJ_FLAG_SCROLLABLE);
        spawn(s, true);
    }
}

static void tick(uint32_t dt_ms) {
    float dt = dt_ms / 1000.0f;
    boost *= expf(-1.5f * dt);
    float ease = 1.0f - expf(-3.0f * dt);
    cx += (target_cx - cx) * ease;
    cy += (target_cy - cy) * ease;
    float dz = BASE_SPEED * (1.0f + boost) * dt;
    // The field spans the whole area at z = 1 and flies outward from there
    float sx_scale = area_w * 0.5f, sy_scale = area_h * 0.5f;

    for (int i = 0; i < count; i++) {
        Star &s = stars[i];
        if (!s.alive) continue;
        s.z -= dz;
        float px = cx + s.x / s.z * sx_scale * 0.35f;
        float py = cy + s.y / s.z * sy_scale * 0.35f;
        if (s.z < NEAR_Z || px < -4 || px > area_w + 4 || py < -4 || py > area_h + 4) {
            if (stopping) { s.alive = false; lv_obj_add_flag(s.obj, LV_OBJ_FLAG_HIDDEN); }
            else          spawn(s, false);
            continue;
        }
        int size = s.z > 0.66f ? 1 : s.z > 0.33f ? 2 : 3;
        if (size != s.size) {
            s.size = size;
            lv_obj_set_size(s.obj, size, size);
            lv_obj_set_style_bg_color(s.obj, size == 1 ? COL_SCENE_STARS_FAR
                                           : size == 2 ? COL_SCENE_STARS_MID
                                           :             COL_SCENE_STARS_NEAR, 0);
        }
        lv_obj_set_pos(s.obj, (int)px - size / 2, (int)py - size / 2);
    }
}

static void request_stop() { stopping = true; }

static bool is_done() {
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

const Scene scene_stars = { "stars", start, tick, request_stop, is_done, touch, nullptr };
