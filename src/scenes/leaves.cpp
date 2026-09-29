#include "registry.h"
#include "../theme.h"
#include <math.h>

// Falling leaves — capsule-shaped leaves that drift down, sway side to side,
// and "tumble" by squashing their width as they fall. They respawn at the top
// until asked to stop, then the rest fall out and the scene reports done.
// Touch: a press or drag blows nearby leaves away from your finger.

static const int   MAX_LEAVES   = 24;
static const float GUST_RADIUS  = 60.0f;
static const float GUST_POWER   = 90.0f;   // px/s given to a leaf right at the finger

struct Leaf {
    lv_obj_t *obj;
    bool  alive;
    float x, y;          // centre, px
    float vy;            // fall speed, px/s
    float push_vx, push_vy;   // gust velocity, decays over time
    float sway_amp, sway_phase, sway_speed;
    float tumble_phase, tumble_speed;
    int   w, h;
};

static Leaf  leaves[MAX_LEAVES];
static int   count, area_w, area_h;
static bool  stopping;
static float spawn_acc, spawn_every_ms;
static float wind, wind_target, wind_timer;

static lv_color_t leaf_color() {
    switch (random(0, 3)) {
        case 0:  return COL_SCENE_LEAVES_1;
        case 1:  return COL_SCENE_LEAVES_2;
        default: return COL_SCENE_LEAVES_3;
    }
}

static void spawn(Leaf &l) {
    l.alive        = true;
    l.w            = random(6, 10);
    l.h            = random(3, 5);
    l.x            = scene_randf(0, area_w);
    l.y            = -(float)l.h - scene_randf(0, 6);
    l.vy           = scene_randf(10, 20);
    l.push_vx      = l.push_vy = 0;
    l.sway_amp     = scene_randf(6, 16);
    l.sway_phase   = scene_randf(0, 6.28f);
    l.sway_speed   = scene_randf(1.2f, 2.4f);   // rad/s
    l.tumble_phase = scene_randf(0, 6.28f);
    l.tumble_speed = scene_randf(2.0f, 5.0f);
    lv_obj_set_style_bg_color(l.obj, leaf_color(), 0);
    lv_obj_set_style_radius(l.obj, l.h / 2, 0);
    lv_obj_set_height(l.obj, l.h);
    lv_obj_clear_flag(l.obj, LV_OBJ_FLAG_HIDDEN);
}

static void start(lv_obj_t *area, int w, int h) {
    area_w = w;
    area_h = h;
    // Density scales with the area so a fullscreen run isn't sparse
    count = w * h / 1000;
    if (count > MAX_LEAVES) count = MAX_LEAVES;
    if (count < 4) count = 4;
    // Fall time ≈ height / avg speed; spread spawns evenly across it
    spawn_every_ms = (h / 15.0f) * 1000.0f / count;
    spawn_acc      = spawn_every_ms;   // first leaf right away
    stopping       = false;
    wind = wind_target = 0;
    wind_timer = 0;

    for (int i = 0; i < count; i++) {
        Leaf &l = leaves[i];
        l.obj = lv_obj_create(area);
        lv_obj_remove_style_all(l.obj);
        lv_obj_set_style_bg_opa(l.obj, LV_OPA_COVER, 0);
        lv_obj_clear_flag(l.obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(l.obj, LV_OBJ_FLAG_HIDDEN);
        l.alive = false;
    }
}

static void tick(uint32_t dt_ms) {
    float dt = dt_ms / 1000.0f;

    // Slowly wandering breeze shared by every leaf
    wind_timer -= dt;
    if (wind_timer <= 0) { wind_target = scene_randf(-8, 8); wind_timer = scene_randf(3, 7); }
    wind += (wind_target - wind) * dt * 0.5f;

    if (!stopping) {
        spawn_acc += dt_ms;
        if (spawn_acc >= spawn_every_ms) {
            for (int i = 0; i < count; i++) {
                if (!leaves[i].alive) { spawn(leaves[i]); spawn_acc = 0; break; }
            }
        }
    }

    float decay = expf(-2.0f * dt);
    for (int i = 0; i < count; i++) {
        Leaf &l = leaves[i];
        if (!l.alive) continue;
        l.sway_phase   += l.sway_speed * dt;
        l.tumble_phase += l.tumble_speed * dt;
        l.x += (wind + l.push_vx) * dt;
        l.y += (l.vy + l.push_vy) * dt;
        l.push_vx *= decay;
        l.push_vy *= decay;

        float sx = l.x + l.sway_amp * sinf(l.sway_phase);
        if (l.y > area_h + 4 || sx < -20 || sx > area_w + 20) {
            if (stopping) { l.alive = false; lv_obj_add_flag(l.obj, LV_OBJ_FLAG_HIDDEN); }
            else          spawn(l);
            continue;
        }
        int tw = (int)(l.w * fabsf(cosf(l.tumble_phase)));
        if (tw < 2) tw = 2;
        lv_obj_set_width(l.obj, tw);
        lv_obj_set_pos(l.obj, (int)sx - tw / 2, (int)l.y - l.h / 2);
    }
}

static void request_stop() { stopping = true; }

static bool is_done() {
    if (!stopping) return false;
    for (int i = 0; i < count; i++) if (leaves[i].alive) return false;
    return true;
}

static void touch(SceneTouch type, int x, int y) {
    if (type == SCENE_TOUCH_RELEASE) return;
    float power = (type == SCENE_TOUCH_PRESS) ? GUST_POWER : GUST_POWER * 0.3f;
    for (int i = 0; i < count; i++) {
        Leaf &l = leaves[i];
        if (!l.alive) continue;
        float dx = l.x + l.sway_amp * sinf(l.sway_phase) - x, dy = l.y - y;
        float d  = sqrtf(dx * dx + dy * dy);
        if (d >= GUST_RADIUS || d < 0.01f) continue;
        float f = power * (1.0f - d / GUST_RADIUS);
        l.push_vx += f * dx / d;
        l.push_vy += f * dy / d - f * 0.3f;   // a little lift, so they flutter up
    }
}

const Scene scene_leaves = { "leaves", start, tick, request_stop, is_done, touch, nullptr };
