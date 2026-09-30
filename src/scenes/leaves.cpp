#include "registry.h"
#include "pixfb.h"
#include "../layout.h"
#include "../theme.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Falling leaves — leaf shapes (a pointed oval with a darker midrib and a
// stem) that drift down, sway side to side, and tumble: they narrow edge-on
// and flip over, showing their darker underside. Some settle into a pile
// along the bottom that builds up over the scene. They respawn at the top
// until asked to stop, then the rest fall out and the scene reports done.
// Touch: a press or drag blows nearby leaves away from your finger.
// Events: a gust sweeps across on Enter and whenever the track changes,
// lifting a few leaves off the pile with it.

static const int   MAX_LEAVES   = 24;
static const float GUST_RADIUS  = 60.0f;
static const float GUST_POWER   = 90.0f;   // px/s given to a leaf right at the finger
static const int   PILE_H       = 12;      // rows at the bottom the pile can fill
static const int   SETTLE_PCT   = 30;      // chance a leaf reaching the ground stays there

struct Leaf {
    bool  alive;
    float x, y;          // centre, px
    float vy;            // fall speed, px/s
    float push_vx, push_vy;   // gust velocity, decays over time
    float sway_amp, sway_phase, sway_speed;
    float tumble_phase, tumble_speed;
    int   w, h;
    uint8_t colour;      // index into leaf_colours()
    bool  settles;       // decided at spawn: lands on the pile or falls past it
    int   bx, by, bw, bh;   // what was drawn last frame, to erase
};

static PixFb      fb;
static Leaf       leaves[MAX_LEAVES];
static lv_color_t pile[PILE_H * SCREEN_W];   // the pile strip, drawn over the backdrop
static uint8_t    pile_top[SCREEN_W];        // pile height per column, px
static int        count, area_w, area_h;
static bool       stopping, oom;
static float      spawn_acc, spawn_every_ms;
static float      wind, wind_target, wind_timer;

static lv_color_t leaf_colour(uint8_t i) {
    switch (i) {
        case 0:  return COL_SCENE_LEAVES_1;
        case 1:  return COL_SCENE_LEAVES_2;
        default: return COL_SCENE_LEAVES_3;
    }
}

static void restore(int x, int y, int w, int h) {
    int x0 = LV_MAX(x, 0), y0 = LV_MAX(y, 0);
    int x1 = LV_MIN(x + w, area_w), y1 = LV_MIN(y + h, area_h);
    if (x0 >= x1 || y0 >= y1) return;
    int strip = area_h - PILE_H;
    for (int yy = y0; yy < y1; yy++) {
        lv_color_t *row = fb.buf + yy * area_w;
        if (yy < strip) for (int xx = x0; xx < x1; xx++) row[xx] = COL_SCENE_BG;
        else            memcpy(row + x0, pile + (yy - strip) * area_w + x0, sizeof(lv_color_t) * (x1 - x0));
    }
    pixfb_touch(fb, x0, y0, x1 - x0, y1 - y0);
}

// A leaf `w` wide (edge-on when small) centred on cx, cy: a pointed oval with
// a darker midrib and a stem on the `stem` side (-1 left, +1 right). Plots
// through `put` so the same shape draws onto the canvas or into the pile.
template <typename Put>
static void leaf_shape(int cx, int cy, int w, int h, lv_color_t c, int stem, Put put) {
    lv_color_t rib = pixfb_mix(c, COL_SCENE_BG, 0.35f);
    int x0 = cx - w / 2, mid = h / 2;
    for (int dy = 0; dy < h; dy++) {
        // Narrower toward the top and bottom rows, so the ends come to a point
        float t = (dy - (h - 1) / 2.0f) / (h / 2.0f);
        int inset = (int)(w * 0.5f * t * t + 0.5f);
        for (int dx = inset; dx < w - inset; dx++)
            put(x0 + dx, cy - mid + dy, (dy == mid && w >= 4 && dx > 0 && dx < w - 1) ? rib : c);
    }
    put(stem > 0 ? x0 + w : x0 - 1, cy, rib);
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
    l.colour       = random(0, 3);
    l.settles      = random(0, 100) < SETTLE_PCT;
}

// Lay a leaf flat on the pile under column `cx` (if there's room left)
static void settle(const Leaf &l, int cx) {
    int x0 = LV_MAX(cx - l.w / 2, 0), x1 = LV_MIN(cx - l.w / 2 + l.w, area_w);
    if (x0 >= x1) return;
    int base = 0;
    for (int x = x0; x < x1; x++) base = LV_MAX(base, pile_top[x]);
    if (base + l.h > PILE_H) return;   // full here: it just blows away
    // Bottom row rests on the pile, sunk one row into it so the pile packs down
    int bottom = PILE_H - 1 - base + (base ? 1 : 0);
    int cy     = bottom - (l.h - 1) + l.h / 2;
    lv_color_t c = pixfb_mix(leaf_colour(l.colour), COL_SCENE_BG, scene_randf(0.1f, 0.35f));
    int stem = random(0, 2) ? 1 : -1;
    leaf_shape(cx, cy, l.w, l.h, c, stem, [](int x, int y, lv_color_t col) {
        if (x >= 0 && x < area_w && y >= 0 && y < PILE_H) pile[y * area_w + x] = col;
    });
    for (int x = x0; x < x1; x++) pile_top[x] = LV_MAX(pile_top[x], PILE_H - (cy - l.h / 2));
    restore(x0 - 2, area_h - PILE_H, x1 - x0 + 4, PILE_H);
}

static void start(lv_obj_t *area, int w, int h) {
    area_w = LV_MIN(w, SCREEN_W);
    area_h = h;
    oom = !pixfb_create(fb, area, area_w, h, COL_SCENE_BG);
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
    for (int i = 0; i < PILE_H * SCREEN_W; i++) pile[i] = COL_SCENE_BG;
    memset(pile_top, 0, sizeof(pile_top));
    for (int i = 0; i < count; i++) { leaves[i].alive = false; leaves[i].bw = 0; }
}

static void tick(uint32_t dt_ms) {
    if (oom) return;
    float dt = dt_ms / 1000.0f;

    for (int i = 0; i < count; i++) {
        Leaf &l = leaves[i];
        if (l.bw) { restore(l.bx, l.by, l.bw, l.bh); l.bw = 0; }
    }

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

    // Move everything (landings repaint the pile) before drawing anything
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

        int sx = (int)(l.x + l.sway_amp * sinf(l.sway_phase));
        bool gone = l.y > area_h + 4 || sx < -20 || sx > area_w + 20;
        if (!gone && l.settles && l.push_vy > -5 && sx >= 0 && sx < area_w &&
            l.y + l.h / 2.0f >= area_h - pile_top[sx]) {
            settle(l, sx);
            gone = true;
        }
        if (gone) { if (stopping) l.alive = false; else spawn(l); }
    }

    for (int i = 0; i < count; i++) {
        Leaf &l = leaves[i];
        if (!l.alive) continue;
        int   sx = (int)(l.x + l.sway_amp * sinf(l.sway_phase));
        float c  = cosf(l.tumble_phase);
        int   tw = (int)(l.w * fabsf(c));
        if (tw < 2) tw = 2;
        // Tumbling over shows the underside: a shade darker, stem on the other side
        lv_color_t col = leaf_colour(l.colour);
        if (c < 0) col = pixfb_mix(col, COL_SCENE_BG, 0.3f);
        int cy = (int)l.y;
        leaf_shape(sx, cy, tw, l.h, col, c < 0 ? -1 : 1, [](int x, int y, lv_color_t cc) { pixfb_px(fb, x, y, cc); });
        l.bx = sx - tw / 2 - 1; l.by = cy - l.h / 2; l.bw = tw + 2; l.bh = l.h;
        pixfb_touch(fb, l.bx, l.by, l.bw, l.bh);
    }
    pixfb_flush(fb);
}

static void request_stop() { stopping = true; }

static bool is_done() {
    if (oom) return true;
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

static void gust() {
    float dir = random(0, 2) ? 1.0f : -1.0f;
    for (int i = 0; i < count; i++) {
        if (!leaves[i].alive) continue;
        leaves[i].push_vx += dir * scene_randf(40, 80);
        leaves[i].push_vy -= scene_randf(5, 15);
    }
    if (stopping) return;
    // Whip a few leaves up off the pile: they leave a dip behind
    for (int n = 0, i = 0; n < 3 && i < count; i++) {
        Leaf &l = leaves[i];
        // Reuse a spare leaf or one near the top — the gust is moving everything anyway
        if (l.alive && l.y > area_h * 0.3f) continue;
        int x = random(4, LV_MAX(area_w - 4, 5));
        if (pile_top[x] < 3) continue;
        spawn(l);
        l.x = x;
        l.y = area_h - pile_top[x];
        l.push_vx = dir * scene_randf(50, 90);
        l.push_vy = -scene_randf(25, 45);
        l.settles = false;
        l.colour  = random(0, 3);
        // Clear the top leaf's worth of the pile there
        int x0 = LV_MAX(x - l.w / 2, 0), x1 = LV_MIN(x + l.w / 2 + 1, area_w);
        for (int c = x0; c < x1; c++) {
            int top = pile_top[c], lower = LV_MAX(top - l.h, 0);
            for (int r = PILE_H - top; r < PILE_H - lower; r++) pile[r * area_w + c] = COL_SCENE_BG;
            pile_top[c] = lower;
        }
        restore(x0 - 1, area_h - PILE_H, x1 - x0 + 2, PILE_H);
        n++;
    }
}

static void event(const SceneEvent &e) {
    if (e.type == SCENE_EV_TRACK || (e.type == SCENE_EV_KEY && e.key == SCENE_KEY_ENTER)) gust();
}

static void finish() { pixfb_free(fb); }

const Scene scene_leaves = { "leaves", start, tick, request_stop, is_done, touch, finish, event };
