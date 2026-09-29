#include "registry.h"
#include "../theme.h"
#include <math.h>

// Snow — round flakes in two depth layers: small, dim, slow ones far away and
// larger, brighter, faster ones up close, each swaying gently. Flakes respawn
// at the top until asked to stop, then the rest settle out and it reports done.
// Touch: a press or drag puffs nearby flakes away from your finger.

static const int   MAX_FLAKES  = 40;
static const float PUFF_RADIUS = 50.0f;
static const float PUFF_POWER  = 70.0f;

struct Flake {
    lv_obj_t *obj;
    bool  alive;
    bool  near;
    float x, y, vy;
    float push_vx, push_vy;
    float sway_amp, sway_phase, sway_speed;
};

static Flake flakes[MAX_FLAKES];
static int   count, area_w, area_h;
static bool  stopping;
static float spawn_acc, spawn_every_ms;

static void spawn(Flake &f, bool anywhere) {
    f.alive      = true;
    f.near       = random(0, 3) == 0;   // a third in the near layer
    int size     = f.near ? 3 : 2;
    f.x          = scene_randf(0, area_w);
    f.y          = anywhere ? scene_randf(0, area_h) : -(float)size;
    f.vy         = f.near ? scene_randf(14, 22) : scene_randf(6, 11);
    f.push_vx    = f.push_vy = 0;
    f.sway_amp   = f.near ? scene_randf(3, 7) : scene_randf(1, 4);
    f.sway_phase = scene_randf(0, 6.28f);
    f.sway_speed = scene_randf(0.6f, 1.4f);
    lv_obj_set_size(f.obj, size, size);
    lv_obj_set_style_bg_color(f.obj, f.near ? COL_SCENE_SNOW_NEAR : COL_SCENE_SNOW_FAR, 0);
    lv_obj_clear_flag(f.obj, LV_OBJ_FLAG_HIDDEN);
}

static void start(lv_obj_t *area, int w, int h) {
    area_w = w;
    area_h = h;
    count = w * h / 600;
    if (count > MAX_FLAKES) count = MAX_FLAKES;
    if (count < 6) count = 6;
    spawn_every_ms = (h / 12.0f) * 1000.0f / count;
    spawn_acc      = 0;
    stopping       = false;

    for (int i = 0; i < count; i++) {
        Flake &f = flakes[i];
        f.obj = lv_obj_create(area);
        lv_obj_remove_style_all(f.obj);
        lv_obj_set_style_bg_opa(f.obj, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(f.obj, LV_RADIUS_CIRCLE, 0);
        lv_obj_clear_flag(f.obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(f.obj, LV_OBJ_FLAG_HIDDEN);
        f.alive = false;
    }
    // Start with a light flurry already in the air, the rest drift in from the top
    for (int i = 0; i < count / 3; i++) spawn(flakes[i], true);
}

static void tick(uint32_t dt_ms) {
    float dt = dt_ms / 1000.0f;

    if (!stopping) {
        spawn_acc += dt_ms;
        if (spawn_acc >= spawn_every_ms) {
            for (int i = 0; i < count; i++) {
                if (!flakes[i].alive) { spawn(flakes[i], false); spawn_acc = 0; break; }
            }
        }
    }

    float decay = expf(-2.5f * dt);
    for (int i = 0; i < count; i++) {
        Flake &f = flakes[i];
        if (!f.alive) continue;
        f.sway_phase += f.sway_speed * dt;
        f.x += f.push_vx * dt;
        f.y += (f.vy + f.push_vy) * dt;
        f.push_vx *= decay;
        f.push_vy *= decay;

        float sx = f.x + f.sway_amp * sinf(f.sway_phase);
        if (f.y > area_h + 3 || sx < -10 || sx > area_w + 10) {
            if (stopping) { f.alive = false; lv_obj_add_flag(f.obj, LV_OBJ_FLAG_HIDDEN); }
            else          spawn(f, false);
            continue;
        }
        lv_obj_set_pos(f.obj, (int)sx, (int)f.y);
    }
}

static void request_stop() { stopping = true; }

static bool is_done() {
    if (!stopping) return false;
    for (int i = 0; i < count; i++) if (flakes[i].alive) return false;
    return true;
}

static void touch(SceneTouch type, int x, int y) {
    if (type == SCENE_TOUCH_RELEASE) return;
    float power = (type == SCENE_TOUCH_PRESS) ? PUFF_POWER : PUFF_POWER * 0.3f;
    for (int i = 0; i < count; i++) {
        Flake &f = flakes[i];
        if (!f.alive) continue;
        float dx = f.x + f.sway_amp * sinf(f.sway_phase) - x, dy = f.y - y;
        float d  = sqrtf(dx * dx + dy * dy);
        if (d >= PUFF_RADIUS || d < 0.01f) continue;
        float k = power * (1.0f - d / PUFF_RADIUS) * (f.near ? 1.0f : 0.6f);
        f.push_vx += k * dx / d;
        f.push_vy += k * dy / d;
    }
}

const Scene scene_snow = { "snow", start, tick, request_stop, is_done, touch, nullptr };
