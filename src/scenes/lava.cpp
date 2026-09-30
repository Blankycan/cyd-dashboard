#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Lava lamp — glowing blobs that warm at the bottom, rise, cool at the top and
// sink again, merging and pulling apart as they pass (metaballs: each blob
// adds to a field, and wherever the total crosses a threshold is wax). Drawn
// on a coarse grid of 2x2 blocks to keep it light. When asked to stop, the
// lamp cools: the blobs sink and shrink away, then it's done.
// Touch: tap to heat the wax under your finger into a new blob; drag to push
// the blobs around.
// Events: with music playing the blobs swell with the bass and each beat
// gives them a little kick of heat.

static const int CELL = 2;
static const int MAX_BLOBS = 10;

// Each blob cycles: warming on the bottom, rising, resting at the top, sinking
enum BlobPhase { BLOB_WARMING, BLOB_RISING, BLOB_AT_TOP, BLOB_SINKING };
struct Blob { bool alive; float x, y, vy, r, speed, timer, drift; uint8_t phase; };

static PixFb fb;
static int   W, H, gw, gh;
static Blob  blobs[MAX_BLOBS];
static float pulse, cool;           // bass swell; 0 → 1 as the lamp cools at the end
static bool  stopping, done, oom;
static float push_x, push_y;
static bool  pushing;

static void add_blob(float x, float y, float r) {
    for (int i = 0; i < MAX_BLOBS; i++)
        if (!blobs[i].alive) {
            blobs[i] = { true, x, y, 0, r, scene_randf(9, 17), scene_randf(0, 3), scene_randf(-4, 4),
                         (uint8_t)random(0, 4) };
            return;
        }
}

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_LAVA_BG_TOP);
    done = oom;
    if (oom) return;
    gw = w / CELL; gh = h / CELL;
    for (int i = 0; i < MAX_BLOBS; i++) blobs[i].alive = false;
    int n = 5 + w * h / 9000;
    if (n > MAX_BLOBS - 2) n = MAX_BLOBS - 2;
    for (int i = 0; i < n; i++) add_blob(scene_randf(w * 0.1f, w * 0.9f), scene_randf(h * 0.2f, h * 0.9f), scene_randf(h * 0.08f, h * 0.13f));
    pulse = cool = 0;
    pushing = stopping = false;
}

static void tick_frame(uint32_t dt_ms) {
    float dt = dt_ms / 1000.0f;
    const SceneContext &ctx = scene_ctx();
    float bass = ctx.music == SCENE_MUSIC_PLAYING ? ctx.bass / 100.0f : 0;
    pulse += (bass * 0.25f - pulse) * fminf(1, dt * 6);
    if (stopping) cool = fminf(1, cool + dt * 0.45f);

    // Heat rises: warm blobs float up, cool ones sink; the bottom warms them,
    // the top cools them
    bool any = false;
    for (int i = 0; i < MAX_BLOBS; i++) {
        Blob &b = blobs[i];
        if (!b.alive) continue;
        any = true;
        float bottom = H - b.r * 0.7f, top = b.r * 0.7f, target = 0;
        switch (b.phase) {
            case BLOB_WARMING: if ((b.timer -= dt) <= 0 && !stopping) b.phase = BLOB_RISING; break;
            case BLOB_RISING:  target = -b.speed; if (b.y <= top)    { b.phase = BLOB_AT_TOP;  b.timer = scene_randf(1, 3); } break;
            case BLOB_AT_TOP:  if ((b.timer -= dt) <= 0 || stopping) b.phase = BLOB_SINKING; break;
            case BLOB_SINKING: target = b.speed * (stopping ? 2.5f : 0.8f);   // cooling down: sink faster
                          if (b.y >= bottom) { b.phase = BLOB_WARMING; b.timer = scene_randf(1.5f, 4.5f); } break;
        }
        if (stopping && b.phase == BLOB_RISING) b.phase = BLOB_SINKING;
        b.vy += (target - b.vy) * fminf(1, 1.2f * dt);   // wax is slow to get going and to stop
        b.y += b.vy * dt;
        b.x += b.drift * dt;
        if (b.x < b.r) b.drift = fabsf(b.drift); else if (b.x > W - b.r) b.drift = -fabsf(b.drift);
        if (b.y > H - b.r * 0.7f) { b.y = H - b.r * 0.7f; b.vy = 0; }
        if (b.y < b.r * 0.7f)     { b.y = b.r * 0.7f; b.vy = 0; }
        if (pushing) {
            float dx = b.x - push_x, dy = b.y - push_y, d2 = dx * dx + dy * dy;
            if (d2 < 900 && d2 > 1) { float d = sqrtf(d2); b.x += dx / d * 40 * dt; b.y += dy / d * 40 * dt; }
        }
        if (stopping) b.r -= dt * (b.phase == BLOB_WARMING ? 12 : 2.5f);   // cooled wax shrinks away as it settles
        if (b.r < 2) b.alive = false;
    }
    if (stopping && !any) { done = true; }

    // The field on the coarse grid, then colour: wax, its glowing rim, or the liquid
    lv_color_t bg_top = COL_SCENE_LAVA_BG_TOP, bg_low = COL_SCENE_LAVA_BG_LOW;
    lv_color_t wax = pixfb_mix(COL_SCENE_LAVA_WAX, COL_SCENE_LAVA_BG_LOW, cool * 0.6f);
    lv_color_t rim = pixfb_mix(COL_SCENE_LAVA_RIM, COL_SCENE_LAVA_WAX, cool * 0.6f);
    for (int gy = 0; gy < gh; gy++) {
        float py = gy * CELL + 1;
        lv_color_t liquid = pixfb_mix(bg_top, bg_low, py / H);
        for (int gx = 0; gx < gw; gx++) {
            float px = gx * CELL + 1, f = 0;
            for (int i = 0; i < MAX_BLOBS; i++) {
                const Blob &b = blobs[i];
                if (!b.alive) continue;
                float dx = px - b.x, dy = (py - b.y) * 1.3f, r = b.r * (1 + pulse);
                float d2 = dx * dx + dy * dy, r2 = r * r;
                if (d2 > 9 * r2) continue;   // beyond 3 radii its pull is too faint to matter
                f += r2 / (d2 + 1);
            }
            lv_color_t c = f > 1.15f ? wax : f > 0.85f ? rim : f > 0.5f ? pixfb_mix(liquid, rim, (f - 0.5f) / 0.7f) : liquid;
            lv_color_t *row = fb.buf + (gy * CELL) * W + gx * CELL;
            row[0] = row[1] = c;
            row[W] = row[W + 1] = c;
        }
    }
    pixfb_touch(fb, 0, 0, gw * CELL, gh * CELL);
}

static void tick(uint32_t dt_ms) {
    if (oom || done) return;
    tick_frame(dt_ms);
    pixfb_flush(fb);
}

static void request_stop() { stopping = true; pushing = false; }
static bool is_done()      { return done; }

static void touch(SceneTouch type, int x, int y) {
    if (stopping) return;
    if (type == SCENE_TOUCH_PRESS) add_blob(x, y, H * 0.09f);
    pushing = type != SCENE_TOUCH_RELEASE;
    push_x = x; push_y = y;
}

static void event(const SceneEvent &e) {
    if (e.type != SCENE_EV_BEAT || stopping) return;
    for (int i = 0; i < MAX_BLOBS; i++)   // a beat hurries resting blobs on their way
        if (blobs[i].alive) blobs[i].timer -= e.strength / 150.0f;
}

static void finish() { pixfb_free(fb); }

const Scene scene_lava = { "lava", start, tick, request_stop, is_done, touch, finish, event };
