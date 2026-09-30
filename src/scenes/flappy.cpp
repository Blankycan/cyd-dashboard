#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Flappy Bird — a little bird flapping through gaps between pipes over a
// skyline, the score counting up at the top. It plays itself (and now and
// then clips a pipe, tumbles down, and starts over). When asked to stop no
// more pipes come; once the last one is past, the bird flies up and away.
// Touch: tap to flap (or to start over after a crash). The computer takes
// back over AUTO_RESUME_MS after your last touch.
// Events: Space on the host keyboard flaps too.

static const uint32_t AUTO_RESUME_MS = 5000;
static const int      MAX_PIPES = 5, PIPE_W = 18, CAP_H = 4, GAP = 38, SPACING = 78, GROUND_H = 8;
static const float    GRAVITY = 430, FLAP_V = -120, SCROLL_V = 52;

// '1' outline, '2' body, '3' eye white, '4' beak, '5' wing
static const char *const BIRD[3][11] = {
    {   ".....1111111...",   // wings up
        "...11222221331.",
        "..1222222213331",
        ".11112222213313",
        "1555512222213331",
        "1555551222211111",
        ".1555512221444441",
        "..11112222144441.",
        "...122222221111..",
        "....11122221.....",
        ".......1111......" },
    {   ".....1111111...",   // level
        "...11222221331.",
        "..1222222213331",
        ".12222222213313",
        ".11111222213331",
        "1555551222211111",
        "15555512221444441",
        ".1111122222144441",
        "...122222221111..",
        "....11122221.....",
        ".......1111......" },
    {   ".....1111111...",   // wings down
        "...11222221331.",
        "..1222222213331",
        ".12222222213313",
        ".12222222213331",
        ".11111222211111",
        "15555512221444441",
        "15555512222144441",
        "1555512222221111.",
        ".11111222221.....",
        ".......1111......" },
};

struct Pipe { bool alive, scored, misjudge; float x; int gap_y; };   // gap_y: top of the gap

static PixFb fb;
static int   W, H, play_h, bird_x, score;
static Pipe  pipes[MAX_PIPES];
static float bird_y, bird_vy, scroll, flap_t, dead_t, next_pipe, exit_x, bob_t;
static bool  dead, waiting, stopping, done, oom;
static uint32_t since_touch;

static uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

static void paint_backdrop() {
    // Sky, a few clouds, and a skyline of rounded bushes and buildings along the bottom
    for (int y = 0; y < play_h; y++)
        pixfb_fill(fb, 0, y, W, 1, pixfb_mix(COL_SCENE_FLAPPY_SKY_TOP, COL_SCENE_FLAPPY_SKY_LOW, (float)y / play_h));
    for (int i = 0; i < 4; i++) {
        uint32_t r = hash32(i * 131 + 5);
        int cx = r % W, cy = play_h - 26 - (r >> 10) % 8;
        for (int k = 0; k < 4; k++) pixfb_fill_circle(fb, cx + k * 7, cy - (k % 2) * 3, 6, COL_SCENE_FLAPPY_CLOUD);
    }
    for (int x = 0; x < W; x += 9) {
        uint32_t r = hash32(x * 7 + 1);
        int bh = 8 + r % 14, bw = 7 + (r >> 8) % 5;
        pixfb_fill(fb, x, play_h - bh, bw, bh, COL_SCENE_FLAPPY_CITY);
        for (int wy = play_h - bh + 2; wy < play_h - 2; wy += 3)
            for (int wx = x + 1; wx < x + bw - 1; wx += 3)
                if (hash32(wx * 17 + wy) % 3 == 0) pixfb_px(fb, wx, wy, COL_SCENE_FLAPPY_SKY_LOW);
    }
    for (int x = -4; x < W; x += 10) pixfb_fill_circle(fb, x, play_h - 2, 5 + hash32(x + 99) % 3, COL_SCENE_FLAPPY_BUSH);
    pixfb_touch(fb, 0, 0, W, H);
    pixfb_bg_save(fb);
}

static void reset_run() {
    for (int i = 0; i < MAX_PIPES; i++) pipes[i].alive = false;
    bird_y = play_h * 0.4f;
    bird_vy = 0;
    score = 0;
    dead = false;
    waiting = true;   // bobs in place for a moment before the pipes start
    bob_t = 0;
    next_pipe = W * 0.5f;
}

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_FLAPPY_SKY_TOP);
    done = oom;
    if (oom) return;
    play_h = h - GROUND_H;
    bird_x = w / 4;
    paint_backdrop();
    if (!fb.bg) { done = oom = true; return; }
    reset_run();
    scroll = flap_t = dead_t = 0;
    exit_x = bird_x;
    stopping = false;
    since_touch = AUTO_RESUME_MS;
}

static void flap() {
    if (dead) return;
    waiting = false;
    bird_vy = FLAP_V;
    flap_t = 0;
}

static void spawn_pipe() {
    for (int i = 0; i < MAX_PIPES; i++)
        if (!pipes[i].alive) {
            pipes[i] = { true, false, random(0, 100) < 3, (float)W + 2, (int)scene_randf(8, play_h - GAP - 8) };
            return;
        }
}

static void autopilot() {
    // Flap each time it sinks to near the bottom of the next gap (a flap lifts
    // it about 17 px, well within the gap)
    const Pipe *next = nullptr;
    for (int i = 0; i < MAX_PIPES; i++)
        if (pipes[i].alive && pipes[i].x + PIPE_W > bird_x - 6 && (!next || pipes[i].x < next->x)) next = &pipes[i];
    float target = next ? next->gap_y + GAP - 16 : play_h * 0.45f;
    if (next && next->misjudge) target += 14;   // a lapse: it lets itself sink too low
    if (bird_y > target && bird_vy >= 0) flap();
}

static void update(float dt) {
    since_touch += (uint32_t)(dt * 1000);
    flap_t += dt;
    if (dead) {
        dead_t += dt;
        bird_vy += GRAVITY * dt;
        bird_y = fminf(play_h - 6, bird_y + bird_vy * dt);   // tumbles to the ground
        if (stopping && dead_t > 1.0f) done = true;
        else if (dead_t > 1.8f && since_touch >= AUTO_RESUME_MS) reset_run();
        return;
    }
    if (waiting) {   // hovering at the start, bobbing up and down
        bob_t += dt;
        bird_y = play_h * 0.4f + sinf(bob_t * 6) * 2;
        if (flap_t > 0.3f) flap_t = 0;
        if (bob_t > 1.2f && (since_touch >= AUTO_RESUME_MS || stopping)) waiting = false;
        if (stopping && bob_t > 0.2f) waiting = false;
        scroll += SCROLL_V * dt;
        return;
    }

    float move = SCROLL_V * dt;
    scroll += move;
    for (int i = 0; i < MAX_PIPES; i++) {
        Pipe &p = pipes[i];
        if (!p.alive) continue;
        p.x -= move;
        if (p.x + PIPE_W < -2) p.alive = false;
        if (!p.scored && p.x + PIPE_W < bird_x) { p.scored = true; score++; }
    }
    if (!stopping && (next_pipe -= move) <= 0) { spawn_pipe(); next_pipe = SPACING; }

    bool any_pipe = false;
    for (int i = 0; i < MAX_PIPES; i++) any_pipe |= pipes[i].alive && pipes[i].x + PIPE_W > exit_x - 8;
    if (stopping && !any_pipe) {   // the way is clear: up and away
        exit_x += 70 * dt;
        if (bird_y > play_h * 0.3f && bird_vy > -20) flap();
        if (exit_x > W + 4 || bird_y < -12) done = true;
    } else if (since_touch >= AUTO_RESUME_MS || stopping) autopilot();

    bird_vy += GRAVITY * dt;
    bird_y += bird_vy * dt;
    if (bird_y < -20) { bird_y = -20; bird_vy = 0; }

    // Crashes: the ground, or a pipe (a slightly forgiving box)
    int bx0 = (int)exit_x + 2, bx1 = (int)exit_x + 14, by0 = (int)bird_y + 2, by1 = (int)bird_y + 9;
    bool hit = by1 >= play_h;
    for (int i = 0; i < MAX_PIPES && !hit; i++) {
        const Pipe &p = pipes[i];
        if (!p.alive || bx1 <= p.x || bx0 >= p.x + PIPE_W) continue;
        hit = by0 < p.gap_y || by1 > p.gap_y + GAP;
    }
    if (hit) { dead = true; dead_t = 0; bird_vy = fmaxf(bird_vy, 0); }
}

static void draw_pipe_part(int x, int y, int w, int h) {
    // Shaded like a tube: highlight on the left, shadow on the right, dark edges
    if (h <= 0) return;
    pixfb_fill(fb, x, y, w, h, COL_SCENE_FLAPPY_PIPE);
    pixfb_fill(fb, x + 2, y, 2, h, COL_SCENE_FLAPPY_PIPE_LIGHT);
    pixfb_fill(fb, x + w - 4, y, 2, h, COL_SCENE_FLAPPY_PIPE_DARK);
    pixfb_fill(fb, x, y, 1, h, COL_SCENE_FLAPPY_OUTLINE);
    pixfb_fill(fb, x + w - 1, y, 1, h, COL_SCENE_FLAPPY_OUTLINE);
}

static void draw_score() {
    lv_color_t outline = COL_SCENE_FLAPPY_OUTLINE;
    pixfb_number(fb, W / 2 - pixfb_number_width(score) / 2, 4, score, COL_SCENE_FLAPPY_SCORE, 0, &outline);
}

static void draw() {
    pixfb_bg_restore(fb, 0, 0, W, H);
    for (int i = 0; i < MAX_PIPES; i++) {
        const Pipe &p = pipes[i];
        if (!p.alive) continue;
        int x = (int)p.x;
        draw_pipe_part(x, 0, PIPE_W, p.gap_y - CAP_H);
        draw_pipe_part(x - 1, p.gap_y - CAP_H, PIPE_W + 2, CAP_H);
        pixfb_fill(fb, x - 1, p.gap_y - 1, PIPE_W + 2, 1, COL_SCENE_FLAPPY_OUTLINE);
        draw_pipe_part(x - 1, p.gap_y + GAP, PIPE_W + 2, CAP_H);
        pixfb_fill(fb, x - 1, p.gap_y + GAP, PIPE_W + 2, 1, COL_SCENE_FLAPPY_OUTLINE);
        draw_pipe_part(x, p.gap_y + GAP + CAP_H, PIPE_W, play_h - (p.gap_y + GAP + CAP_H));
    }

    // Ground: a strip with stripes scrolling by
    pixfb_fill(fb, 0, play_h, W, 1, COL_SCENE_FLAPPY_OUTLINE);
    pixfb_fill(fb, 0, play_h + 1, W, 2, COL_SCENE_FLAPPY_GRASS);
    pixfb_fill(fb, 0, play_h + 3, W, H - play_h - 3, COL_SCENE_FLAPPY_GROUND);
    int off = (int)scroll % 8;
    for (int x = -off; x < W; x += 8)
        for (int k = 0; k < 2; k++) pixfb_fill(fb, x + k + 1, play_h + 1, 3, 1 + k, COL_SCENE_FLAPPY_GRASS_DARK);

    lv_color_t pal[] = { COL_SCENE_FLAPPY_OUTLINE, COL_SCENE_FLAPPY_BIRD, COL_SCENE_FLAPPY_EYE,
                         COL_SCENE_FLAPPY_BEAK, COL_SCENE_FLAPPY_WING };
    int frame = dead ? 1 : waiting ? (int)(bob_t * 8) % 3 : flap_t < 0.25f ? (int)(flap_t * 12) % 3 : 1;
    pixfb_art(fb, (int)exit_x, (int)bird_y, BIRD[frame], 11, pal);
    if (!stopping || score) draw_score();
}

static void tick(uint32_t dt_ms) {
    if (oom || done) return;
    update(dt_ms / 1000.0f);
    if (done) return;
    draw();
    pixfb_flush(fb);
}

static void request_stop() { stopping = true; }
static bool is_done()      { return done; }

static void poke() {   // a tap or Space: flap, or start over after a crash
    if (dead) { if (dead_t > 0.6f) reset_run(); }
    else flap();
}

static void touch(SceneTouch type, int, int) {
    if (type != SCENE_TOUCH_PRESS || stopping) return;
    since_touch = 0;
    poke();
}

static void event(const SceneEvent &e) {
    if (e.type == SCENE_EV_KEY && e.key == SCENE_KEY_SPACE && !stopping) poke();
}

static void finish() { pixfb_free(fb); }

const Scene scene_flappy = { "flappy", start, tick, request_stop, is_done, touch, finish, event };
SCENE_REGISTER(scene_flappy, "Flappy Bird");
