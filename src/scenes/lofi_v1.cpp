#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Lofi girl, first version (kept as a backup of the silhouette-shapes
// character; the current one is lofi.cpp) — a silhouetted girl with headphones writing at a desk in front of
// a big window: rain runs down the glass, the city outside has windows that
// light up and go dark, a desk lamp glows and a mug steams. The room is painted
// once as the backdrop; rain, the girl, steam, notes and ink are redrawn over it.
// When asked to stop, the rain eases off and the last notes drift away.
// Touch: tap the window for a flash of lightning, tap the lamp to switch it.
// Events: while music plays she nods to the beat and notes float up from her
// headphones; every character you type is ink in her notebook, and she turns
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
static int   hx, hyy;                 // head centre
static int   lamp_x, mug_x;
static int   note_x0, note_x1, note_y0, note_lines;   // notebook writing area
static Drop  drops[MAX_DROPS];
static Note  notes[MAX_NOTES];
static Lit   lit[MAX_LIT];
static int   n_lit;
static int   ink;                     // characters written on this page
static bool  lamp_on, stopping, done, oom;
static float nod, nod_phase, flash, steam_t, write_t, note_acc, lit_acc;
static uint32_t since_beat;

static const uint16_t NOTE_SPRITE[7] = {   // a quaver, 5×7
    0b00110, 0b00101, 0b00100, 0b00100, 0b11100, 0b11100, 0b01000,
};

// ---------------------------------------------------------------------------
// Backdrop

static void draw_city() {
    // Skyline across the bottom of the window, with a grid of little windows
    randomSeed(W * 13 + H);
    n_lit = 0;
    int x = wx0;
    while (x < wx1) {
        int bw = random(10, 24), bh = random((wy1 - wy0) / 5, (wy1 - wy0) * 3 / 5);
        int top = wy1 - bh;
        pixfb_fill(fb, x, top, LV_MIN(bw, wx1 - x), bh, COL_SCENE_LOFI_CITY);
        for (int yy = top + 3; yy < wy1 - 2; yy += 4)
            for (int xx = x + 2; xx < x + bw - 2 && xx < wx1 - 1; xx += 4)
                if (n_lit < MAX_LIT && random(0, 100) < 35) lit[n_lit++] = { xx, yy, random(0, 100) < 60 };
        x += bw + random(0, 3);
    }
    randomSeed(esp_random());
    for (int i = 0; i < n_lit; i++)
        if (lit[i].on) pixfb_fill(fb, lit[i].x, lit[i].y, 2, 2, COL_SCENE_LOFI_CITY_LIGHT);
}

static void paint_backdrop() {
    pixfb_fill(fb, 0, 0, W, H, COL_SCENE_LOFI_ROOM);

    // Lamp glow on the wall and desk, when it's on
    if (lamp_on) {
        for (int r = 30; r > 0; r -= 3)
            pixfb_fill_circle(fb, lamp_x, desk_y - 8, r,
                              pixfb_mix(COL_SCENE_LOFI_ROOM, COL_SCENE_LOFI_LAMP_GLOW, 0.5f * (1.0f - r / 30.0f)));
    }

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

    // Desk, lamp, mug, notebook
    pixfb_fill(fb, 0, desk_y, W, 3, COL_SCENE_LOFI_DESK);
    pixfb_fill(fb, 0, desk_y + 3, W, H - desk_y - 3, COL_SCENE_LOFI_FIGURE);
    pixfb_fill(fb, lamp_x - 5, desk_y - 2, 10, 2, COL_SCENE_LOFI_LAMP);                    // base
    pixfb_line(fb, lamp_x, desk_y - 2, lamp_x + 6, desk_y - 14, COL_SCENE_LOFI_LAMP);     // arm
    pixfb_fill_tri(fb, lamp_x + 1, desk_y - 13, lamp_x + 12, desk_y - 19, lamp_x + 11, desk_y - 10,
                   COL_SCENE_LOFI_LAMP);                                                    // shade
    if (lamp_on) pixfb_fill(fb, lamp_x + 5, desk_y - 11, 3, 2, COL_SCENE_LOFI_LAMP_GLOW);
    pixfb_fill(fb, mug_x, desk_y - 7, 6, 7, COL_SCENE_LOFI_MUG);
    pixfb_fill(fb, mug_x + 6, desk_y - 5, 2, 3, COL_SCENE_LOFI_MUG);
    pixfb_fill(fb, note_x0 - 2, desk_y - 2, note_x1 - note_x0 + 4, 2, COL_SCENE_LOFI_PAPER);

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
    hx = w * 62 / 100; hyy = desk_y - 30;
    lamp_x = w * 8 / 100;
    mug_x  = w * 84 / 100;
    note_x0 = hx - 34; note_x1 = hx - 10; note_y0 = desk_y - 1; note_lines = 1;
    lamp_on = true;
    paint_backdrop();
    if (!fb.bg) { done = oom = true; return; }

    for (int i = 0; i < MAX_DROPS; i++) drops[i].alive = false;
    for (int i = 0; i < MAX_NOTES; i++) notes[i].alive = false;
    ink = 0;
    stopping = false;
    nod = nod_phase = flash = steam_t = write_t = note_acc = lit_acc = 0;
    since_beat = 10000;
}

// The girl, facing left toward her notebook. `bob` nods the head down.
static void draw_girl(int bob, int hand_dx) {
    lv_color_t c = COL_SCENE_LOFI_FIGURE;
    int hy2 = hyy + bob;
    // Torso: shoulders down to the desk
    pixfb_fill_tri(fb, hx - 9, desk_y, hx + 14, desk_y, hx + 9, hyy + 9, c);
    pixfb_fill_tri(fb, hx - 9, desk_y, hx + 9, hyy + 9, hx - 5, hyy + 10, c);
    pixfb_fill(fb, hx - 1, hyy + 5, 5, 6, c);                       // neck
    // Arm reaching forward to the notebook
    int hand_x = note_x1 - 4 + hand_dx, hand_y = desk_y - 3;
    pixfb_fill_tri(fb, hx - 4, hyy + 12, hx + 2, hyy + 14, hand_x, hand_y, c);
    pixfb_fill_tri(fb, hx + 2, hyy + 14, hand_x + 3, hand_y + 1, hand_x, hand_y, c);
    pixfb_fill_circle(fb, hand_x, hand_y, 2, c);
    pixfb_line(fb, hand_x - 1, hand_y, hand_x - 4, hand_y + 2, c);   // pen
    // Head, messy bun, headphones
    pixfb_fill_circle(fb, hx, hy2, 7, c);
    pixfb_fill_circle(fb, hx + 6, hy2 - 5, 3, c);
    pixfb_fill_circle(fb, hx - 6, hy2 + 1, 1, c);                    // nose, looking down-left
    for (int a = 200; a <= 340; a += 10) {                           // band over the head
        float r = a * 0.01745f;
        pixfb_fill(fb, hx + (int)(9 * cosf(r)), hy2 + (int)(9 * sinf(r)), 1, 1, COL_SCENE_LOFI_HEADPHONES);
    }
    pixfb_fill_circle(fb, hx + 2, hy2 + 1, 3, COL_SCENE_LOFI_HEADPHONES);   // ear cup
}

static void spawn_drop() {
    for (int i = 0; i < MAX_DROPS; i++) {
        Drop &d = drops[i];
        if (d.alive) continue;
        d = { true, scene_randf(wx0, wx1), (float)wy0 - scene_randf(0, 10), scene_randf(40, 70), (int)random(2, 5) };
        return;
    }
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
    int fig_x0 = note_x0 - 6, fig_y0 = hyy - 12;
    pixfb_bg_restore(fb, fig_x0, fig_y0, hx + 20 - fig_x0, desk_y - fig_y0);
    for (int i = 0; i < MAX_DROPS; i++)
        if (drops[i].alive) pixfb_bg_restore(fb, (int)drops[i].x - 2, (int)drops[i].y, 4, drops[i].len + 1);
    for (int i = 0; i < MAX_NOTES; i++)
        if (notes[i].alive) pixfb_bg_restore(fb, (int)notes[i].x, (int)notes[i].y, 5, 7);
    pixfb_bg_restore(fb, mug_x - 1, desk_y - 22, 9, 15);
    if (flash > 0) {
        flash -= dt * 4;
        if (flash <= 0) pixfb_bg_restore(fb, wx0, wy0, wx1 - wx0, wy1 - wy0);
        else pixfb_fill(fb, wx0, wy0, wx1 - wx0, (wy1 - wy0) * 2 / 5,
                        pixfb_mix(COL_SCENE_LOFI_SKY_TOP, COL_SCENE_LOFI_MOON, flash * 0.6f));
    }

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

    // Nodding: kicked by beats, or a gentle sway if the tempo is unknown
    float bob = 0;
    if (music) {
        if (since_beat < 2000) bob = nod;
        else { nod_phase += dt * 6.28f * 80 / 60; bob = sinf(nod_phase) > 0.6f ? 1 : 0; }
    }
    nod *= expf(-8.0f * dt);

    // Writing: the hand moves while you type
    int hand_dx = 0;
    if (ctx.typing) { write_t += dt * 14; hand_dx = (int)(2 * sinf(write_t)); }
    draw_girl((int)(bob + 0.5f), hand_dx);

    // Ink: one pixel per character typed, filling the page's lines
    int line_len = note_x1 - note_x0;
    for (int i = 0; i < ink && i < line_len; i++)
        if ((i * 7) % 5 != 0) pixfb_fill(fb, note_x0 + i, note_y0 - 1, 1, 1, COL_SCENE_LOFI_INK);

    // Steam from the mug
    steam_t += dt;
    for (int k = 0; k < 3; k++) {
        float t = fmodf(steam_t * 0.8f + k * 0.33f, 1.0f);
        int sx = mug_x + 2 + (int)(2 * sinf(t * 9 + k)), sy = desk_y - 8 - (int)(t * 13);
        pixfb_fill(fb, sx, sy, 1, 2, pixfb_mix(COL_SCENE_LOFI_STEAM, COL_SCENE_LOFI_ROOM, t));
    }

    // Music notes floating up from the headphones
    if (music && !stopping) {
        note_acc += dt;
        if (note_acc > 2.2f) {
            note_acc = 0;
            for (int i = 0; i < MAX_NOTES; i++) if (!notes[i].alive) {
                notes[i] = { true, (float)hx + 6, (float)hyy - 4, 0 };
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
        done = !any_drop && !any_note && flash <= 0;
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
    if (x >= wx0 && x < wx1 && y >= wy0 && y < wy1) { flash = 1.0f; return; }
    if (abs(x - lamp_x - 5) < 16 && y > desk_y - 30) {
        lamp_on = !lamp_on;
        paint_backdrop();   // the glow is part of the backdrop
    }
}

static void event(const SceneEvent &e) {
    if (e.type == SCENE_EV_BEAT) { nod = 1.0f; since_beat = 0; }
    if (e.type == SCENE_EV_KEY && e.key == SCENE_KEY_CHAR) {
        if (++ink > note_x1 - note_x0) {
            ink = 0;   // page full: turn it
            pixfb_bg_restore(fb, note_x0, note_y0 - 1, note_x1 - note_x0, 1);
        }
    }
}

static void finish() { pixfb_free(fb); }

const Scene scene_lofi_v1 = { "lofi_v1", start, tick, request_stop, is_done, touch, finish, event };
