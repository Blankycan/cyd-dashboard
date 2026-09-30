#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include "lofi_art.h"
#include <math.h>

// Lofi girl — a girl in a big green sweater and headphones, chin on her fist,
// writing at a desk in front of a big window (her pixel art, the lamp, the mug,
// and the lamp's light live in assets/lofi/*.png; see the README there): rain runs down the glass, the city outside has windows that
// light up and go dark, a desk lamp glows and a mug steams. The room is painted
// once as the backdrop; rain, the girl, steam, notes and ink are redrawn over it.
// When asked to stop, the rain eases off and the last notes drift away.
// Touch: tap the window for a flash of lightning; tap the lamp (anywhere on the
// left, up to just past it) to switch it.
// Events: while music plays she nods along and notes float up from her
// headphones (see the nodding notes above frame()); every character you type is ink in her notebook, and she turns
// the page when it's full.

static const int MAX_DROPS = 28;
static const int MAX_NOTES = 5;
static const int MAX_LIT   = 60;

struct Drop { bool alive; float x, y, v; int len; };
struct Note { bool alive; float x, y, t; };
struct Lit  { int x, y; bool on; };

static PixFb fb;
static int   W, H;
static int   wx0, wy0, wx1, wy1;      // window glass
static int   desk_y;
static int   hx, hyy;                 // headphone cup, where the music notes come from
static int   girl_dx, girl_dy;        // where the 240×90 girl art sits in this area
static int   mug_x, mug_top;           // where the mug (prop_mug.png) stands — the steam rises from it
static int   props_dy;                 // the props are drawn for a 90-px-tall scene: keep them on the desk
static const int MAX_INK = 600;
static int16_t ink_x[MAX_INK], ink_y[MAX_INK];   // where each character goes, in writing order
static int   ink_slots;
static Drop  drops[MAX_DROPS];
static Note  notes[MAX_NOTES];
static Lit   lit[MAX_LIT];
static int   n_lit;
static int   ink;                     // characters written on this spread
static bool  lamp_on, stopping, done, oom;
static float nod, nod_phase, steam_t, write_t, note_acc, lit_acc;
static float flash_t = -1;             // seconds into a lightning flash, -1 when there's none
static uint8_t sky_top[320];           // per column: where the city's rooftops start (sky is above)
static const int BOLT_POINTS = 9;
static int16_t bolt_x[BOLT_POINTS], bolt_y[BOLT_POINTS];   // the bolt's zigzag, top to rooftops
static int16_t fork_x[4], fork_y[4];                       // and a short fork off it
static uint32_t since_beat;
static float beat_phase;               // 0-1 through her current nod period, nods on the wrap

// Nodding: a comfortable head-bob pace, and how beats steer it
static const float NOD_MIN_BPM     = 60, NOD_MAX_BPM = 140;   // fold the song's tempo into this range
static const float NOD_FALLBACK_BPM = 80;    // no tempo and no beats: a gentle sway
static const float NOD_LOCK_WINDOW = 0.25f;  // a beat this close (fraction of a period) to a nod steers it
static const float NOD_LOCK_GAIN   = 0.6f;   // how far a full-strength on-time beat pulls her into step
static const int   NOD_RAW_MIN_S   = 40;     // before the tempo is known, only beats this strong...
static const uint32_t NOD_RAW_GAP_MS = 400;  // ...and this far apart get a nod

static const uint16_t NOTE_SPRITE[7] = {   // a quaver, 5×7
    0b00110, 0b00101, 0b00100, 0b00100, 0b11100, 0b11100, 0b01000,
};

// ---------------------------------------------------------------------------
// Backdrop

static void draw_city() {
    // Skyline across the bottom of the window, with a grid of little windows
    randomSeed(W * 13 + H);
    n_lit = 0;
    for (int i = 0; i < W && i < 320; i++) sky_top[i] = wy1;
    int x = wx0;
    while (x < wx1) {
        int bw = random(10, 24), bh = random((wy1 - wy0) / 5, (wy1 - wy0) * 3 / 5);
        int top = wy1 - bh;
        pixfb_fill(fb, x, top, LV_MIN(bw, wx1 - x), bh, COL_SCENE_LOFI_CITY);
        for (int i = x; i < x + bw && i < wx1 && i < 320; i++) sky_top[i] = top;
        for (int yy = top + 3; yy < wy1 - 2; yy += 4)
            for (int xx = x + 2; xx < x + bw - 2 && xx < wx1 - 1; xx += 4)
                if (n_lit < MAX_LIT && random(0, 100) < 35) lit[n_lit++] = { xx, yy, random(0, 100) < 60 };
        x += bw + random(0, 3);
    }
    randomSeed(esp_random());
    for (int i = 0; i < n_lit; i++)
        if (lit[i].on) pixfb_fill(fb, lit[i].x, lit[i].y, 2, 2, COL_SCENE_LOFI_CITY_LIGHT);
}

static lv_color_t girl_color(char c);

static lv_color_t prop_color(char c) {
    switch (c) {
        case 'A': return COL_SCENE_LOFI_LAMP;
        case 'Y': return COL_SCENE_LOFI_LAMP_GLOW;
        case 'y': return COL_SCENE_LOFI_SHADE_RIM;
        case 'U': return COL_SCENE_LOFI_MUG;
        case 'u': return COL_SCENE_LOFI_MUG_SHADE;
        case 'N': return COL_SCENE_LOFI_MUG_RIM;
        case 'F': return COL_SCENE_LOFI_COFFEE;
        case 'V': return COL_SCENE_LOFI_MUG_SHINE;
        case 'Q': return COL_SCENE_LOFI_PAPER;
        case 'q': return COL_SCENE_LOFI_PAPER_SHADE;
        case 'X': return COL_SCENE_LOFI_PAGE_EDGE;
        case 'D': return COL_SCENE_LOFI_BOOK_COVER;
        case 'J': return COL_SCENE_LOFI_INK;
        default:  return girl_color(c);
    }
}

// How strongly the lamp lights scene pixel (x, y), 0-1, from light.png
static const float LIGHT_MAX = 0.6f;   // white in light.png tints this far toward the lamp's colour
static float light_at(int x, int y) {
    int c = x - LIGHT_X, r = y - props_dy - LIGHT_Y;
    if (!lamp_on || c < 0 || r < 0 || c >= LIGHT_W || r >= LIGHT_H) return 0;
    return LIGHT[r * LIGHT_W + c] / 255.0f;
}

// Draw a prop layer from lofi_art.h into the backdrop. `lit` scales how much
// the lamp's light warms it (the mug stands in it; the lamp is a silhouette);
// with the lamp off a lit prop sinks into the room's shadow instead.
static void draw_prop(const char *const *rows, int x0, int y0, int w, int h, float lit) {
    y0 += props_dy;
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++)
            if (rows[r][c] != '.') {
                int x = x0 + c, y = y0 + r;
                lv_color_t col = prop_color(rows[r][c]);
                if (lit > 0) {
                    // Sample the light just above the desk: whatever stands on the
                    // desk overlaps the pool painted on its surface, which is far
                    // brighter than the beam that actually reaches the object
                    int sy = y < desk_y - 3 ? y : desk_y - 3;
                    col = lamp_on ? pixfb_mix(col, COL_SCENE_LOFI_LAMP_GLOW, fminf(1.0f, light_at(x, sy) * lit))
                                  : pixfb_mix(col, COL_SCENE_LOFI_ROOM, 0.45f);
                }
                pixfb_px(fb, x, y, col);
            }
    pixfb_touch(fb, x0, y0, w, h);
}

// The lamp's light (light.png): tints everything under it toward the lamp's colour
static void draw_light() {
    for (int r = 0; r < LIGHT_H; r++)
        for (int c = 0; c < LIGHT_W; c++) {
            int x = LIGHT_X + c, y = LIGHT_Y + props_dy + r;
            float t = light_at(x, y);
            if (t <= 0 || x >= W || y >= H) continue;
            fb.buf[y * W + x] = pixfb_mix(fb.buf[y * W + x], COL_SCENE_LOFI_LAMP_GLOW, t * LIGHT_MAX);
        }
    pixfb_touch(fb, LIGHT_X, LIGHT_Y + props_dy, LIGHT_W, LIGHT_H);
}

// Her handwriting (book_ink.png, then book_ink2.png), one slot per typed
// character — worked out by tools/png_to_sprites.py: lines from our left to
// our right, each written from its lower end up, and every line taking the
// same number of characters even where some are out of view (x = -1 here).
static void plan_ink() {
    ink_slots = INK_N < MAX_INK ? INK_N : MAX_INK;
    for (int i = 0; i < ink_slots; i++) {
        bool empty = INK_ORDER[i][0] == INK_EMPTY;
        ink_x[i] = empty ? -1 : INK_ORDER[i][0];
        ink_y[i] = empty ? -1 : INK_ORDER[i][1] + props_dy;
    }
}

static void paint_backdrop() {
    pixfb_fill(fb, 0, 0, W, H, COL_SCENE_LOFI_ROOM);

    // Window: frame, sky gradient, moon, city
    pixfb_fill(fb, wx0 - 3, wy0 - 3, wx1 - wx0 + 6, wy1 - wy0 + 6, COL_SCENE_LOFI_FRAME);
    for (int y = wy0; y < wy1; y++)
        pixfb_fill(fb, wx0, y, wx1 - wx0, 1,
                   pixfb_mix(COL_SCENE_LOFI_SKY_TOP, COL_SCENE_LOFI_SKY_LOW, (float)(y - wy0) / (wy1 - wy0)));
    pixfb_fill_circle(fb, wx0 + (wx1 - wx0) / 5, wy0 + 9, 5, COL_SCENE_LOFI_MOON);
    draw_city();
    // Mullions: one vertical, one horizontal bar
    pixfb_fill(fb, (wx0 + wx1) / 2 - 1, wy0, 2, wy1 - wy0, COL_SCENE_LOFI_FRAME);
    pixfb_fill(fb, wx0, wy0 + (wy1 - wy0) * 2 / 5, wx1 - wx0, 2, COL_SCENE_LOFI_FRAME);
    pixfb_fill(fb, wx0 - 5, wy1 + 3, wx1 - wx0 + 10, 2, COL_SCENE_LOFI_FRAME);   // sill

    // Desk with the lamp's light over everything so far, then the paper, the
    // mug, and the lamp itself (its lit parts only while it's on)
    pixfb_fill(fb, 0, desk_y, W, 3, COL_SCENE_LOFI_DESK);
    pixfb_fill(fb, 0, desk_y + 3, W, H - desk_y - 3, COL_SCENE_LOFI_FIGURE);
    if (lamp_on) draw_light();
    draw_prop(PROP_BOOK, PROP_BOOK_X, PROP_BOOK_Y, PROP_BOOK_W, PROP_BOOK_H, 1.3f);
    draw_prop(PROP_MUG, PROP_MUG_X, PROP_MUG_Y, PROP_MUG_W, PROP_MUG_H, 1.5f);
    draw_prop(PROP_LAMP, PROP_LAMP_X, PROP_LAMP_Y, PROP_LAMP_W, PROP_LAMP_H, 0);
    if (lamp_on) draw_prop(PROP_LAMP_LIT, PROP_LAMP_LIT_X, PROP_LAMP_LIT_Y, PROP_LAMP_LIT_W, PROP_LAMP_LIT_H, 0);

    pixfb_bg_save(fb);
}

// ---------------------------------------------------------------------------

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_LOFI_ROOM);
    done = oom;
    if (oom) return;
    wx0 = w * 18 / 100; wx1 = w * 92 / 100;
    wy0 = h * 9 / 100;  wy1 = h * 62 / 100;
    desk_y = h * 78 / 100;
    // The girl is drawn for a 240x90 scene: keep her against the right edge, on the desk
    girl_dx = w - 240;
    girl_dy = desk_y - 70;
    hx = 142 + girl_dx; hyy = 26 + girl_dy;
    props_dy = desk_y - 70;
    plan_ink();
    mug_x = PROP_MUG_X; mug_top = PROP_MUG_Y + props_dy;
    lamp_on = true;
    paint_backdrop();
    if (!fb.bg) { done = oom = true; return; }

    for (int i = 0; i < MAX_DROPS; i++) drops[i].alive = false;
    for (int i = 0; i < MAX_NOTES; i++) notes[i].alive = false;
    ink = 0;
    stopping = false;
    nod = nod_phase = beat_phase = steam_t = write_t = note_acc = lit_acc = 0;
    flash_t = -1;
    since_beat = 10000;
}

static lv_color_t girl_color(char c) {
    switch (c) {
        case 'H': return COL_SCENE_LOFI_HAIR;
        case 'h': return COL_SCENE_LOFI_HAIR_SHADE;
        case 'T': return COL_SCENE_LOFI_HAIR_TIE;
        case 'S': return COL_SCENE_LOFI_SKIN;
        case 's': return COL_SCENE_LOFI_SKIN_SHADE;
        case 'E': return COL_SCENE_LOFI_EYE;
        case 'W': return COL_SCENE_LOFI_PHONES_BAND;
        case 'R': return COL_SCENE_LOFI_HEADPHONES;
        case 'K': return COL_SCENE_LOFI_PHONES_CUSHION;
        case 'G': return COL_SCENE_LOFI_SWEATER;
        case 'g': return COL_SCENE_LOFI_SWEATER_SHADE;
        case 'C': return COL_SCENE_LOFI_SCARF;
        case 'c': return COL_SCENE_LOFI_SCARF_SHADE;
        case 'O': return COL_SCENE_LOFI_OUTLINE;
        case 'L': return COL_SCENE_LOFI_HAIR_SHINE;
        case 'M': return COL_SCENE_LOFI_MOUTH;
        case 'B': return COL_SCENE_LOFI_BLUSH;
        case 'I': return COL_SCENE_LOFI_IRIS;
        default:  return COL_SCENE_LOFI_PEN;
    }
}

// Draw one layer of the pixel-art girl (lofi_girl.h), shifted by (dx, dy)
static void draw_art(const char *const *rows, int x0, int y0, int w, int h, int dx, int dy) {
    x0 += girl_dx + dx;
    y0 += girl_dy + dy;
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++)
            if (rows[r][c] != '.') pixfb_px(fb, x0 + c, y0 + r, girl_color(rows[r][c]));
    pixfb_touch(fb, x0, y0, w, h);
}

// The girl, facing left toward her notebook: `bob` nods the head down,
// `hand_dx` moves the writing hand.
static void draw_girl(int bob, int hand_dx) {
    draw_art(GIRL_BODY, GIRL_BODY_X, GIRL_BODY_Y, GIRL_BODY_W, GIRL_BODY_H, 0, 0);
    draw_art(GIRL_FIST, GIRL_FIST_X, GIRL_FIST_Y, GIRL_FIST_W, GIRL_FIST_H, 0, 0);   // behind her head
    draw_art(GIRL_HEAD, GIRL_HEAD_X, GIRL_HEAD_Y, GIRL_HEAD_W, GIRL_HEAD_H, 0, bob);
    draw_art(GIRL_HAND, GIRL_HAND_X, GIRL_HAND_Y, GIRL_HAND_W, GIRL_HAND_H, hand_dx, 0);
}

// Start a lightning strike: a bolt zigzagging from the top of the window down
// to a rooftop, with a short fork off it
static void strike() {
    flash_t = 0;
    // Strike either side of her, never hidden behind her head
    int head_l = GIRL_HEAD_X + girl_dx - 6, head_r = GIRL_HEAD_X + girl_dx + GIRL_HEAD_W + 6;
    int x = random(0, 2) && head_l - 10 > wx0 + 10 ? random(wx0 + 10, head_l - 10)
                                                    : random(LV_MIN(head_r, wx1 - 20), wx1 - 10);
    int bottom = sky_top[x < 320 ? x : 319];
    for (int i = 0; i < BOLT_POINTS; i++) {
        bolt_y[i] = wy0 + (bottom - wy0) * i / (BOLT_POINTS - 1);
        bolt_x[i] = i == 0 ? x : bolt_x[i - 1] + random(-4, 5);
        bolt_x[i] = constrain(bolt_x[i], LV_MAX(wx0 + 3, PROP_LAMP_X + PROP_LAMP_W + 3), wx1 - 4);   // never in front of the lamp
    }
    int k = random(2, 5);
    fork_x[0] = bolt_x[k]; fork_y[0] = bolt_y[k];
    for (int i = 1; i < 4; i++) {
        fork_x[i] = constrain(fork_x[i - 1] + (random(0, 2) ? 3 : -3) + random(-1, 2),
                              LV_MAX(wx0 + 3, PROP_LAMP_X + PROP_LAMP_W + 3), wx1 - 4);
        fork_y[i] = fork_y[i - 1] + random(2, 5);
    }
}

// True where the lamp (or its lit parts) covers the window: the flash is
// behind it, so those pixels stay as they are
static bool lamp_covers(int x, int y) {
    int c = x - PROP_LAMP_X, r = y - props_dy - PROP_LAMP_Y;
    if (c >= 0 && r >= 0 && c < PROP_LAMP_W && r < PROP_LAMP_H && PROP_LAMP[r][c] != '.') return true;
    c = x - PROP_LAMP_LIT_X; r = y - props_dy - PROP_LAMP_LIT_Y;
    return lamp_on && c >= 0 && r >= 0 && c < PROP_LAMP_LIT_W && r < PROP_LAMP_LIT_H && PROP_LAMP_LIT[r][c] != '.';
}

// The flash: the sky above the rooftops flares twice — bright, then softer —
// with the city staying black against it, and the bolt showing at its brightest
static void draw_lightning(float dt) {
    flash_t += dt;
    float t = flash_t, f;
    if (t < 0.06f)      f = 1.0f;
    else if (t < 0.12f) f = 0.25f;
    else if (t < 0.20f) f = 0.75f;
    else                f = 0.75f * expf(-(t - 0.20f) * 8);
    pixfb_bg_restore(fb, wx0, wy0, wx1 - wx0, wy1 - wy0);
    if (t > 0.7f) { flash_t = -1; return; }
    int mx = (wx0 + wx1) / 2 - 1, my = wy0 + (wy1 - wy0) * 2 / 5;
    for (int y = wy0; y < wy1; y++)
        for (int x = wx0; x < wx1; x++) {
            if (y >= sky_top[x < 320 ? x : 319] || x == mx || x == mx + 1 || y == my || y == my + 1 ||
                lamp_covers(x, y)) continue;
            fb.buf[y * W + x] = pixfb_mix(fb.buf[y * W + x], COL_SCENE_LOFI_MOON, f * 0.5f);
        }
    if (t < 0.25f) {   // the bolt, with a soft glow either side so it stands out from the lit sky
        lv_color_t glow = pixfb_mix(COL_SCENE_LOFI_SKY_LOW, COL_SCENE_LOFI_MOON, 0.75f);
        for (int side = -1; side <= 1; side += 2) {
            for (int i = 0; i + 1 < BOLT_POINTS; i++)
                pixfb_line(fb, bolt_x[i] + side, bolt_y[i], bolt_x[i + 1] + side, bolt_y[i + 1], glow);
        }
        for (int i = 0; i + 1 < BOLT_POINTS; i++)
            pixfb_line(fb, bolt_x[i], bolt_y[i], bolt_x[i + 1], bolt_y[i + 1], COL_SCENE_LOFI_MOON);
        for (int i = 0; i + 1 < 4; i++)
            pixfb_line(fb, fork_x[i], fork_y[i], fork_x[i + 1], fork_y[i + 1], COL_SCENE_LOFI_MOON);
    }
}

static void spawn_drop() {
    for (int i = 0; i < MAX_DROPS; i++) {
        Drop &d = drops[i];
        if (d.alive) continue;
        d = { true, scene_randf(wx0, wx1), (float)wy0 - scene_randf(0, 10), scene_randf(40, 70), (int)random(2, 5) };
        return;
    }
}

// Seconds per nod for a song at `bpm`: halved or doubled into a pace a
// person would actually bob at (half-time on a fast track)
static float nod_period(float bpm) {
    while (bpm > NOD_MAX_BPM) bpm /= 2;
    while (bpm < NOD_MIN_BPM) bpm *= 2;
    return 60.0f / bpm;
}

static void frame(uint32_t dt_ms) {
    if (oom || done) return;
    float dt = dt_ms / 1000.0f;
    const SceneContext &ctx = scene_ctx();
    bool music = ctx.music == SCENE_MUSIC_PLAYING;
    since_beat += dt_ms;

    // City windows switch on and off now and then (into the backdrop too)
    lit_acc += dt;
    if (lit_acc > 1.5f && n_lit > 0) {
        lit_acc = 0;
        Lit &l = lit[random(0, n_lit)];
        l.on = !l.on;
        pixfb_bg_restore(fb, l.x, l.y, 2, 2);   // put back sky/building under it...
        lv_color_t col = l.on ? COL_SCENE_LOFI_CITY_LIGHT : COL_SCENE_LOFI_CITY;
        pixfb_fill(fb, l.x, l.y, 2, 2, col);
        for (int y = l.y; y < l.y + 2; y++) for (int x = l.x; x < l.x + 2; x++) fb.bg[y * W + x] = col;
    }

    // Erase everything that moves
    int fig_x0 = GIRL_HAND_X + girl_dx - 3, fig_y0 = GIRL_HEAD_Y + girl_dy - 1;
    {   // both pages, for the ink
        int x0 = LV_MIN(BOOK_INK_X, BOOK_INK2_X), y0 = LV_MIN(BOOK_INK_Y, BOOK_INK2_Y);
        int x1 = LV_MAX(BOOK_INK_X + BOOK_INK_W, BOOK_INK2_X + BOOK_INK2_W);
        int y1 = LV_MAX(BOOK_INK_Y + BOOK_INK_H, BOOK_INK2_Y + BOOK_INK2_H);
        pixfb_bg_restore(fb, x0, y0 + props_dy, x1 - x0, y1 - y0);
    }
    pixfb_bg_restore(fb, fig_x0, fig_y0, W - fig_x0, desk_y + 2 - fig_y0);
    for (int i = 0; i < MAX_DROPS; i++)
        if (drops[i].alive) pixfb_bg_restore(fb, (int)drops[i].x - 2, (int)drops[i].y, 4, drops[i].len + 1);
    for (int i = 0; i < MAX_NOTES; i++)
        if (notes[i].alive) pixfb_bg_restore(fb, (int)notes[i].x, (int)notes[i].y, 5, 7);
    pixfb_bg_restore(fb, mug_x - 1, mug_top - 19, 16, 19);
    if (flash_t >= 0) draw_lightning(dt);

    // Rain on the glass, slanting a little
    if (!stopping && random(0, 100) < 45) spawn_drop();
    for (int i = 0; i < MAX_DROPS; i++) {
        Drop &d = drops[i];
        if (!d.alive) continue;
        d.y += d.v * dt;
        d.x -= d.v * 0.15f * dt;
        if (d.y > wy1 || d.x < wx0) { d.alive = false; continue; }
        int x = (int)d.x, y = (int)d.y;
        pixfb_line(fb, x, y, x - 1, y + d.len, COL_SCENE_LOFI_RAIN);
    }

    // Nodding: with a known tempo she keeps her own time (beats only steer
    // it, see event()); before that, strong beats nod her directly, or with
    // no beats at all she sways at a relaxed pace
    float bob = 0;
    if (music) {
        if (ctx.bpm) {
            beat_phase += dt / nod_period(ctx.bpm);
            if (beat_phase >= 1) { beat_phase -= floorf(beat_phase); nod = 1.0f; }
            bob = nod;
        } else if (since_beat < 2000) bob = nod;
        else { nod_phase += dt * 6.28f * NOD_FALLBACK_BPM / 60; bob = sinf(nod_phase) > 0.6f ? 1 : 0; }
    }
    nod *= expf(-8.0f * dt);

    // Writing: the hand moves while you type
    int hand_dx = 0;
    if (ctx.typing) { write_t += dt * 14; hand_dx = (int)(2 * sinf(write_t)); }
    draw_girl((int)(bob + 0.5f), hand_dx);

    // Ink: one pixel per character typed, filling the page's lines
    for (int i = 0; i < ink && i < ink_slots; i++)   // lit by the lamp like the page under it
        if (ink_x[i] >= 0) pixfb_fill(fb, ink_x[i], ink_y[i], 1, 1,
                   lamp_on ? pixfb_mix(COL_SCENE_LOFI_INK, COL_SCENE_LOFI_LAMP_GLOW,
                                       fminf(1.0f, light_at(ink_x[i], LV_MIN(ink_y[i], desk_y - 3)) * 0.6f))
                           : COL_SCENE_LOFI_INK);

    // Steam from the mug
    steam_t += dt;
    for (int k = 0; k < 3; k++) {
        float t = fmodf(steam_t * 0.8f + k * 0.33f, 1.0f);
        int sx = mug_x + 3 + k * 2 + (int)(2 * sinf(t * 9 + k)), sy = mug_top - 2 - (int)(t * 16);
        pixfb_fill(fb, sx, sy, 1, 2, pixfb_mix(COL_SCENE_LOFI_STEAM, COL_SCENE_LOFI_ROOM, t));
    }

    // Music notes floating up from the headphones
    if (music && !stopping) {
        note_acc += dt;
        if (note_acc > 2.2f) {
            note_acc = 0;
            for (int i = 0; i < MAX_NOTES; i++) if (!notes[i].alive) {
                notes[i] = { true, (float)hx + 22, (float)hyy + 2, 0 };   // just behind her head
                break;
            }
        }
    }
    bool any_note = false;
    for (int i = 0; i < MAX_NOTES; i++) {
        Note &n = notes[i];
        if (!n.alive) continue;
        n.t += dt;
        n.y -= 7 * dt;
        n.x += 3 * sinf(n.t * 2.5f) * dt * 3;
        if (n.y < 0 || n.t > 4.5f) { n.alive = false; continue; }
        any_note = true;
        pixfb_sprite(fb, (int)n.x, (int)n.y, NOTE_SPRITE, 5, 7,
                     pixfb_mix(COL_SCENE_LOFI_NOTE, COL_SCENE_LOFI_ROOM, n.t / 4.5f));
    }

    if (stopping) {
        bool any_drop = false;
        for (int i = 0; i < MAX_DROPS; i++) any_drop |= drops[i].alive;
        done = !any_drop && !any_note && flash_t < 0;
    }
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
    // The whole strip from the left edge to just past the lamp, at any height:
    // the touchscreen isn't precise enough for a thin lamp arm to be a target
    bool on_lamp = x < PROP_LAMP_X + PROP_LAMP_W + 10;
    if (!on_lamp && x >= wx0 && x < wx1 && y >= wy0 && y < wy1) { strike(); return; }
    if (on_lamp) {
        lamp_on = !lamp_on;
        paint_backdrop();   // the glow is part of the backdrop
    }
}

static void event(const SceneEvent &e) {
    if (e.type == SCENE_EV_BEAT) {
        const SceneContext &ctx = scene_ctx();
        if (ctx.bpm) {
            // Only beats near where she expects one steer her, stronger ones
            // more: off-beats and hi-hats between nods are ignored
            float err = beat_phase < 0.5f ? beat_phase : beat_phase - 1;   // + late, - early
            if (fabsf(err) < NOD_LOCK_WINDOW) beat_phase -= err * NOD_LOCK_GAIN * e.strength / 100.0f;
        } else if (e.strength >= NOD_RAW_MIN_S && since_beat >= NOD_RAW_GAP_MS) {
            nod = 1.0f;
            since_beat = 0;
        }
    }
    if (e.type == SCENE_EV_KEY && e.key == SCENE_KEY_CHAR) {
        if (++ink > ink_slots) ink = 0;   // both pages full: turn the page
    }
}

static void finish() { pixfb_free(fb); }

const Scene scene_lofi = { "lofi", start, tick, request_stop, is_done, touch, finish, event };
SCENE_REGISTER(scene_lofi, "Lofi girl");
