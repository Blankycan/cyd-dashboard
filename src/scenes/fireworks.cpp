#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Fireworks — rockets climb from behind a night skyline and burst into
// peonies, rings, and slow golden willows; the sparks fall under gravity,
// twinkle, and fade into the sky. Sky and skyline are painted once as the
// backdrop. When asked to stop, no more rockets go up and it's done once the
// last sparks have faded.
// Touch: tap to launch a rocket that bursts where you tapped.
// Events: with music playing, rockets launch on the beat (bigger bursts for
// stronger beats); without it they go up at a relaxed random pace. Enter
// launches a big one, and a Claude session finishing earns a golden willow.

static const int   MAX_ROCKETS = 6;
static const int   MAX_SPARKS  = 220;
static const float GRAVITY     = 38.0f;

enum Kind { PEONY, RING, WILLOW, DOUBLE };

struct Rocket { bool alive; float x, y, vy, apex; Kind kind; int size; lv_color_t col; };
struct Spark  { bool alive; float x, y, vx, vy, life, max_life, drag; lv_color_t col; bool twinkle; int px, py; };

static PixFb  fb;
static int    W, H, sky_bottom;
static Rocket rockets[MAX_ROCKETS];
static Spark  sparks[MAX_SPARKS];
static float  auto_acc, auto_next;
static uint32_t last_beat_launch;
static bool   stopping, done, oom;
static int    claude_was;   // working-session count before the latest change
static int    prev_rx[MAX_ROCKETS], prev_ry[MAX_ROCKETS];

static lv_color_t palette(int i) {
    switch (i % 4) {
        case 0:  return COL_SCENE_FW_1;
        case 1:  return COL_SCENE_FW_2;
        case 2:  return COL_SCENE_FW_3;
        default: return COL_SCENE_FW_4;
    }
}

static lv_color_t sky_at(int y) {
    return pixfb_mix(COL_SCENE_FW_SKY_TOP, COL_SCENE_FW_SKY_LOW, (float)y / sky_bottom);
}

static void paint_backdrop() {
    for (int y = 0; y < H; y++) pixfb_fill(fb, 0, y, W, 1, sky_at(y < sky_bottom ? y : sky_bottom));
    for (int i = 0; i < W / 6; i++)
        pixfb_fill(fb, random(0, W), random(0, sky_bottom * 3 / 4), 1, 1, COL_SCENE_FW_STAR);
    // Skyline along the bottom, a few windows lit
    int x = 0;
    while (x < W) {
        int bw = random(8, 20), bh = random(H / 10, H / 4);
        pixfb_fill(fb, x, H - bh, bw, bh, COL_SCENE_FW_CITY);
        for (int yy = H - bh + 3; yy < H - 2; yy += 4)
            for (int xx = x + 2; xx < x + bw - 2; xx += 4)
                if (random(0, 100) < 25) pixfb_fill(fb, xx, yy, 1, 2, COL_SCENE_FW_CITY_LIGHT);
        x += bw;
    }
    pixfb_bg_save(fb);
}

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_FW_SKY_TOP);
    done = oom;
    if (oom) return;
    sky_bottom = h * 80 / 100;
    paint_backdrop();
    if (!fb.bg) { done = oom = true; return; }
    for (int i = 0; i < MAX_ROCKETS; i++) { rockets[i].alive = false; prev_rx[i] = -1; }
    for (int i = 0; i < MAX_SPARKS; i++)  { sparks[i].alive = false; sparks[i].px = -1; }
    auto_acc = 0;
    auto_next = 0.5f;
    last_beat_launch = 0;
    claude_was = scene_ctx().claude_working;
    stopping = false;
}

static void launch(float x, float apex, Kind kind, int size) {
    for (int i = 0; i < MAX_ROCKETS; i++) {
        Rocket &r = rockets[i];
        if (r.alive) continue;
        // Speed that just reaches the apex under gravity
        float rise = H - apex;
        r = { true, x, (float)H, -sqrtf(2 * GRAVITY * 1.6f * rise), apex, kind, size, palette(random(0, 4)) };
        return;
    }
}

static void launch_random(int size) {
    Kind k = (Kind)random(0, 4);
    launch(scene_randf(W * 0.12f, W * 0.88f), scene_randf(H * 0.12f, H * 0.42f), k, size);
}

static void add_spark(float x, float y, float vx, float vy, float life, float drag, lv_color_t col, bool twinkle) {
    for (int i = 0; i < MAX_SPARKS; i++) {
        Spark &s = sparks[i];
        if (s.alive) continue;
        int px = s.px, py = s.py;   // keep the old position so it still gets erased
        s = { true, x, y, vx, vy, life, life, drag, col, twinkle, px, py };
        return;
    }
}

static void burst(const Rocket &r) {
    int n = 18 + r.size * 8;
    float speed = 22.0f + r.size * 7;
    lv_color_t second = palette(random(0, 4));
    for (int i = 0; i < n; i++) {
        float a = i * 6.28318f / n + scene_randf(-0.1f, 0.1f);
        float s = r.kind == RING ? speed : speed * scene_randf(0.35f, 1.0f);
        switch (r.kind) {
            case WILLOW:
                add_spark(r.x, r.y, cosf(a) * s * 0.6f, sinf(a) * s * 0.6f, scene_randf(1.8f, 2.6f), 0.6f,
                          COL_SCENE_FW_GOLD, true);
                break;
            case DOUBLE:
                add_spark(r.x, r.y, cosf(a) * s, sinf(a) * s, scene_randf(0.9f, 1.3f), 1.4f, r.col, false);
                add_spark(r.x, r.y, cosf(a) * s * 0.5f, sinf(a) * s * 0.5f, scene_randf(0.9f, 1.3f), 1.4f, second, false);
                break;
            default:
                add_spark(r.x, r.y, cosf(a) * s, sinf(a) * s, scene_randf(1.0f, 1.5f), 1.2f, r.col, r.kind == PEONY);
        }
    }
}

static void frame(uint32_t dt_ms) {
    if (oom || done) return;
    float dt = dt_ms / 1000.0f;
    const SceneContext &ctx = scene_ctx();

    // Without music (or without beat detection), launch on a relaxed timer
    bool beat_driven = ctx.music == SCENE_MUSIC_PLAYING && ctx.last_beat_ms && millis() - ctx.last_beat_ms < 3000;
    if (!stopping && !beat_driven) {
        auto_acc += dt;
        if (auto_acc >= auto_next) { auto_acc = 0; auto_next = scene_randf(0.8f, 2.2f); launch_random(random(1, 4)); }
    }

    // Erase last frame
    for (int i = 0; i < MAX_ROCKETS; i++)
        if (prev_rx[i] >= 0) { pixfb_bg_restore(fb, prev_rx[i], prev_ry[i], 1, 4); prev_rx[i] = -1; }
    for (int i = 0; i < MAX_SPARKS; i++)
        if (sparks[i].px >= 0) { pixfb_bg_restore(fb, sparks[i].px, sparks[i].py, 1, 1); sparks[i].px = -1; }

    // Rockets climb, then burst at the apex
    bool any = false;
    for (int i = 0; i < MAX_ROCKETS; i++) {
        Rocket &r = rockets[i];
        if (!r.alive) continue;
        any = true;
        r.vy += GRAVITY * 1.6f * dt;
        r.y  += r.vy * dt;
        r.x  += scene_randf(-4, 4) * dt;
        if (r.vy >= 0 || r.y <= r.apex) { r.alive = false; burst(r); continue; }
        int x = (int)r.x, y = (int)r.y;
        pixfb_fill(fb, x, y, 1, 1, COL_SCENE_FW_ROCKET);
        pixfb_fill(fb, x, y + 1, 1, 3, pixfb_mix(COL_SCENE_FW_ROCKET, sky_at(y), 0.6f));
        prev_rx[i] = x; prev_ry[i] = y;
    }

    // Sparks: drag, gravity, fade toward the sky behind them
    for (int i = 0; i < MAX_SPARKS; i++) {
        Spark &s = sparks[i];
        if (!s.alive) continue;
        s.life -= dt;
        if (s.life <= 0) { s.alive = false; continue; }
        any = true;
        float d = expf(-s.drag * dt);
        s.vx *= d; s.vy = s.vy * d + GRAVITY * dt;
        s.x += s.vx * dt; s.y += s.vy * dt;
        int x = (int)s.x, y = (int)s.y;
        if (x < 0 || x >= W || y < 0 || y >= H) { s.alive = false; continue; }
        if (s.twinkle && s.life < s.max_life * 0.5f && random(0, 3) == 0) continue;   // flicker out
        float fade = 1.0f - s.life / s.max_life;
        pixfb_fill(fb, x, y, 1, 1, pixfb_mix(s.col, sky_at(y < sky_bottom ? y : sky_bottom), fade * fade));
        s.px = x; s.py = y;
    }

    if (stopping && !any) done = true;
}

static void tick(uint32_t dt_ms) {
    if (oom) return;
    frame(dt_ms);
    pixfb_flush(fb);   // hand this frame's changes to LVGL
}

static void request_stop() { stopping = true; }
static bool is_done()      { return done; }

static void touch(SceneTouch type, int x, int y) {
    if (type != SCENE_TOUCH_PRESS || stopping) return;
    launch(x, y < H * 0.1f ? H * 0.1f : y, (Kind)random(0, 4), 2);
}

static void event(const SceneEvent &e) {
    if (stopping) return;
    uint32_t now = millis();
    if (e.type == SCENE_EV_BEAT && now - last_beat_launch > 350) {
        last_beat_launch = now;
        launch_random(e.strength > 70 ? 3 : e.strength > 35 ? 2 : 1);
    }
    if (e.type == SCENE_EV_KEY && e.key == SCENE_KEY_ENTER) launch_random(4);
    if (e.type == SCENE_EV_CLAUDE) {
        // A session finished: a golden willow straight up the middle
        if (e.value < claude_was) launch(W / 2.0f, H * 0.18f, WILLOW, 4);
        claude_was = e.value;
    }
}

static void finish() { pixfb_free(fb); }

const Scene scene_fireworks = { "fireworks", start, tick, request_stop, is_done, touch, finish, event };
SCENE_REGISTER(scene_fireworks, "Fireworks");
