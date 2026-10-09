#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Spectrum analyser — a hi-fi style LED bar display: one bar per band of the
// playing music (12 bands, bass on the left), lit segment by segment from the
// bottom, with peak caps that hang for a moment and then drop, and a dim
// reflection on the "glass" below. Each bar is only redrawn when it changes.
// Events: beats flash the lit segments brighter. With music paused or off the
// bars idle in a slow low wave; without spectrum data from the companion they
// fake one from the bass and intensity. When asked to stop, everything drains.
// Touch: poke the bars — the ones under your finger jump up.

static const int NB        = SCENE_BANDS;
static const int SEG_H     = 3;
static const int SEG_PITCH = 4;
static const int REFL_SEGS = 4;
static const int MAX_SEGS  = 40;
static const float FALL    = 1.6f;   // bar height lost per second (fraction of full)

static PixFb fb;
static int   W, H, x0, bar_w, gap, top, base, nseg;
static float level[NB], peak[NB], peak_v[NB], peak_hold[NB], poke[NB], fake[NB];
static float pulse, idle_t;
static uint32_t no_data_ms;           // playing, but no spectrum arriving
static int   drawn_lit[NB], drawn_peak[NB], drawn_pulse;
static lv_color_t on_col[MAX_SEGS], off_col[MAX_SEGS];
static bool  stopping, done, oom;

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_SPEC_BG);
    done = oom;
    if (oom) return;

    gap   = w >= 200 ? 3 : 2;
    bar_w = (w - 12 - (NB - 1) * gap) / NB;
    x0    = (w - (NB * bar_w + (NB - 1) * gap)) / 2;
    top   = 4;
    base  = h - 4 - REFL_SEGS * SEG_PITCH;
    nseg  = (base - top) / SEG_PITCH;
    if (nseg > MAX_SEGS) nseg = MAX_SEGS;

    // Green-ish → amber → red up the bar, in the theme's own colours
    for (int k = 0; k < nseg; k++) {
        float t = nseg > 1 ? (float)k / (nseg - 1) : 0;
        on_col[k]  = t < 0.6f ? pixfb_mix(COL_SCENE_SPEC_LOW, COL_SCENE_SPEC_MID, t / 0.6f)
                              : pixfb_mix(COL_SCENE_SPEC_MID, COL_SCENE_SPEC_HIGH, (t - 0.6f) / 0.4f);
        off_col[k] = pixfb_mix(COL_SCENE_SPEC_OFF, on_col[k], 0.12f);
    }

    for (int i = 0; i < NB; i++) {
        level[i] = peak[i] = peak_v[i] = peak_hold[i] = poke[i] = fake[i] = 0;
        drawn_lit[i] = drawn_peak[i] = -1;   // forces the first draw
    }
    pulse = idle_t = 0;
    no_data_ms = 0;
    drawn_pulse = -1;
    stopping = false;
}

static void draw_bar(int i, int lit, int pk, float glow) {
    int x = x0 + i * (bar_w + gap);
    for (int k = 0; k < nseg; k++) {
        int y = base - (k + 1) * SEG_PITCH + 1;
        lv_color_t c = k < lit ? pixfb_mix(on_col[k], COL_SCENE_SPEC_PEAK, glow) : off_col[k];
        if (k == pk && pk >= lit && pk > 0) c = COL_SCENE_SPEC_PEAK;
        pixfb_fill(fb, x, y, bar_w, SEG_H, c);
    }
    // Reflection: the lowest segments again, mirrored and fading into the glass
    for (int r = 0; r < REFL_SEGS; r++) {
        int y = base + 1 + r * SEG_PITCH;
        lv_color_t c = r < lit ? pixfb_mix(on_col[r], COL_SCENE_SPEC_BG, 0.62f + 0.09f * r)
                               : COL_SCENE_SPEC_BG;
        pixfb_fill(fb, x, y, bar_w, SEG_H, c);
    }
}

static float target_for(int i, const SceneContext &ctx, bool have_sp) {
    if (stopping) return 0;
    if (ctx.music != SCENE_MUSIC_PLAYING)
        return 0.10f + 0.07f * sinf(idle_t * 1.7f - i * 0.55f);
    if (have_sp) return ctx.spectrum[i] / 99.0f;
    return fake[i];
}

static void tick(uint32_t dt_ms) {
    if (oom || done) return;
    float dt = dt_ms / 1000.0f;
    const SceneContext &ctx = scene_ctx();
    idle_t += dt;
    pulse  *= expf(-6.0f * dt);

    bool have_sp = false;
    for (int i = 0; i < NB; i++) if (ctx.spectrum[i]) { have_sp = true; break; }
    if (ctx.music == SCENE_MUSIC_PLAYING && !have_sp) no_data_ms += dt_ms; else no_data_ms = 0;
    // No spectrum from the companion: wander around the bass (left) and intensity
    if (no_data_ms > 1000) {
        for (int i = 0; i < NB; i++) {
            float aim = (i < 4 ? ctx.bass : ctx.intensity * 0.8f) / 100.0f * (1.0f - i * 0.03f);
            fake[i] += (aim - fake[i]) * 0.2f + scene_randf(-0.08f, 0.08f);
            fake[i] = fminf(1, fmaxf(0, fake[i]));
        }
    }

    bool all_down = true;
    int  pq = (int)(pulse * 8);
    for (int i = 0; i < NB; i++) {
        poke[i] *= expf(-3.0f * dt);
        float t = fminf(1.0f, target_for(i, ctx, have_sp) + poke[i]);
        if (t > level[i]) level[i] += (t - level[i]) * 0.6f;
        else              level[i] = fmaxf(t, level[i] - FALL * dt);

        if (level[i] >= peak[i]) { peak[i] = level[i]; peak_hold[i] = 0.45f; peak_v[i] = 0; }
        else if (peak_hold[i] > 0) peak_hold[i] -= dt;
        else {
            peak_v[i] += 2.2f * dt;
            peak[i] = fmaxf(level[i], peak[i] - peak_v[i] * dt);
        }
        if (level[i] > 0.01f || peak[i] > 0.02f) all_down = false;

        int lit = (int)(level[i] * nseg + 0.5f);
        int pk  = (int)(peak[i] * nseg);
        if (pk >= nseg) pk = nseg - 1;
        if (lit != drawn_lit[i] || pk != drawn_peak[i] || (pq != drawn_pulse && lit > 0)) {
            draw_bar(i, lit, pk, pulse * 0.45f);
            drawn_lit[i] = lit;
            drawn_peak[i] = pk;
        }
    }
    drawn_pulse = pq;
    if (stopping && all_down) done = true;
    pixfb_flush(fb);
}

static void request_stop() { stopping = true; }
static bool is_done()      { return done; }

static void touch(SceneTouch type, int x, int) {
    if (stopping || type == SCENE_TOUCH_RELEASE) return;
    int hit = (x - x0) / (bar_w + gap);
    for (int i = 0; i < NB; i++) {
        float p = 1.0f - 0.28f * abs(i - hit);
        if (p > poke[i]) poke[i] = p;
    }
}

static void event(const SceneEvent &e) {
    if (e.type == SCENE_EV_BEAT) pulse = fminf(1.0f, pulse + 0.5f + e.strength / 200.0f);
}

static void finish() { pixfb_free(fb); }

const Scene scene_spectrum = { "spectrum", start, tick, request_stop, is_done, touch, finish, event };
SCENE_REGISTER(scene_spectrum, "Spectrum");
