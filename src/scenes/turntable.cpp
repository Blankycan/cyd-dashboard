#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Turntable — a record deck seen from above on the left, two analogue VU
// meters on the right. The record spins at 33 rpm nudged by the tempo, its
// label and the strobe dots on the platter rim turning with it; the tonearm
// tracks slowly inward through the song. The plinth, platter, grooves and
// meter faces are painted once as the backdrop; each frame redraws only the
// deck's moving parts and the two needles.
// Events: playing → the arm swings over and drops onto the record; paused →
// the platter spins down and the arm hovers; a new track lifts the arm back to
// the outer groove and drops it again ("needle drop"). Beats flash the strobe
// dots and, if strong, the meters' peak lights. The meters follow intensity
// (L) and bass (R). When asked to stop, the arm goes home and the platter
// spins down, then it's done.
// Touch: tap the record to scratch it.

static const float PI_F      = 3.14159265f;
static const float RPS_33    = 33.333f / 60.0f;
static const float TRACK_S   = 240.0f;   // the arm reaches the inner groove after this long
static const int   STROBE_N  = 40;

static PixFb fb;
static int   W, H;
static int   cx, cy, R, label_r;          // platter
static float px, py, arm_len;             // tonearm pivot
static float ang_rest, ang_outer, ang_inner;
static int   dyn_x0, dyn_y0, dyn_x1, dyn_y1;   // the deck's redraw box
static int   mx0, mw, mh, my;             // meters: first face x, face size, top
static float rot, rps, scratch;
static float arm_ang, lift, progress;
static float vu[2], vu_v[2], led[2];
static float strobe_flash;
static bool  stopping, done, oom;
static int   drawn_bpm;

// ---------------------------------------------------------------------------
// Geometry

static void needle_at(float a, float &x, float &y) {
    x = px + arm_len * cosf(a);
    y = py + arm_len * sinf(a);
}

// Arm angle that puts the needle `r` px from the spindle (sweeping from rest
// toward the record)
static float angle_for_radius(float r) {
    float best = ang_rest, best_err = 1e9f;
    for (float a = ang_rest; a < ang_rest + PI_F * 0.9f; a += 0.003f) {
        float x, y;
        needle_at(a, x, y);
        float err = fabsf(hypotf(x - cx, y - cy) - r);
        if (err < best_err) { best_err = err; best = a; }
        if (err > best_err + 4) break;   // moving away again
    }
    return best;
}

static float groove_angle() { return ang_outer + (ang_inner - ang_outer) * progress; }

// ---------------------------------------------------------------------------
// Backdrop

static void paint_platter() {
    // Soft shadow, then platter rim, record with grooves and two light sheens
    pixfb_fill_circle(fb, cx + 2, cy + 2, R, COL_SCENE_TT_SHADOW);
    for (int y = cy - R; y <= cy + R; y++) {
        for (int x = cx - R; x <= cx + R; x++) {
            float dx = x - cx, dy = y - cy, d = sqrtf(dx * dx + dy * dy);
            if (d > R + 0.5f) continue;
            lv_color_t c;
            if (d > R - 2.5f)      c = COL_SCENE_TT_RIM;
            else if (d > R - 4.0f) c = COL_SCENE_TT_SHADOW;
            else {
                c = COL_SCENE_TT_VINYL;
                bool grooved = d < R - 6.5f && d > label_r + 4;
                if (grooved && fmodf(d, 2.5f) < 0.7f) c = COL_SCENE_TT_GROOVE;
                // Light catching the grooves at two opposite angles
                float a = atan2f(dy, dx);
                static const float SHEEN_ANG[2] = { -2.36f, 0.79f };
                for (float s : SHEEN_ANG) {
                    float da = fabsf(a - s);
                    if (da > PI_F) da = 2 * PI_F - da;
                    if (da < 0.22f) c = pixfb_mix(c, COL_SCENE_TT_SHEEN, (1 - da / 0.22f) * (grooved ? 0.35f : 0.15f));
                }
            }
            pixfb_px(fb, x, y, c);
        }
    }
    pixfb_touch(fb, cx - R, cy - R, 2 * R + 1, 2 * R + 1);
}

static void paint_arm_base() {
    pixfb_fill_circle(fb, (int)px, (int)py, 7, COL_SCENE_TT_SHADOW);
    pixfb_fill_circle(fb, (int)px, (int)py, 6, COL_SCENE_TT_RIM);
    // Arm rest: a little post where the needle parks
    float rx, ry;
    needle_at(ang_rest, rx, ry);
    pixfb_fill(fb, (int)rx - 2, (int)ry - 1, 5, 4, COL_SCENE_TT_RIM);
}

static void meter_geom(int m, int &x, int &pvx, int &pvy, int &len) {
    x   = mx0 + m * (mw + 6);
    pvx = x + mw / 2;
    pvy = my + mh + 6;          // pivot just below the face, hidden by the base strip
    len = mh + 1;
}

static void paint_meters() {
    for (int m = 0; m < 2; m++) {
        int x, pvx, pvy, len;
        meter_geom(m, x, pvx, pvy, len);
        pixfb_fill(fb, x - 1, my - 1, mw + 2, mh + 2, COL_SCENE_TT_SHADOW);
        pixfb_fill(fb, x, my, mw, mh, COL_SCENE_TT_FACE);
        // Scale: ticks along an arc, the top end in red
        for (int k = 0; k <= 10; k++) {
            float a = -0.85f + 1.7f * k / 10;
            lv_color_t c = k >= 7 ? COL_SCENE_TT_RED : COL_SCENE_TT_TICK;
            float r0 = len - 6, r1 = len - (k % 5 == 0 ? 1 : 3);
            pixfb_line(fb, pvx + (int)(r0 * sinf(a)), pvy - (int)(r0 * cosf(a)),
                           pvx + (int)(r1 * sinf(a)), pvy - (int)(r1 * cosf(a)), c);
        }
        pixfb_text(fb, pvx - pixfb_text_width("VU") / 2, my + mh - 16, "VU", COL_SCENE_TT_TICK);
        pixfb_fill(fb, x, my + mh - 5, mw, 5, COL_SCENE_TT_SHADOW);   // base strip over the pivot
        pixfb_text(fb, pvx - 2, my + mh + 4, m ? "R" : "L", COL_SCENE_TT_TEXT);
    }
}

// ---------------------------------------------------------------------------

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_TT_BG);
    done = oom;
    if (oom) return;

    R  = h / 2 - 4;
    cx = R + 6;
    cy = h / 2;
    label_r = R * 30 / 100;
    px = cx + R + 15;
    py = cy - R + 7;
    arm_len = hypotf(px - cx, py - cy) - 4;
    ang_rest  = PI_F * 0.53f;             // pointing down, just right of the platter
    ang_outer = angle_for_radius(R - 7);
    ang_inner = angle_for_radius(label_r + 5);

    float nx, ny;
    needle_at(ang_rest, nx, ny);
    dyn_x0 = cx - R - 1;  dyn_y0 = 0;
    dyn_x1 = (int)px + 10; dyn_y1 = h - 1;

    mx0 = dyn_x1 + 8;
    mw  = (w - mx0 - 4 - 6) / 2;
    mh  = h * 52 / 100;
    my  = 6;

    paint_platter();
    paint_arm_base();
    paint_meters();
    pixfb_bg_save(fb);
    if (!fb.bg) { done = oom = true; return; }

    rot = 0; rps = 0; scratch = 0;
    arm_ang = ang_rest; lift = 0; progress = 0;
    vu[0] = vu[1] = vu_v[0] = vu_v[1] = led[0] = led[1] = 0;
    strobe_flash = 0;
    drawn_bpm = -1;
    stopping = false;
}

// ---------------------------------------------------------------------------
// Frame

static void draw_deck() {
    pixfb_bg_restore(fb, dyn_x0, dyn_y0, dyn_x1 - dyn_x0 + 1, dyn_y1 - dyn_y0 + 1);

    // Strobe dots on the rim
    lv_color_t sc = pixfb_mix(COL_SCENE_TT_STROBE, COL_SCENE_TT_STROBE_BEAT, strobe_flash);
    for (int k = 0; k < STROBE_N; k++) {
        float a = rot + k * 2 * PI_F / STROBE_N;
        pixfb_fill(fb, cx + (int)lroundf((R - 1) * cosf(a)), cy + (int)lroundf((R - 1) * sinf(a)), 1, 1, sc);
    }

    // Label, with "printing" that turns with it, and the spindle
    pixfb_fill_circle(fb, cx, cy, label_r, COL_SCENE_TT_LABEL);
    float ca = cosf(rot), sa = sinf(rot);
    for (int j = -1; j <= 1; j++) {
        float len = j == 0 ? label_r * 0.75f : label_r * 0.5f;
        float ox = -sa * j * 3, oy = ca * j * 3;   // offset across the line
        int ax = cx + (int)(ox + ca * 3), ay = cy + (int)(oy + sa * 3);
        pixfb_line(fb, ax, ay, ax + (int)(ca * len * 0.6f), ay + (int)(sa * len * 0.6f), COL_SCENE_TT_LABEL_INK);
    }
    pixfb_fill(fb, cx - 1, cy - 1, 2, 2, COL_SCENE_TT_SHEEN);
    // A speck of dust on the vinyl, so the record itself visibly turns
    float da = rot * 1.0f + 2.0f;
    pixfb_fill(fb, cx + (int)((R - 12) * cosf(da)), cy + (int)((R - 12) * sinf(da)), 1, 1, COL_SCENE_TT_SHEEN);

    // Tonearm: shadow (further away the higher it's lifted), arm, headshell, counterweight
    float nx, ny;
    needle_at(arm_ang, nx, ny);
    float c = cosf(arm_ang), s = sinf(arm_ang);
    int wx = (int)(px - 9 * c), wy = (int)(py - 9 * s);
    int sh = 1 + (int)(lift * 3);
    pixfb_line(fb, wx + sh, wy + sh, (int)nx + sh, (int)ny + sh, COL_SCENE_TT_ARM_SHADOW);
    for (int o = 0; o <= 1; o++)
        pixfb_line(fb, wx + o, wy, (int)nx + o, (int)ny, COL_SCENE_TT_ARM);
    // Headshell: angled a little inward at the end
    float hc = cosf(arm_ang + 0.5f), hs = sinf(arm_ang + 0.5f);
    pixfb_fill_tri(fb, (int)nx, (int)ny, (int)(nx - 6 * c + 2 * hc), (int)(ny - 6 * s + 2 * hs),
                   (int)(nx + 2 * hc), (int)(ny + 2 * hs), COL_SCENE_TT_ARM);
    pixfb_fill_circle(fb, wx, wy, 3, COL_SCENE_TT_RIM);
    pixfb_fill_circle(fb, (int)px, (int)py, 3, COL_SCENE_TT_ARM);
}

static void draw_meters(int bpm) {
    for (int m = 0; m < 2; m++) {
        int x, pvx, pvy, len;
        meter_geom(m, x, pvx, pvy, len);
        pixfb_bg_restore(fb, x, my, mw, mh - 5);
        float a = -0.85f + 1.7f * fminf(1.1f, fmaxf(-0.05f, vu[m]));
        int tx = pvx + (int)((len - 3) * sinf(a)), ty = pvy - (int)((len - 3) * cosf(a));
        // Clip the needle to the face: it starts where it comes out of the base strip
        float t0 = 11.0f / ((len - 3) * fmaxf(0.5f, cosf(a)));
        int bx = pvx + (int)((tx - pvx) * t0), byy = pvy + (int)((ty - pvy) * t0);
        pixfb_line(fb, bx, byy, tx, ty, COL_SCENE_TT_NEEDLE);
        // Peak light, top right of the face
        pixfb_fill_circle(fb, x + mw - 5, my + 4, 2, pixfb_mix(COL_SCENE_TT_LED_OFF, COL_SCENE_TT_RED, led[m]));
    }

    if (bpm != drawn_bpm) {
        drawn_bpm = bpm;
        int y = my + mh + 15;
        pixfb_bg_restore(fb, mx0, y, W - mx0, PIXFB_DIGIT_H);
        if (bpm > 0) {
            int tw = pixfb_number_width(bpm) + 4 + pixfb_text_width("BPM");
            int x = mx0 + (2 * mw + 6 - tw) / 2;
            x += pixfb_number(fb, x, y, bpm, COL_SCENE_TT_TEXT) + 4;
            pixfb_text(fb, x, y, "BPM", COL_SCENE_TT_TEXT);
        }
    }
}

static void tick(uint32_t dt_ms) {
    if (oom || done) return;
    float dt = dt_ms / 1000.0f;
    const SceneContext &ctx = scene_ctx();
    bool playing = ctx.music == SCENE_MUSIC_PLAYING && !stopping;
    bool paused  = ctx.music == SCENE_MUSIC_PAUSED && !stopping;

    // Where the arm wants to be, and whether down on the record
    float goal = playing || paused ? groove_angle() : ang_rest;
    bool  down = !paused;          // on the record while playing, on its rest otherwise
    bool  at_goal = fabsf(arm_ang - goal) < 0.01f;
    if (!at_goal) {                // lift first, then swing over
        lift = fminf(1, lift + 3.0f * dt);
        if (lift >= 1) {
            float step = 0.9f * dt;
            arm_ang += fmaxf(-step, fminf(step, goal - arm_ang));
        }
    } else {
        lift = down ? fmaxf(0, lift - 2.0f * dt) : fminf(1, lift + 3.0f * dt);
    }
    bool on_record = playing && at_goal && lift <= 0;
    if (on_record) {
        progress = fminf(1, progress + dt / TRACK_S);
        arm_ang = groove_angle();
    }

    // Platter: 33 rpm, nudged by the tempo; spins up and down like a real one
    float want = 0;
    if (playing) {
        want = RPS_33;
        if (ctx.bpm) want *= fminf(1.35f, fmaxf(0.75f, ctx.bpm / 120.0f));
    }
    float acc = want > rps ? 0.8f : 0.35f;
    rps += fmaxf(-acc * dt, fminf(acc * dt, want - rps));
    scratch *= expf(-5.0f * dt);
    rot += 2 * PI_F * (rps + scratch) * dt;
    if (rot > 2 * PI_F) rot -= 2 * PI_F;
    if (rot < 0) rot += 2 * PI_F;
    strobe_flash *= expf(-6.0f * dt);

    // Meters: a damped spring toward the level, like a real needle
    float target[2] = { 0, 0 };
    if (on_record) {
        target[0] = 0.15f + 0.75f * ctx.intensity / 100.0f;
        target[1] = 0.10f + 0.80f * ctx.bass / 100.0f;
    }
    for (int m = 0; m < 2; m++) {
        vu_v[m] += ((target[m] - vu[m]) * 60.0f - vu_v[m] * 9.0f) * dt;
        vu[m]   += vu_v[m] * dt;
        led[m]  *= expf(-4.0f * dt);
    }

    draw_deck();
    draw_meters(on_record ? ctx.bpm : 0);
    pixfb_flush(fb);

    if (stopping && rps < 0.01f && fabsf(arm_ang - ang_rest) < 0.01f && lift <= 0) done = true;
}

static void request_stop() { stopping = true; }
static bool is_done()      { return done; }

static void touch(SceneTouch type, int x, int y) {
    if (type != SCENE_TOUCH_PRESS || stopping) return;
    if (hypotf(x - cx, y - cy) <= R) scratch = rps > 0.1f ? -2.5f : 1.2f;
}

static void event(const SceneEvent &e) {
    if (e.type == SCENE_EV_TRACK) progress = 0;   // lift, back to the outer groove, drop
    if (e.type == SCENE_EV_BEAT) {
        strobe_flash = fminf(1.0f, strobe_flash + 0.5f + e.strength / 200.0f);
        if (e.strength > 45) {
            int m = random(0, 2);
            led[m] = 1;
            vu_v[m] += 2.0f;   // the needle kicks
        }
    }
}

static void finish() { pixfb_free(fb); }

const Scene scene_turntable = { "turntable", start, tick, request_stop, is_done, touch, finish, event };
SCENE_REGISTER(scene_turntable, "Turntable");
