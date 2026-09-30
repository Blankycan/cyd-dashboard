#include "registry.h"
#include "pixfb.h"
#include "../layout.h"
#include "../theme.h"
#include <math.h>

// Snow — round flakes in two depth layers: small, dim, slow ones far away and
// larger, brighter, faster ones up close, each swaying gently. Flakes that
// reach the ground settle into a drift that builds up (and slumps into an
// even slope) over the scene. Flakes respawn at the top until asked to stop,
// then the rest settle out and it reports done.
// Touch: a press or drag puffs nearby flakes away from your finger; pressing
// on the drift scoops some of it back up into the air.

static const int   MAX_FLAKES  = 40;
static const float PUFF_RADIUS = 50.0f;
static const float PUFF_POWER  = 70.0f;
static const int   SCOOP_R     = 10;     // px either side of the finger

struct Flake {
    bool  alive;
    bool  near;
    float x, y, vy;
    float push_vx, push_vy;
    float sway_amp, sway_phase, sway_speed;
    int   sx;           // this frame's swayed x
    int   bx, by, bs;   // what was drawn last frame (bs = size, 0 = nothing)
};

static PixFb fb;
static Flake flakes[MAX_FLAKES];
static float drift[SCREEN_W];   // snow depth per column, px
static int   count, area_w, area_h, drift_max;
static bool  stopping, oom;
static float spawn_acc, spawn_every_ms;
static float hurry;   // 1 → 3 once stopping, so the last flakes clear the sky in time

// The backdrop at (x, y): sky, or the drift — a bright crest shading deeper down
static lv_color_t backdrop(int x, int y) {
    int top = area_h - (int)(drift[x] + 0.5f);
    if (y < top) return COL_SCENE_BG;
    return pixfb_mix(COL_SCENE_SNOW_NEAR, COL_SCENE_SNOW_FAR, fminf(1.0f, (y - top) / 7.0f));
}

static void restore(int x, int y, int w, int h) {
    int x0 = LV_MAX(x, 0), y0 = LV_MAX(y, 0);
    int x1 = LV_MIN(x + w, area_w), y1 = LV_MIN(y + h, area_h);
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) fb.buf[yy * area_w + xx] = backdrop(xx, yy);
    pixfb_touch(fb, x0, y0, x1 - x0, y1 - y0);
}

// Repaint columns [x0, x1] of the drift after it changed
static void repaint_drift(int x0, int x1) {
    restore(x0, area_h - drift_max - 2, x1 - x0 + 1, drift_max + 2);
}

// Let a steep bit slump into its neighbours, so the drift stays a soft slope
static void slump(int x0, int x1) {
    x0 = LV_MAX(x0, 0); x1 = LV_MIN(x1, area_w - 1);
    for (int pass = 0; pass < 3; pass++)
        for (int x = x0; x < x1; x++) {
            float d = drift[x] - drift[x + 1];
            if (fabsf(d) > 1.5f) { drift[x] -= d * 0.25f; drift[x + 1] += d * 0.25f; }
        }
}

static void spawn(Flake &f, bool anywhere) {
    f.alive      = true;
    f.near       = random(0, 3) == 0;   // a third in the near layer
    int size     = f.near ? 3 : 2;
    f.x          = scene_randf(0, area_w);
    f.y          = anywhere ? scene_randf(0, area_h * 0.7f) : -(float)size;
    f.vy         = f.near ? scene_randf(14, 22) : scene_randf(6, 11);
    f.push_vx    = f.push_vy = 0;
    f.sway_amp   = f.near ? scene_randf(3, 7) : scene_randf(1, 4);
    f.sway_phase = scene_randf(0, 6.28f);
    f.sway_speed = scene_randf(0.6f, 1.4f);
}

static void start(lv_obj_t *area, int w, int h) {
    area_w = LV_MIN(w, SCREEN_W);
    area_h = h;
    oom = !pixfb_create(fb, area, area_w, h, COL_SCENE_BG);
    count = w * h / 600;
    if (count > MAX_FLAKES) count = MAX_FLAKES;
    if (count < 6) count = 6;
    spawn_every_ms = (h / 12.0f) * 1000.0f / count;
    spawn_acc      = 0;
    stopping       = false;
    hurry          = 1;
    drift_max      = h / 4;
    for (int x = 0; x < area_w; x++) drift[x] = 0;

    for (int i = 0; i < count; i++) { flakes[i].alive = false; flakes[i].bs = 0; }
    // Start with a light flurry already in the air, the rest drift in from the top
    for (int i = 0; i < count / 3; i++) spawn(flakes[i], true);
}

// Near flakes: a soft 3×3 dot (dim corners); far ones a 2×2 speck
static void draw_flake(Flake &f, int x, int y) {
    lv_color_t bg = COL_SCENE_BG;
    if (f.near) {
        lv_color_t c = COL_SCENE_SNOW_NEAR, soft = pixfb_mix(bg, c, 0.35f);
        for (int dy = 0; dy < 3; dy++)
            for (int dx = 0; dx < 3; dx++)
                pixfb_px(fb, x + dx, y + dy, (dx == 1 || dy == 1) ? c : soft);
        pixfb_touch(fb, x, y, 3, 3);
        f.bs = 3;
    } else {
        pixfb_fill(fb, x, y, 2, 2, COL_SCENE_SNOW_FAR);
        f.bs = 2;
    }
    f.bx = x; f.by = y;
}

static void tick(uint32_t dt_ms) {
    if (oom) return;
    float dt = dt_ms / 1000.0f;

    for (int i = 0; i < count; i++) {
        Flake &f = flakes[i];
        if (f.bs) { restore(f.bx, f.by, f.bs, f.bs); f.bs = 0; }
    }

    if (!stopping) {
        spawn_acc += dt_ms;
        if (spawn_acc >= spawn_every_ms) {
            for (int i = 0; i < count; i++) {
                if (!flakes[i].alive) { spawn(flakes[i], false); spawn_acc = 0; break; }
            }
        }
    }

    if (stopping) hurry = fminf(3.0f, hurry + dt * 0.5f);
    float decay = expf(-2.5f * dt);
    for (int i = 0; i < count; i++) {
        Flake &f = flakes[i];
        if (!f.alive) continue;
        f.sway_phase += f.sway_speed * dt;
        f.x += f.push_vx * dt;
        f.y += (f.vy * hurry + f.push_vy) * dt;
        f.push_vx *= decay;
        f.push_vy *= decay;

        int size = f.near ? 3 : 2;
        int sx = f.sx = (int)(f.x + f.sway_amp * sinf(f.sway_phase));
        if (sx < -10 || sx > area_w + 10) {
            if (stopping) f.alive = false; else { spawn(f, false); f.sx = (int)f.x; }
            continue;
        }
        int col = LV_MIN(LV_MAX(sx + size / 2, 0), area_w - 1);
        if (f.y + size >= area_h - drift[col]) {
            // Landed: the drift grows where it fell
            if (drift[col] < drift_max) {
                drift[col] += f.near ? 1.6f : 0.8f;
                slump(col - 4, col + 4);
                repaint_drift(col - 5, col + 5);
            }
            if (stopping) f.alive = false; else spawn(f, false);
            f.sx = (int)f.x;
            continue;
        }
    }
    // Draw after every drift repaint, so none of them covers a flake
    for (int i = 0; i < count; i++)
        if (flakes[i].alive) draw_flake(flakes[i], flakes[i].sx, (int)flakes[i].y);
    pixfb_flush(fb);
}

static void request_stop() { stopping = true; }

static bool is_done() {
    if (oom) return true;
    if (!stopping) return false;
    for (int i = 0; i < count; i++) if (flakes[i].alive) return false;
    return true;
}

// Scoop the drift under the finger and throw it up as loose flakes
static void scoop(int x) {
    float taken = 0;
    for (int c = LV_MAX(x - SCOOP_R, 0); c <= LV_MIN(x + SCOOP_R, area_w - 1); c++) {
        float t = fminf(drift[c], 4.0f * (1.0f - fabsf((float)(c - x)) / (SCOOP_R + 1)));
        drift[c] -= t;
        taken += t;
    }
    repaint_drift(x - SCOOP_R - 1, x + SCOOP_R + 1);
    // Spare flakes first, then borrow ones still high in the sky
    int thrown = 0;
    for (int pass = 0; pass < 2; pass++)
    for (int i = 0; i < count && thrown < taken / 6; i++) {
        Flake &f = flakes[i];
        if (pass == 0 ? f.alive : (!f.alive || f.y > area_h * 0.4f)) continue;
        spawn(f, false);
        f.x = x + scene_randf(-SCOOP_R, SCOOP_R);
        f.y = area_h - drift_max - 3;
        f.push_vx = scene_randf(-25, 25);
        f.push_vy = -scene_randf(30, 60);
        thrown++;
    }
}

static void touch(SceneTouch type, int x, int y) {
    if (type == SCENE_TOUCH_RELEASE) return;
    if (type == SCENE_TOUCH_PRESS && x >= 0 && x < area_w && y >= area_h - drift[x] - 8) scoop(x);
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

static void finish() { pixfb_free(fb); }

const Scene scene_snow = { "snow", start, tick, request_stop, is_done, touch, finish };
