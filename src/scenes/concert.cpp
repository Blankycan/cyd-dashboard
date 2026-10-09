#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Concert — the view from the crowd: a stage with a band, a lighting truss
// whose beams sweep through the haze, and two rows of silhouettes in front.
// The hall, truss, stage and speaker stacks are painted once as the backdrop;
// each frame redraws the beams, the band and the crowd over it.
// Events: with music playing the band is on stage, the beams sweep in time
// (tempo) and flash and change colour on beats, the crowd bounces, and strong
// beats throw hands in the air; in quiet passages phone lights come out. With
// the music paused or off it's between sets: the stage is empty, the lights
// low and the crowd waits. When asked to stop, the lights fade out.
// Touch: tap the crowd to make people jump; tap the stage to flash the lights.

static const int   MAX_PEOPLE = 64;
static const int   N_LIGHTS   = 6;
static const float PI_F       = 3.14159265f;

struct Person {
    int16_t x;
    uint8_t row;          // 0 = back, 1 = front
    float   bounce;       // px up, springing back
    float   bounce_v;
    float   raise;        // 0-1, arms up
    float   sway;         // phase
    bool    phone;        // holds a phone up in quiet bits
};

static PixFb  fb;
static int    W, H, truss_y, stage_y, row_y[2];
static Person people[MAX_PEOPLE];
static int    n_people;
static float  t, beam_phase, flash, master, phone_amt, band_amt, palette_t;
static int    palette;
static bool   stopping, done, oom;

// Band, as silhouettes standing on the stage: guitarist, singer, bassist
static const char *const GUITARIST[] = {
    "..11....", "..11....", "...1....", ".1111...", "1.1111..", "1.11.11.",
    "..11..11", "..11....", "..1.1...", "..1.1...", "..1.1...", ".11.11..",
};
static const char *const SINGER[] = {
    "...11...", "...11..1", "....1.1.", "..1111..", ".1.11...", ".1.11...",
    "...11...", "...11...", "...1.1..", "...1.1..", "...1.1..", "..11.11.",
};
static const char *const DRUMS[] = {
    "....11....", "....11....", ".1.1111.1.", "1111111111", ".11.11.11.",
    "1111..1111", "1111..1111", ".11....11.",
};

static void paint_backdrop() {
    for (int y = 0; y < stage_y; y++)
        pixfb_fill(fb, 0, y, W, 1, pixfb_mix(COL_SCENE_CONC_BG_TOP, COL_SCENE_CONC_BG_LOW, (float)y / stage_y));
    for (int y = stage_y; y < H; y++)
        pixfb_fill(fb, 0, y, W, 1, COL_SCENE_CONC_FLOOR);

    // Truss: a bar with a zig-zag lattice
    pixfb_fill(fb, 0, truss_y, W, 1, COL_SCENE_CONC_TRUSS);
    pixfb_fill(fb, 0, truss_y + 4, W, 1, COL_SCENE_CONC_TRUSS);
    for (int x = 0; x < W; x += 6) pixfb_line(fb, x, truss_y, x + 3, truss_y + 4, COL_SCENE_CONC_TRUSS);

    // Speaker stacks at the sides, and the stage deck
    int sw = W / 14, sh = (stage_y - truss_y) / 2;
    for (int side = 0; side < 2; side++) {
        int x = side ? W - 4 - sw : 4;
        for (int k = 0; k < 2; k++) {
            int y = stage_y - (k + 1) * (sh / 2);
            pixfb_fill(fb, x, y, sw, sh / 2 - 1, COL_SCENE_CONC_SPEAKER);
            pixfb_fill_circle(fb, x + sw / 2, y + sh / 4, sw / 3, COL_SCENE_CONC_FLOOR);
        }
    }
    pixfb_fill(fb, 0, stage_y, W, 4, COL_SCENE_CONC_STAGE);
    pixfb_bg_save(fb);
}

static void place_people() {
    n_people = 0;
    for (int row = 0; row < 2; row++) {
        int step = row ? 12 : 9;
        for (int x = row ? 4 : 0; x < W + 6 && n_people < MAX_PEOPLE; x += step + random(-2, 3)) {
            Person &p = people[n_people++];
            p.x = x;
            p.row = row;
            p.bounce = p.bounce_v = p.raise = 0;
            p.sway = scene_randf(0, 2 * PI_F);
            p.phone = random(0, 4) == 0;
        }
    }
}

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_CONC_BG_TOP);
    done = oom;
    if (oom) return;
    truss_y  = 2;
    stage_y  = h * 60 / 100;
    row_y[0] = stage_y + 8;          // shoulders of the back row
    row_y[1] = stage_y + 17;
    paint_backdrop();
    if (!fb.bg) { done = oom = true; return; }
    place_people();
    t = beam_phase = flash = palette_t = 0;
    master = 0;                      // lights come up at the start
    phone_amt = 0;
    band_amt = scene_ctx().music == SCENE_MUSIC_PLAYING ? 1 : 0;
    palette = 0;
    stopping = false;
}

static lv_color_t beam_col(int i) {
    static const int N = 3;
    int k = (i + palette) % N;
    return k == 0 ? COL_SCENE_CONC_BEAM1 : k == 1 ? COL_SCENE_CONC_BEAM2 : COL_SCENE_CONC_BEAM3;
}

static void draw_person(const Person &p, float phone_show) {
    int sy = row_y[p.row] - (int)p.bounce;
    lv_color_t c = p.row ? COL_SCENE_CONC_CROWD : COL_SCENE_CONC_CROWD_BACK;
    int hw = p.row ? 5 : 4;
    int sx = p.x + (int)(sinf(p.sway) * 1.2f);
    if (p.raise > 0.3f) {           // arms up, hands spread a little
        int reach = (int)(p.raise * (p.row ? 10 : 8));
        pixfb_line(fb, sx - hw + 1, sy + 1, sx - hw - 1, sy - reach, c);
        pixfb_line(fb, sx + hw - 1, sy + 1, sx + hw + 1, sy - reach, c);
    } else if (p.phone && phone_show > 0.05f) {   // one arm up with a phone light
        int ax = sx + hw, ay = sy - 7;
        pixfb_line(fb, sx + hw - 1, sy + 1, ax, ay, c);
        pixfb_fill(fb, ax, ay - 2, 1, 2, pixfb_mix(c, COL_SCENE_CONC_PHONE, phone_show));
    }
    pixfb_fill_circle(fb, sx, sy - (p.row ? 4 : 3), p.row ? 3 : 2, c);
    pixfb_fill(fb, sx - hw, sy, 2 * hw + 1, H - sy, c);
    pixfb_fill(fb, sx - hw + 1, sy - 1, 2 * hw - 1, 1, c);   // round the shoulders
}

static void tick(uint32_t dt_ms) {
    if (oom || done) return;
    float dt = dt_ms / 1000.0f;
    const SceneContext &ctx = scene_ctx();
    bool playing = ctx.music == SCENE_MUSIC_PLAYING && !stopping;
    t += dt;

    // Lights: up while running, out at the end; band on stage only while playing
    master = stopping ? fmaxf(0, master - 0.6f * dt) : fminf(1, master + 0.8f * dt);
    band_amt = playing ? fminf(1, band_amt + 0.7f * dt) : fmaxf(0, band_amt - 0.7f * dt);
    flash *= expf(-5.0f * dt);
    float quiet = playing ? (ctx.intensity < 35 ? 1.0f : 0.0f) : 1.0f;
    phone_amt += (quiet - phone_amt) * fminf(1, 1.5f * dt);

    // Beams sweep one cycle per two beats when the tempo is known
    float sweep_hz = playing ? (ctx.bpm ? ctx.bpm / 120.0f : 0.5f) : 0.12f;
    beam_phase += sweep_hz * dt * PI_F;

    pixfb_bg_restore(fb, 0, 0, W, H);

    float level = master * (playing ? 0.22f + 0.18f * ctx.intensity / 100.0f : 0.10f) + 0.35f * flash * master;
    int gap = W / (N_LIGHTS + 1);
    for (int i = 0; i < N_LIGHTS; i++) {
        int lx = gap * (i + 1), ly = truss_y + 6;
        float a = sinf(beam_phase + i * (i % 2 ? 1.1f : -0.9f)) * 0.55f;
        int tx = lx + (int)(tanf(a) * (H - ly));
        int spread = 6 + (int)(10 * ctx.bass / 100.0f) + (int)(6 * flash);
        lv_color_t c = beam_col(i);
        if (level > 0.01f) {
            pixfb_blend_tri(fb, lx, ly, tx - spread, H - 1, tx + spread, H - 1, c, level);
            pixfb_glow(fb, lx, ly, 5, c, fminf(1, level * 2.2f));
        }
        pixfb_fill(fb, lx - 2, truss_y + 5, 5, 2, pixfb_mix(COL_SCENE_CONC_TRUSS, c, master * (0.5f + 0.5f * flash)));
    }
    // Haze on the stage, breathing with the bass
    if (master > 0.01f)
        pixfb_glow(fb, W / 2, stage_y - 6, W / 5, COL_SCENE_CONC_BEAM1,
                   master * (0.15f + 0.25f * (playing ? ctx.bass / 100.0f : 0) + 0.2f * flash));

    // Band
    if (band_amt > 0.5f) {
        int hop = playing && ctx.last_beat_ms && millis() - ctx.last_beat_ms < 120 ? 1 : 0;
        lv_color_t pal[1] = { COL_SCENE_CONC_BAND };
        pixfb_art(fb, W / 2 - W / 6 - 4, stage_y - 12 - hop, GUITARIST, 12, pal);
        pixfb_art(fb, W / 2 - 4, stage_y - 12 - hop, SINGER, 12, pal);
        pixfb_art(fb, W / 2 + W / 6 - 4, stage_y - 12, GUITARIST, 12, pal, true);
        pixfb_art(fb, W / 2 - 5, stage_y - 22, DRUMS, 8, pal);
    }

    // Crowd: back row first; everyone springs back from their bounce
    for (int i = 0; i < n_people; i++) {
        Person &p = people[i];
        p.bounce_v += (-p.bounce * 90.0f - p.bounce_v * 9.0f) * dt;
        p.bounce   += p.bounce_v * dt;
        if (p.bounce < 0) { p.bounce = 0; p.bounce_v = 0; }
        p.raise *= expf(-(playing ? 0.9f : 2.0f) * dt);
        p.sway  += dt * (playing ? 2.0f : 0.8f);
        draw_person(p, phone_amt * master);
    }

    pixfb_touch(fb, 0, 0, W, H);
    pixfb_flush(fb);
    if (stopping && master <= 0) done = true;
}

static void request_stop() { stopping = true; }
static bool is_done()      { return done; }

static void jump(int x, float strength) {
    for (int i = 0; i < n_people; i++) {
        int d = abs(people[i].x - x);
        if (d > 20) continue;
        float s = strength * (1.0f - d / 24.0f);
        people[i].bounce_v += 40.0f * s;
        people[i].raise = fmaxf(people[i].raise, s);
    }
}

static void touch(SceneTouch type, int x, int y) {
    if (type != SCENE_TOUCH_PRESS || stopping) return;
    if (y >= stage_y) jump(x, 1.0f);
    else { flash = 1; palette++; }
}

static void event(const SceneEvent &e) {
    if (stopping) return;
    if (e.type == SCENE_EV_BEAT) {
        flash = fminf(1.0f, flash + 0.3f + e.strength / 200.0f);
        palette_t += 1;
        if (palette_t >= 8) { palette_t = 0; palette++; }   // new colours every 8 beats
        // Everyone bounces a little; a strong beat sends some hands up
        for (int i = 0; i < n_people; i++) {
            if (random(0, 3)) people[i].bounce_v += 12.0f + e.strength * 0.12f;
            if (e.strength > 55 && random(0, 100) < e.strength / 3) people[i].raise = 1;
        }
    }
    if (e.type == SCENE_EV_TRACK) { flash = 1; palette++; }
}

static void finish() { pixfb_free(fb); }

const Scene scene_concert = { "concert", start, tick, request_stop, is_done, touch, finish, event };
SCENE_REGISTER(scene_concert, "Concert");
