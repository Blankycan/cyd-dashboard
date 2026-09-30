#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Campfire night — a fire of rising flame particles that cool from white-hot
// through orange to red, sparks drifting up into a starry sky over a ring of
// pine silhouettes, and a warm glow on the ground that flickers with the
// flames. Sky, trees, ground and logs are the painted backdrop. When asked to
// stop, the fire dies down to embers and it's done.
// Touch: tap to toss on a log; the fire flares up for a few seconds.
// Events: with music playing the flames surge with the bass and beats throw
// sparks; Enter puffs out a burst of sparks.

static const int MAX_FLAMES = 90;
static const int MAX_SPARKS = 30;

struct Flame { bool alive; float x, y, vx, vy, life, max_life, r; };
struct Spark { bool alive; float x, y, vx, vy, life, max_life; int px, py; };

static PixFb fb;
static int   W, H, fx, base_y, ground_y;
static Flame flames[MAX_FLAMES];
static Spark sparks[MAX_SPARKS];
static float flare, strength, spawn_acc, flicker, beat_kick;
static bool  stopping, done, oom;
static int   fire_x0, fire_y0, fire_w, fire_h;   // box the flames live in, restored each frame
static int   glow_w, glow_h;

static void paint_backdrop() {
    for (int y = 0; y < ground_y; y++)
        pixfb_fill(fb, 0, y, W, 1, pixfb_mix(COL_SCENE_FIRE_SKY_TOP, COL_SCENE_FIRE_SKY_LOW, (float)y / ground_y));
    for (int i = 0; i < W / 7; i++)
        pixfb_fill(fb, random(0, W), random(0, ground_y * 2 / 3), 1, 1, COL_SCENE_FIRE_STAR);
    // Pines along the horizon, taller at the edges
    for (int x = -6; x < W + 6; x += random(7, 13)) {
        float edge = fabsf(x - W / 2.0f) / (W / 2.0f);
        int h = (int)(H * (0.15f + 0.3f * edge) * scene_randf(0.7f, 1.1f)), hw = h / 4 + 2;
        pixfb_fill_tri(fb, x - hw, ground_y, x, ground_y - h, x + hw, ground_y, COL_SCENE_FIRE_TREES);
    }
    pixfb_fill(fb, 0, ground_y, W, H - ground_y, COL_SCENE_FIRE_GROUND);
    pixfb_bg_save(fb);
}

static void draw_logs() {
    pixfb_fill_tri(fb, fx - 14, base_y + 2, fx - 12, base_y + 5, fx + 12, base_y - 1, COL_SCENE_FIRE_LOGS);
    pixfb_fill_tri(fb, fx - 14, base_y + 2, fx + 12, base_y - 1, fx + 11, base_y - 4, COL_SCENE_FIRE_LOGS);
    pixfb_fill_tri(fb, fx + 14, base_y + 2, fx + 12, base_y + 5, fx - 12, base_y - 1, COL_SCENE_FIRE_LOGS);
    pixfb_fill_tri(fb, fx + 14, base_y + 2, fx - 12, base_y - 1, fx - 11, base_y - 4, COL_SCENE_FIRE_LOGS);
    for (int i = -3; i <= 3; i++)   // stones around the pit
        pixfb_fill_circle(fb, fx + i * 6, base_y + 5 - (abs(i) == 3 ? 1 : 0), 2, COL_SCENE_FIRE_STONES);
}

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_FIRE_SKY_TOP);
    done = oom;
    if (oom) return;
    ground_y = h * 74 / 100;
    fx = w / 2;
    base_y = h - 9;
    paint_backdrop();
    if (!fb.bg) { done = oom = true; return; }
    fire_w = 48; fire_h = h * 72 / 100;
    fire_x0 = fx - fire_w / 2; fire_y0 = base_y + 6 - fire_h;
    glow_w = w * 36 / 100; glow_h = 10;
    for (int i = 0; i < MAX_FLAMES; i++) flames[i].alive = false;
    for (int i = 0; i < MAX_SPARKS; i++) { sparks[i].alive = false; sparks[i].px = -1; }
    flare = flicker = beat_kick = spawn_acc = 0;
    strength = 1.0f;
    stopping = false;
}

static void add_spark(float vy_min, float vy_max) {
    for (int i = 0; i < MAX_SPARKS; i++) {
        Spark &s = sparks[i];
        if (s.alive) continue;
        int px = s.px, py = s.py;
        float life = scene_randf(1.2f, 2.4f);
        s = { true, fx + scene_randf(-6, 6), (float)base_y - 6, scene_randf(-8, 8), -scene_randf(vy_min, vy_max),
              life, life, px, py };
        return;
    }
}

static lv_color_t flame_color(float t) {   // t: 0 fresh → 1 dying
    if (t < 0.25f) return pixfb_mix(COL_SCENE_FIRE_CORE, COL_SCENE_FIRE_MID, t / 0.25f);
    if (t < 0.6f)  return pixfb_mix(COL_SCENE_FIRE_MID, COL_SCENE_FIRE_OUTER, (t - 0.25f) / 0.35f);
    return pixfb_mix(COL_SCENE_FIRE_OUTER, COL_SCENE_FIRE_SKY_LOW, (t - 0.6f) / 0.4f);
}

static void frame(uint32_t dt_ms) {
    if (oom || done) return;
    float dt = dt_ms / 1000.0f;
    const SceneContext &ctx = scene_ctx();

    // How big the fire is: dies down when stopping, flares from logs, surges with the bass
    if (stopping) strength -= dt * 0.45f;
    if (strength < 0) strength = 0;
    flare *= expf(-0.5f * dt);
    beat_kick *= expf(-6.0f * dt);
    float bass = ctx.music == SCENE_MUSIC_PLAYING ? ctx.bass / 100.0f : 0;
    float size = strength * (1.0f + flare + 0.5f * bass + 0.3f * beat_kick);
    flicker += (scene_randf(-1, 1) - flicker) * 0.3f;

    // Erase: the fire box, the glow band, and last frame's sparks
    pixfb_bg_restore(fb, fire_x0, fire_y0, fire_w, fire_h);
    pixfb_bg_restore(fb, fx - glow_w / 2, base_y - glow_h / 2, glow_w, glow_h);
    for (int i = 0; i < MAX_SPARKS; i++)
        if (sparks[i].px >= 0) { pixfb_bg_restore(fb, sparks[i].px, sparks[i].py, 1, 1); sparks[i].px = -1; }

    // Glow on the ground: concentric ellipses, brighter with a bigger fire
    float g = fminf(1.0f, size * (0.55f + 0.1f * flicker));
    for (int k = 4; k >= 1; k--) {
        int ew = glow_w * k / 4 / 2, eh = glow_h * k / 4 / 2;
        lv_color_t c = pixfb_mix(COL_SCENE_FIRE_GROUND, COL_SCENE_FIRE_GLOW, g * (1.0f - k / 5.0f));
        for (int dy = -eh; dy <= eh; dy++) {
            int dx = eh ? (int)(ew * sqrtf(1.0f - (float)(dy * dy) / (eh * eh))) : ew;
            int y = base_y + dy;
            if (y < ground_y) continue;
            pixfb_fill(fb, fx - dx, y, 2 * dx + 1, 1, c);
        }
    }
    draw_logs();

    // Flames
    spawn_acc += dt * 70 * fminf(size, 2.5f);
    while (spawn_acc >= 1) {
        spawn_acc -= 1;
        for (int i = 0; i < MAX_FLAMES; i++) {
            Flame &f = flames[i];
            if (f.alive) continue;
            float life = scene_randf(0.45f, 0.9f) * (0.8f + 0.4f * fminf(size, 1.5f));
            f = { true, fx + scene_randf(-9, 9) * fminf(1.0f, 0.4f + size * 0.6f), (float)base_y - 2,
                  scene_randf(-4, 4), -scene_randf(22, 34) * (0.7f + 0.3f * size), life, life, scene_randf(1.5f, 3.2f) };
            break;
        }
    }
    bool any = false;
    for (int i = 0; i < MAX_FLAMES; i++) {
        Flame &f = flames[i];
        if (!f.alive) continue;
        f.life -= dt;
        if (f.life <= 0) { f.alive = false; continue; }
        any = true;
        float t = 1.0f - f.life / f.max_life;
        f.vx += (fx - f.x) * 1.8f * dt + scene_randf(-12, 12) * dt;   // drawn toward the centre, wavering
        f.x += f.vx * dt; f.y += f.vy * dt;
        // Keep whole circles inside the box that gets restored each frame
        if (f.y < fire_y0 + 4 || f.x < fire_x0 + 4 || f.x > fire_x0 + fire_w - 5) continue;
        pixfb_fill_circle(fb, (int)f.x, (int)f.y, (int)(f.r * (1.0f - t * 0.7f)), flame_color(t));
    }

    // Sparks: a few on their own, more with flares and beats
    if (!stopping && random(0, 1000) < (int)(dt_ms * (1 + 4 * flare + 3 * beat_kick))) add_spark(20, 40);
    for (int i = 0; i < MAX_SPARKS; i++) {
        Spark &s = sparks[i];
        if (!s.alive) continue;
        s.life -= dt;
        if (s.life <= 0) { s.alive = false; continue; }
        any = true;
        s.vx += scene_randf(-20, 20) * dt;
        s.vy *= expf(-0.4f * dt);
        s.x += s.vx * dt; s.y += s.vy * dt;
        int x = (int)s.x, y = (int)s.y;
        if (x < 0 || x >= W || y < 0) { s.alive = false; continue; }
        float fade = 1.0f - s.life / s.max_life;
        pixfb_fill(fb, x, y, 1, 1, pixfb_mix(COL_SCENE_FIRE_SPARK, COL_SCENE_FIRE_SKY_TOP, fade));
        s.px = x; s.py = y;
    }

    if (stopping && strength <= 0 && !any) done = true;
}

static void tick(uint32_t dt_ms) {
    if (oom) return;
    frame(dt_ms);
    pixfb_flush(fb);   // hand this frame's changes to LVGL
}

static void request_stop() { stopping = true; }
static bool is_done()      { return done; }

static void touch(SceneTouch type, int, int) {
    if (type != SCENE_TOUCH_PRESS || stopping) return;
    flare = fminf(2.0f, flare + 1.2f);
    for (int i = 0; i < 8; i++) add_spark(30, 55);
}

static void event(const SceneEvent &e) {
    if (stopping) return;
    if (e.type == SCENE_EV_BEAT) {
        beat_kick = fminf(1.0f, beat_kick + e.strength / 100.0f);
        if (e.strength > 50) add_spark(25, 45);
    }
    if (e.type == SCENE_EV_KEY && e.key == SCENE_KEY_ENTER)
        for (int i = 0; i < 5; i++) add_spark(30, 50);
}

static void finish() { pixfb_free(fb); }

const Scene scene_campfire = { "campfire", start, tick, request_stop, is_done, touch, finish, event };
