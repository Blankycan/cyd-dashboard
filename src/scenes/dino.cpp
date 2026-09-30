#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Dino runner — the offline-browser T-rex, running through the desert,
// jumping cacti and ducking pterodactyls while the pace picks up. It plays
// itself (and now and then misjudges a jump; after a moment it starts over).
// At night (by the real clock) a moon and stars come out. When asked to stop,
// no more obstacles come, and once the way is clear the dino sprints off
// the right edge.
// The distance counter at the top right blinks every 100, with the best
// run so far (since the board started) beside it.
// Touch: tap to jump (or to start over after a crash). The computer takes
// back over AUTO_RESUME_MS after your last touch.
// Events: Space on the host keyboard jumps, just like the real one.

static const uint32_t AUTO_RESUME_MS = 5000;
static const int      MAX_OBS = 4, MAX_CLOUDS = 3;
static const float    GRAVITY = 960, JUMP_V = 265, AIR_T = 2 * JUMP_V / GRAVITY;
static const float    START_SPEED = 95, MAX_SPEED = 210;

// '1' ink, '2' the background (the eye)
static const char *const DINO_TOP[] = {   // 20 wide, shared by every standing frame
    "...........11111111.",
    "..........1121111111",
    "..........1111111111",
    "..........1111111111",
    "..........11111.....",
    "..........11111111..",
    "1........11111......",
    "1.......1111111.....",
    "11.....111111111....",
    "111...11111111.1....",
    "1111111111111.......",
    "1111111111111.......",
    ".11111111111........",
    "..111111111.........",
    "...1111111..........",
};
static const char *const DINO_LEGS[3][4] = {
    { "....111.11..", "....11...1..", "....1....1..", "....11...11." },   // standing / in the air
    { "....111.11..", "....11...11.", "....1.......", "....11......" },   // running, left foot down
    { "....111.11..", "....1....1..", ".........1..", ".........11." },   // running, right foot down
};
static const char *const DINO_DEAD_EYE[] = { "1.1", ".1.", "1.1" };        // drawn over the eye
static const char *const DINO_DUCK[2][11] = {
    {   ".................11111111.",
        "1.......11111111112111111.",
        "11...111111111111111111111",
        "1111111111111111111111111.",
        "111111111111111111111.....",
        ".1111111111111111111111...",
        "..11111111111111.1........",
        "...111111111111...........",
        "....11...11...............",
        "....1.....11..............",
        "....11....................", },
    {   ".................11111111.",
        "1.......11111111112111111.",
        "11...111111111111111111111",
        "1111111111111111111111111.",
        "111111111111111111111.....",
        ".1111111111111111111111...",
        "..11111111111111.1........",
        "...111111111111...........",
        "....11...11...............",
        "....11....1...............",
        "..........11..............", },
};
static const char *const CACTUS_S[] = {   // 7 x 13
    "...1...", "..111..", "..111..", "1.111..", "1.111.1", "1.111.1", "11111.1",
    ".111111", "..111..", "..111..", "..111..", "..111..", "..111..",
};
static const char *const CACTUS_L[] = {   // 11 x 19
    "....111....", "...11111...", "...11111...", "...11111..1", "1..11111.11", "11.11111.11",
    "11.11111.11", "11.11111.11", "11.11111.11", "11.11111111", "11111111111", ".1111111111",
    "...11111...", "...11111...", "...11111...", "...11111...", "...11111...", "...11111...",
    "...11111...",
};
static const char *const PTERO[2][10] = {   // 18 x 10, wings up / down
    {   ".....1............", ".....11...........", ".....111..........", "..1..1111.........",
        ".11..11111........", "111111111111111...", "....1111111111111.", "......111111111...",
        "..................", ".................." },
    {   "..................", "..................", "..................", "..1...............",
        ".11...............", "111111111111111...", "....1111111111111.", ".....11111111.....",
        ".....1111.........", ".....11..........." },
};
static const char *const CLOUD[] = {
    "........1111......", "......11....111...", "..1111.........11.", "11...............1", "111111111111111111",
};
static const char *const MOON[] = {
    "..111..", ".11....", "111....", "111....", "111....", ".11....", "..111..",
};
static const char *const HI_ART[] = {   // "HI", in the digits' 5x7 style
    "1...1.111", "1...1..1.", "1...1..1.", "11111..1.", "1...1..1.", "1...1..1.", "1...1.111",
};
static const char *const RESTART[] = {   // the circular arrow shown after a crash
    "...11111.1", "..11...111", ".11...1111", ".1.........", ".1........1", ".11......11", "..11....11.", "...111111..",
};

enum ObsType { OBS_CACTUS_S, OBS_CACTUS_L, OBS_PTERO };
struct Obstacle { bool alive; uint8_t type, count; float x; int y, w, h; bool misjudge; };
struct Cloud    { float x; int y; };

static PixFb    fb;
static int      W, H, ground_y, dino_x;
static Obstacle obs[MAX_OBS];
static Cloud    clouds[MAX_CLOUDS];
static float    speed, scroll, jump_h, jump_v, gap_left, run_t, dead_t, exit_x;
static bool     ducking, dead, stopping, done, oom, night;
static uint32_t since_touch;
static float    dist, flash_t;   // distance run this go (the score is a tenth of it); milestone blink
static long     hi_score;        // best since the board started

static uint32_t hash32(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

static void paint_backdrop() {
    pixfb_fill(fb, 0, 0, W, H, COL_SCENE_ARCADE_BG);
    int h = scene_ctx().hour;
    night = h >= 0 && (h >= 20 || h < 6);
    if (night) {   // stars and a moon, as the real one does at night
        lv_color_t pal[] = { COL_SCENE_DINO_MOON };
        for (int i = 0; i < W / 12; i++) {
            uint32_t r = hash32(i * 977 + 3);
            pixfb_px(fb, r % W, (r >> 12) % (ground_y - 30), COL_SCENE_DINO_STAR);
        }
        pixfb_art(fb, W * 11 / 20, 6, MOON, 7, pal);
    }
    pixfb_touch(fb, 0, 0, W, H);
    pixfb_bg_save(fb);
}

static void reset_run() {
    for (int i = 0; i < MAX_OBS; i++) obs[i].alive = false;
    speed = START_SPEED;
    jump_h = jump_v = 0;
    gap_left = W * 0.6f;
    ducking = dead = false;
    dead_t = 0;
    dist = flash_t = 0;
}

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_ARCADE_BG);
    done = oom;
    if (oom) return;
    ground_y = h - 10;
    dino_x = 18;
    paint_backdrop();
    if (!fb.bg) { done = oom = true; return; }
    for (int i = 0; i < MAX_CLOUDS; i++) clouds[i] = { scene_randf(0, w), (int)scene_randf(6, ground_y - 40) };
    reset_run();
    scroll = run_t = 0;
    exit_x = dino_x;
    stopping = false;
    since_touch = AUTO_RESUME_MS;
}

static bool on_ground() { return jump_h <= 0 && jump_v <= 0; }

static void jump() {
    if (!on_ground() || dead) return;
    jump_v = JUMP_V;
    ducking = false;
}

static void spawn_obstacle() {
    for (int i = 0; i < MAX_OBS; i++) {
        Obstacle &o = obs[i];
        if (o.alive) continue;
        int kind = random(0, 10);
        o.alive = true;
        o.x = W + 4;
        o.misjudge = random(0, 100) < 6;   // now and then the computer gets it wrong
        if (kind < 5)      { o.type = OBS_CACTUS_S; o.count = random(1, 4); o.w = o.count * 8 - 1; o.h = 13; }
        else if (kind < 8) { o.type = OBS_CACTUS_L; o.count = random(1, 3); o.w = o.count * 12 - 1; o.h = 19; }
        else {
            o.type = OBS_PTERO; o.count = 1; o.w = 18; o.h = 10;
            int level = speed < 130 ? random(0, 2) * 2 : random(0, 3);   // low, mid (duck), high (run under)
            o.y = ground_y - (level == 0 ? 12 : level == 1 ? 26 : 38);
            return;
        }
        o.y = ground_y - o.h + 2;
        return;
    }
}

static void autopilot() {
    // The nearest obstacle still ahead of the dino
    const Obstacle *next = nullptr;
    for (int i = 0; i < MAX_OBS; i++)
        if (obs[i].alive && obs[i].x + obs[i].w > dino_x && (!next || obs[i].x < next->x)) next = &obs[i];
    ducking = false;
    if (!next) return;
    float ahead = next->x + next->w / 2.0f - (dino_x + 8);
    bool mid_ptero = next->type == OBS_PTERO && next->y == ground_y - 26;
    bool high_ptero = next->type == OBS_PTERO && next->y == ground_y - 38;
    if (high_ptero) return;
    if (mid_ptero) { ducking = on_ground() && ahead < 40; return; }
    // Time the jump so its peak is over the obstacle's middle
    float lead = speed * AIR_T / 2 * (next->misjudge ? 1.9f : 1);   // a misjudged one goes up far too soon
    if (ahead <= lead && ahead > lead - 12) jump();
}

static bool hits(const Obstacle &o) {
    // Forgiving boxes, a couple of pixels inside the art
    int dh = ducking ? 11 : 19, dw = ducking ? 24 : 16;
    int dy1 = ground_y + 2 - (int)jump_h, dy0 = dy1 - dh;
    int dx0 = dino_x + 3, dx1 = dino_x + dw - 2;
    int ox0 = (int)o.x + 2, ox1 = (int)o.x + o.w - 2, oy0 = o.y + 2, oy1 = o.y + o.h;
    return dx1 > ox0 && dx0 < ox1 && dy1 > oy0 + 1 && dy0 < oy1 - 2;
}

static void update(float dt) {
    since_touch += (uint32_t)(dt * 1000);
    if (dead) {
        dead_t += dt;
        if (stopping && dead_t > 1.0f) done = true;
        else if (dead_t > 2.0f && since_touch >= AUTO_RESUME_MS) reset_run();
        return;
    }
    run_t += dt;
    if (!stopping) speed = fminf(MAX_SPEED, speed + dt * 1.2f);
    float move = speed * dt;
    scroll += move;
    // The score counts distance; every 100 it blinks, as the real one does
    long before = (long)(dist * 0.1f);
    dist += move;
    long now = (long)(dist * 0.1f);
    if (now / 100 != before / 100) flash_t = 1.0f;
    if (flash_t > 0) flash_t -= dt;
    if (now > hi_score) hi_score = now;

    // Obstacles scroll past; new ones come at random gaps that grow with speed
    for (int i = 0; i < MAX_OBS; i++) {
        Obstacle &o = obs[i];
        if (!o.alive) continue;
        o.x -= move + (o.type == OBS_PTERO ? 20 * dt : 0);   // pterodactyls fly toward you too
        if (o.x + o.w < -2) o.alive = false;
    }
    if (!stopping && (gap_left -= move) <= 0) {
        spawn_obstacle();
        gap_left = scene_randf(fmaxf(90, speed * 0.85f), speed * 1.9f);
    }
    for (int i = 0; i < MAX_CLOUDS; i++) {
        clouds[i].x -= move * 0.15f;
        if (clouds[i].x < -20) clouds[i] = { W + scene_randf(0, 60), (int)scene_randf(6, ground_y - 40) };
    }

    if (since_touch >= AUTO_RESUME_MS || stopping) autopilot();   // it clears what's left before leaving
    jump_v -= GRAVITY * dt;
    jump_h += jump_v * dt;
    if (jump_h <= 0) { jump_h = 0; jump_v = 0; }

    for (int i = 0; i < MAX_OBS; i++)
        if (obs[i].alive && hits(obs[i])) { dead = true; dead_t = 0; jump_v = 0; return; }

    // Wrapping up: once the way is clear the dino sprints off to the right
    if (stopping) {
        bool any = false;
        for (int i = 0; i < MAX_OBS; i++) any |= obs[i].alive;
        if (!any) { exit_x += (60 + exit_x) * dt * 2; if (exit_x > W + 4) done = true; }
    }
}

static void draw() {
    pixfb_bg_restore(fb, 0, 0, W, H);
    lv_color_t ink = COL_SCENE_DINO_INK;
    lv_color_t pal[] = { ink, COL_SCENE_ARCADE_BG };
    lv_color_t cloud_pal[] = { COL_SCENE_DINO_CLOUD };
    for (int i = 0; i < MAX_CLOUDS; i++) pixfb_art(fb, (int)clouds[i].x, clouds[i].y, CLOUD, 5, cloud_pal);

    // Ground: a line with the odd bump, and pebbles scrolling by beneath it
    for (int x = 0; x < W; x++) {
        uint32_t wx = (uint32_t)(scroll + x);
        bool bump = hash32(wx / 6) % 9 == 0;
        pixfb_px(fb, x, ground_y - (bump && hash32(wx / 6 + 1) % 2 ? 1 : 0), ink);
        uint32_t r = hash32(wx * 31 + 7);
        if (r % 17 == 0) pixfb_px(fb, x, ground_y + 2 + (r >> 8) % 6, COL_SCENE_DINO_CLOUD);
        else if (r % 41 == 0) { pixfb_px(fb, x, ground_y + 3 + (r >> 8) % 4, ink); }
    }

    for (int i = 0; i < MAX_OBS; i++) {
        const Obstacle &o = obs[i];
        if (!o.alive) continue;
        int x = (int)o.x;
        if (o.type == OBS_PTERO) pixfb_art(fb, x, o.y, PTERO[(int)(run_t * 5) & 1], 10, pal, true);
        else for (int k = 0; k < o.count; k++)
            pixfb_art(fb, x + k * (o.type == OBS_CACTUS_S ? 8 : 12), o.y, o.type == OBS_CACTUS_S ? CACTUS_S : CACTUS_L,
                      o.type == OBS_CACTUS_S ? 13 : 19, pal);
    }

    int dx = stopping ? (int)exit_x : dino_x, feet = ground_y + 2 - (int)jump_h;
    if (ducking && on_ground()) pixfb_art(fb, dx, feet - 11, DINO_DUCK[(int)(run_t * 10) & 1], 11, pal);
    else {
        int top = feet - 19;
        pixfb_art(fb, dx, top, DINO_TOP, 15, pal);
        int legs = (dead || !on_ground()) ? 0 : 1 + ((int)(run_t * 10) & 1);
        pixfb_art(fb, dx, top + 15, DINO_LEGS[legs], 4, pal);
        if (dead) {
            lv_color_t eye[] = { COL_SCENE_ARCADE_BG };
            pixfb_fill(fb, dx + 11, top, 3, 3, ink);
            pixfb_art(fb, dx + 11, top, DINO_DEAD_EYE, 3, eye);
        }
    }
    // Score, top right: HI 00123  00456
    long score = (long)(dist * 0.1f), shown = flash_t > 0 ? score / 100 * 100 : score;
    int sx = W - 4 - pixfb_number_width(0, 5);
    if (flash_t <= 0 || fmodf(flash_t, 0.25f) < 0.125f) pixfb_number(fb, sx, 4, shown, ink, 5);
    int hx = sx - 8 - pixfb_number_width(0, 5);
    lv_color_t hi_pal[] = { COL_SCENE_DINO_CLOUD };
    pixfb_number(fb, hx, 4, hi_score, COL_SCENE_DINO_CLOUD, 5);
    pixfb_art(fb, hx - 13, 4, HI_ART, 7, hi_pal);
    if (dead && dead_t > 0.4f && !stopping) pixfb_art(fb, W / 2 - 5, ground_y / 2 - 4, RESTART, 8, pal);
}

static void tick(uint32_t dt_ms) {
    if (oom || done) return;
    update(dt_ms / 1000.0f);
    if (done) return;
    draw();
    pixfb_flush(fb);
}

static void request_stop() { stopping = true; exit_x = dino_x; }
static bool is_done()      { return done; }

static void touch(SceneTouch type, int, int) {
    if (type != SCENE_TOUCH_PRESS || stopping) return;
    since_touch = 0;
    if (dead) { if (dead_t > 0.4f) reset_run(); }
    else jump();
}

static void event(const SceneEvent &e) {
    if (e.type == SCENE_EV_KEY && e.key == SCENE_KEY_SPACE && !stopping) {
        if (dead) { if (dead_t > 0.4f) reset_run(); }
        else jump();
    }
    if (e.type == SCENE_EV_HOUR) paint_backdrop();
}

static void finish() { pixfb_free(fb); }

const Scene scene_dino = { "dino", start, tick, request_stop, is_done, touch, finish, event };
SCENE_REGISTER(scene_dino, "Dino runner");
