#include "registry.h"
#include "../theme.h"
#include <math.h>

// Fish tank — a few fish wander between random spots, bobbing gently, with
// swaying seaweed along the bottom and bubbles rising from it. When asked to
// stop, the fish swim off the nearest side and the scene reports done once
// they're all gone.
// Touch: tap to drop a food pellet; it sinks, and the nearest fish swims over
// and eats it.
// Events: pressing Enter startles the fish into a quick dart.

static const int   MAX_FISH    = 5;
static const int   MAX_BUBBLES = 10;
static const int   MAX_WEEDS   = 10;
static const int   MAX_FOOD    = 4;
static const float FOOD_LIFE_S = 15.0f;

struct Fish {
    lv_obj_t *box, *body, *tail, *eye;
    bool  alive, facing_right;
    float x, y, vx, vy, tx, ty, speed, bob, wag;
    int   bw, bh, tw;
};

struct Bubble { lv_obj_t *obj; bool alive; float x, y, vy, phase; };
struct Weed   { lv_obj_t *obj; int base_x, h; float phase, speed; };
struct Food   { lv_obj_t *obj; bool alive; float x, y, age; };

static Fish   fish[MAX_FISH];
static Bubble bubbles[MAX_BUBBLES];
static Weed   weeds[MAX_WEEDS];
static Food   food[MAX_FOOD];
static int    n_fish, n_weeds, area_w, area_h;
static bool   stopping;
static float  bubble_acc;

static lv_obj_t *make_blob(lv_obj_t *parent, lv_color_t col, int radius) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_color(o, col, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_color_t fish_color(int i) {
    switch (i % 3) {
        case 0:  return COL_SCENE_FISH_1;
        case 1:  return COL_SCENE_FISH_2;
        default: return COL_SCENE_FISH_3;
    }
}

// Lay out tail, body and eye inside the fish's box for its heading
static void set_facing(Fish &f, bool right) {
    f.facing_right = right;
    int body_x = right ? f.tw - 1 : 0;
    int tail_x = right ? 0 : f.bw - 1;
    lv_obj_set_x(f.body, body_x);
    lv_obj_set_x(f.tail, tail_x);
    lv_obj_set_pos(f.eye, right ? body_x + f.bw - 4 : body_x + 2, f.bh / 3);
}

static void new_target(Fish &f) {
    f.tx = scene_randf(12, area_w - 12);
    f.ty = scene_randf(8, area_h - 14);
}

static void start(lv_obj_t *area, int w, int h) {
    area_w = w;
    area_h = h;
    stopping   = false;
    bubble_acc = 0;

    // Seaweed first so everything else draws in front of it
    n_weeds = w / 30;
    if (n_weeds > MAX_WEEDS) n_weeds = MAX_WEEDS;
    for (int i = 0; i < n_weeds; i++) {
        Weed &wd = weeds[i];
        wd.h      = random(h / 6, h / 3);
        wd.base_x = (i + 0.5f) * w / n_weeds + random(-8, 9);
        wd.phase  = scene_randf(0, 6.28f);
        wd.speed  = scene_randf(0.8f, 1.5f);
        wd.obj    = make_blob(area, COL_SCENE_FISH_WEED, 1);
        lv_obj_set_size(wd.obj, 3, wd.h);
        lv_obj_set_pos(wd.obj, wd.base_x, h - wd.h);
    }

    for (int i = 0; i < MAX_BUBBLES; i++) {
        Bubble &b = bubbles[i];
        b.obj = lv_obj_create(area);
        lv_obj_remove_style_all(b.obj);
        lv_obj_set_style_border_color(b.obj, COL_SCENE_FISH_BUBBLE, 0);
        lv_obj_set_style_border_width(b.obj, 1, 0);
        lv_obj_set_style_radius(b.obj, LV_RADIUS_CIRCLE, 0);
        lv_obj_clear_flag(b.obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(b.obj, LV_OBJ_FLAG_HIDDEN);
        b.alive = false;
    }

    for (int i = 0; i < MAX_FOOD; i++) {
        food[i].obj = make_blob(area, COL_SCENE_FISH_FOOD, 1);
        lv_obj_set_size(food[i].obj, 2, 2);
        lv_obj_add_flag(food[i].obj, LV_OBJ_FLAG_HIDDEN);
        food[i].alive = false;
    }

    n_fish = w * h / 5000;
    if (n_fish > MAX_FISH) n_fish = MAX_FISH;
    if (n_fish < 2) n_fish = 2;
    for (int i = 0; i < n_fish; i++) {
        Fish &f = fish[i];
        f.bw = random(10, 16);
        f.bh = random(5, 8);
        f.tw = f.bh - 1;
        f.alive = true;
        f.x = scene_randf(20, w - 20);
        f.y = scene_randf(10, h - 16);
        f.vx = f.vy = 0;
        f.speed = scene_randf(10, 20);
        f.bob = scene_randf(0, 6.28f);
        f.wag = scene_randf(0, 6.28f);
        new_target(f);

        f.box = lv_obj_create(area);
        lv_obj_remove_style_all(f.box);
        lv_obj_set_size(f.box, f.bw + f.tw - 1, f.bh);
        lv_obj_clear_flag(f.box, LV_OBJ_FLAG_SCROLLABLE);
        lv_color_t col = fish_color(i);
        f.tail = make_blob(f.box, col, 1);
        lv_obj_set_size(f.tail, f.tw, f.bh);
        f.body = make_blob(f.box, col, f.bh / 2);
        lv_obj_set_size(f.body, f.bw, f.bh);
        f.eye = make_blob(f.box, COL_SCENE_FISH_EYE, 1);
        lv_obj_set_size(f.eye, 2, 2);
        set_facing(f, f.tx > f.x);
    }
}

static void spawn_bubble(float x, float y) {
    for (int i = 0; i < MAX_BUBBLES; i++) {
        Bubble &b = bubbles[i];
        if (b.alive) continue;
        b.alive = true;
        b.x = x;
        b.y = y;
        b.vy = scene_randf(12, 22);
        b.phase = scene_randf(0, 6.28f);
        int s = random(3, 6);
        lv_obj_set_size(b.obj, s, s);
        lv_obj_clear_flag(b.obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }
}

static Food *nearest_food(const Fish &f) {
    Food *best = nullptr;
    float best_d = 1e9f;
    for (int i = 0; i < MAX_FOOD; i++) {
        if (!food[i].alive) continue;
        float dx = food[i].x - f.x, dy = food[i].y - f.y, d = dx * dx + dy * dy;
        if (d < best_d) { best_d = d; best = &food[i]; }
    }
    return best;
}

static void tick(uint32_t dt_ms) {
    float dt = dt_ms / 1000.0f;

    for (int i = 0; i < n_weeds; i++) {
        Weed &wd = weeds[i];
        wd.phase += wd.speed * dt;
        lv_obj_set_x(wd.obj, wd.base_x + (int)(2.0f * sinf(wd.phase)));
    }

    if (!stopping) {
        bubble_acc += dt;
        if (bubble_acc > 0.9f) {
            bubble_acc = 0;
            spawn_bubble(scene_randf(4, area_w - 4), area_h - 2);
        }
    }
    for (int i = 0; i < MAX_BUBBLES; i++) {
        Bubble &b = bubbles[i];
        if (!b.alive) continue;
        b.y -= b.vy * dt;
        b.phase += 3.0f * dt;
        if (b.y < -6) { b.alive = false; lv_obj_add_flag(b.obj, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_set_pos(b.obj, (int)(b.x + 1.5f * sinf(b.phase)), (int)b.y);
    }

    for (int i = 0; i < MAX_FOOD; i++) {
        Food &p = food[i];
        if (!p.alive) continue;
        p.age += dt;
        if (p.y < area_h - 4) p.y += 8.0f * dt;
        if (p.age > FOOD_LIFE_S) { p.alive = false; lv_obj_add_flag(p.obj, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_set_pos(p.obj, (int)p.x, (int)p.y);
    }

    for (int i = 0; i < n_fish; i++) {
        Fish &f = fish[i];
        if (!f.alive) continue;

        Food *meal = stopping ? nullptr : nearest_food(f);
        float tx = meal ? meal->x : f.tx, ty = meal ? meal->y : f.ty;
        float dx = tx - f.x, dy = ty - f.y, d = sqrtf(dx * dx + dy * dy);
        // Hurry for food, and hurry out when the tank is closing so every
        // fish is gone well inside the player's grace period
        float speed = stopping ? f.speed * 2.5f : meal ? f.speed * 1.8f : f.speed;
        if (d > 0.5f) {
            float ease = 1.0f - expf(-1.5f * dt);
            f.vx += (dx / d * speed - f.vx) * ease;
            f.vy += (dy / d * speed * 0.6f - f.vy) * ease;
        }
        f.x += f.vx * dt;
        f.y += f.vy * dt;

        if (meal && d < 4) {
            meal->alive = false;
            lv_obj_add_flag(meal->obj, LV_OBJ_FLAG_HIDDEN);
            spawn_bubble(f.x, f.y);   // a happy little burp
        } else if (!meal && !stopping && d < 6) {
            new_target(f);
        }

        if (stopping && (f.x < -f.bw - f.tw || f.x > area_w + f.bw + f.tw)) {
            f.alive = false;
            lv_obj_add_flag(f.box, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        if (f.vx > 2 && !f.facing_right)  set_facing(f, true);
        if (f.vx < -2 && f.facing_right)  set_facing(f, false);

        f.bob += 1.6f * dt;
        f.wag += 8.0f * dt;
        int tail_h = f.bh - 1 - (int)(1.5f + 1.5f * sinf(f.wag));
        lv_obj_set_height(f.tail, tail_h);
        lv_obj_set_y(f.tail, (f.bh - tail_h) / 2);
        int box_w = f.bw + f.tw - 1;
        lv_obj_set_pos(f.box, (int)f.x - box_w / 2, (int)(f.y + 1.5f * sinf(f.bob)) - f.bh / 2);
    }
}

static void request_stop() {
    stopping = true;
    // Head for whichever side is closer, at the same depth
    for (int i = 0; i < n_fish; i++) {
        Fish &f = fish[i];
        f.tx = f.x < area_w / 2 ? -60 : area_w + 60;
        f.ty = f.y;
    }
}

static bool is_done() {
    if (!stopping) return false;
    for (int i = 0; i < n_fish; i++) if (fish[i].alive) return false;
    return true;
}

static void touch(SceneTouch type, int x, int y) {
    if (type != SCENE_TOUCH_PRESS || stopping) return;
    for (int i = 0; i < MAX_FOOD; i++) {
        Food &p = food[i];
        if (p.alive) continue;
        p.alive = true;
        p.x = x;
        p.y = y;
        p.age = 0;
        lv_obj_set_pos(p.obj, x, y);
        lv_obj_clear_flag(p.obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }
}

static void event(const SceneEvent &e) {
    if (e.type != SCENE_EV_KEY || e.key != SCENE_KEY_ENTER || stopping) return;
    for (int i = 0; i < n_fish; i++) {
        Fish &f = fish[i];
        f.vx += scene_randf(-45, 45);
        f.vy += scene_randf(-15, 15);
        new_target(f);
    }
}

const Scene scene_fish = { "fish", start, tick, request_stop, is_done, touch, nullptr, event };
