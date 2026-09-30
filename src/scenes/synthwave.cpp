#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Synthwave sunset drive — a striped sun sinking behind two mountain ranges,
// a perspective grid rushing toward you, and a car on the road seen from
// behind. The sky, sun, mountains and static grid are painted once as the
// backdrop; each frame only redraws the ground's moving lines and the car.
// When asked to stop, the sun sets below the horizon while the car drives off
// toward the vanishing point, then it's done.
// Touch: the car steers toward your finger; pressing also gives a burst of speed.
// Events: with music playing the grid scrolls one line per beat (when the tempo
// is known) and flashes on every beat; typing nudges the speed up.

static const int   MAX_PEAKS  = 24;
static const float BASE_SPEED = 1.1f;    // grid lines passing per second

struct Peak { int x, w, h; };

static PixFb fb;
static int   W, H, hy, cx;            // area size, horizon row, centre column
static int   sun_r, sun_cy;
static float sun_drop;                // px the sun has sunk while setting
static Peak  far_peaks[MAX_PEAKS], near_peaks[MAX_PEAKS];
static int   n_far, n_near;
static float phase;                   // 0..1, how far the nearest grid line has come
static float boost, pulse, typing_boost;
static float car_x, target_x, sway;
static float leave;                   // 0 → 1 as the car drives off at the end
static bool  stopping, done, oom;
static int   road_half;               // road half-width at the bottom row

// ---------------------------------------------------------------------------
// Backdrop

static void make_peaks(Peak *p, int &n, int min_h, int max_h) {
    n = 0;
    for (int x = -20; x < W + 20 && n < MAX_PEAKS; n++) {
        p[n].w = random(18, 42);
        p[n].h = random(min_h, max_h + 1);
        p[n].x = x;
        x += p[n].w * 2 / 3;
    }
}

static void draw_peaks(const Peak *p, int n, lv_color_t col, int base) {
    for (int i = 0; i < n; i++)
        pixfb_fill_tri(fb, p[i].x, base, p[i].x + p[i].w / 2, base - p[i].h, p[i].x + p[i].w, base, col);
}

static void draw_sky_and_sun() {
    for (int y = 0; y < hy; y++)
        pixfb_fill(fb, 0, y, W, 1, pixfb_mix(COL_SCENE_SYNTH_SKY_TOP, COL_SCENE_SYNTH_SKY_LOW, (float)y / hy));

    // Stars in the upper sky (seeded, so they don't jump when this is redrawn)
    randomSeed(W * 7 + H);
    for (int i = 0; i < W / 8; i++)
        pixfb_fill(fb, random(0, W), random(0, hy * 2 / 3), 1, 1, COL_SCENE_SYNTH_STAR);
    randomSeed(esp_random());

    // Sun: gradient disc with the classic gaps in its lower half, widening downward
    int cy = sun_cy + (int)sun_drop;
    for (int dy = -sun_r; dy <= sun_r; dy++) {
        int y = cy + dy;
        if (y < 0 || y >= hy) continue;
        if (dy > -sun_r / 5) {
            int d = dy + sun_r / 5;
            int gap = 1 + d * 3 / sun_r;
            if (d % 6 < gap) continue;
        }
        int dx = (int)sqrtf((float)(sun_r * sun_r - dy * dy));
        float t = (float)(dy + sun_r) / (2 * sun_r);
        pixfb_fill(fb, cx - dx, y, 2 * dx + 1, 1, pixfb_mix(COL_SCENE_SYNTH_SUN_TOP, COL_SCENE_SYNTH_SUN_LOW, t));
    }

    draw_peaks(far_peaks, n_far, COL_SCENE_SYNTH_MOUNTAIN_FAR, hy);
    draw_peaks(near_peaks, n_near, COL_SCENE_SYNTH_MOUNTAIN, hy);
}

static int road_half_at(int y) { return (int)((float)(y - hy) / (H - hy) * road_half); }

static void draw_ground_base() {
    for (int y = hy; y < H; y++) {
        float t = (float)(y - hy) / (H - hy);
        pixfb_fill(fb, 0, y, W, 1, pixfb_mix(COL_SCENE_SYNTH_GROUND_TOP, COL_SCENE_SYNTH_GROUND_LOW, t));
        int rh = road_half_at(y);
        pixfb_fill(fb, cx - rh, y, 2 * rh + 1, 1, COL_SCENE_SYNTH_ROAD);
    }
    // Grid lines running to the vanishing point, and the road edges
    for (int i = -12; i <= 12; i++) {
        int xb = cx + i * 34;
        if (abs(xb - cx) <= road_half) continue;
        pixfb_line(fb, cx, hy, xb, H - 1, COL_SCENE_SYNTH_GRID);
    }
    pixfb_line(fb, cx, hy, cx - road_half, H - 1, COL_SCENE_SYNTH_GRID);
    pixfb_line(fb, cx, hy, cx + road_half, H - 1, COL_SCENE_SYNTH_GRID);
    pixfb_fill(fb, 0, hy, W, 1, COL_SCENE_SYNTH_GRID);   // horizon
}

static void paint_backdrop() {
    draw_sky_and_sun();
    draw_ground_base();
    pixfb_bg_save(fb);
}

// ---------------------------------------------------------------------------

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_SYNTH_SKY_TOP);
    done = oom;
    if (oom) return;
    hy = h * 56 / 100;
    cx = w / 2;
    sun_r  = h * 30 / 100;
    sun_cy = hy - sun_r / 3;
    sun_drop = 0;
    road_half = w / 5;
    make_peaks(far_peaks, n_far, h / 12, h / 5);
    make_peaks(near_peaks, n_near, h / 20, h / 8);
    paint_backdrop();
    if (!fb.bg) { done = oom = true; return; }

    phase = 0;
    boost = pulse = typing_boost = 0;
    car_x = target_x = cx;
    sway = 0;
    leave = 0;
    stopping = false;
}

static void draw_car(float x, int bottom, float s) {
    int cw = (int)(34 * s), ch = (int)(13 * s);
    if (cw < 3) cw = 3;
    if (ch < 2) ch = 2;
    int x0 = (int)x - cw / 2, y0 = bottom - ch;
    int roof = ch * 45 / 100;
    // Roof and cabin as a trapezoid, then the body
    pixfb_fill_tri(fb, x0 + cw / 5, y0 + roof, x0 + cw * 3 / 10, y0, x0 + cw * 7 / 10, y0, COL_SCENE_SYNTH_CAR);
    pixfb_fill_tri(fb, x0 + cw / 5, y0 + roof, x0 + cw * 7 / 10, y0, x0 + cw * 4 / 5, y0 + roof, COL_SCENE_SYNTH_CAR);
    pixfb_fill(fb, x0, y0 + roof, cw, ch - roof - 1, COL_SCENE_SYNTH_CAR);
    // Rear window and the glowing tail-light strip
    if (s > 0.4f) pixfb_fill(fb, x0 + cw * 3 / 10 + 1, y0 + 1, cw * 2 / 5 - 2, roof - 2, COL_SCENE_SYNTH_SKY_LOW);
    lv_color_t tail = pixfb_mix(COL_SCENE_SYNTH_TAILLIGHT, COL_SCENE_SYNTH_GRID_BEAT, pulse * 0.6f);
    pixfb_fill(fb, x0 + 1, y0 + roof + 1, cw - 2, ch > 6 ? 2 : 1, tail);
    // Wheels
    int wh = ch > 8 ? 2 : 1;
    pixfb_fill(fb, x0 + 1, bottom - wh, cw / 5, wh, COL_SCENE_SYNTH_ROAD);
    pixfb_fill(fb, x0 + cw - 1 - cw / 5, bottom - wh, cw / 5, wh, COL_SCENE_SYNTH_ROAD);
}

static void frame(uint32_t dt_ms) {
    if (oom || done) return;
    float dt = dt_ms / 1000.0f;
    const SceneContext &ctx = scene_ctx();

    // Speed: one grid line per beat when the tempo is known, plus boosts
    float speed = BASE_SPEED;
    if (ctx.music == SCENE_MUSIC_PLAYING && ctx.bpm) {
        speed = ctx.bpm / 60.0f;
        while (speed > 2.6f) speed /= 2;
        while (speed < 0.9f) speed *= 2;
    }
    speed *= 1.0f + boost + typing_boost;
    boost        *= expf(-1.2f * dt);
    typing_boost *= expf(-0.8f * dt);
    pulse        *= expf(-5.0f * dt);
    phase += speed * dt;
    while (phase >= 1) phase -= 1;

    if (stopping) {
        sun_drop += 14.0f * dt;
        leave += 0.45f * dt;
        if (leave > 1) leave = 1;
        // The sky band has to be repainted while the sun moves
        draw_sky_and_sun();
        pixfb_fill(fb, 0, hy, W, 1, COL_SCENE_SYNTH_GRID);
        if (sun_cy + sun_drop - sun_r > hy && leave >= 1) done = true;
    }

    // Ground: restore the static part, then the lines that move
    pixfb_bg_restore(fb, 0, hy + 1, W, H - hy - 1);
    lv_color_t grid = pixfb_mix(COL_SCENE_SYNTH_GRID, COL_SCENE_SYNTH_GRID_BEAT, pulse);
    for (int k = 0; k < 40; k++) {
        float z = 1.0f + k + (1.0f - phase);   // depth of this line, 1 = nearest
        int y = hy + (int)((H - hy) / z);
        if (y <= hy + 1) break;
        int rh = road_half_at(y);
        pixfb_fill(fb, 0, y, cx - rh, 1, grid);
        pixfb_fill(fb, cx + rh + 1, y, W - cx - rh - 1, 1, grid);
        // Centre dashes on every other line
        if (k % 2 == 0) {
            float z2 = z + 0.45f;
            int y2 = hy + (int)((H - hy) / z2);
            pixfb_fill(fb, cx, y2, 1, y - y2 + 1, COL_SCENE_SYNTH_LANE);
        }
    }

    // Car: eases toward the steering target, sways a little, drives off at the end
    float ease = 1.0f - expf(-3.0f * dt);
    car_x += (target_x - car_x) * ease;
    sway += dt * 2.3f;
    float s = 1.0f - leave * 0.95f;
    int bottom = H - 3 - (int)(leave * (H - 3 - hy - 1));
    float x = cx + (car_x - cx) * s + sinf(sway) * 1.5f * s;
    draw_car(x, bottom, s);
}

static void tick(uint32_t dt_ms) {
    if (oom) return;
    frame(dt_ms);
    pixfb_flush(fb);   // hand this frame's changes to LVGL
}

static void request_stop() { stopping = true; target_x = cx; }
static bool is_done()      { return done; }

static void touch(SceneTouch type, int x, int) {
    if (stopping || type == SCENE_TOUCH_RELEASE) return;
    float lim = road_half * 0.7f;
    target_x = x < cx - lim ? cx - lim : x > cx + lim ? cx + lim : x;
    if (type == SCENE_TOUCH_PRESS) boost += 1.5f;
}

static void event(const SceneEvent &e) {
    if (e.type == SCENE_EV_BEAT) pulse = fminf(1.0f, pulse + 0.4f + e.strength / 150.0f);
    if (e.type == SCENE_EV_KEY)  typing_boost = fminf(1.5f, typing_boost + 0.08f);
}

static void finish() { pixfb_free(fb); }

const Scene scene_synthwave = { "synthwave", start, tick, request_stop, is_done, touch, finish, event };
SCENE_REGISTER(scene_synthwave, "Synthwave drive");
