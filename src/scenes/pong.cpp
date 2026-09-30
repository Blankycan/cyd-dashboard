#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Pong — the 1972 arcade original: two paddles, a square ball, a dashed net
// and the big blocky scores. Each paddle is eight segments: the middle ones
// send the ball back nearly flat, the ends at a steep angle. The ball speeds
// up after the 4th and 12th hit of a rally, and, as in the original, the
// paddles can't quite reach the top of the screen. First to 11 wins; the
// winning score blinks, then a new game. Both sides play themselves, not
// perfectly. When asked to stop, the paddles let the ball through and it runs out.
// Touch: drag to move the right-hand paddle. The computer takes back over
// AUTO_RESUME_MS after your last touch.
// Events: with music playing the net pulses on the beat.

static const uint32_t AUTO_RESUME_MS = 5000;
static const int      PAD_W = 3, PAD_H = 16, BALL = 3, TOP_GAP = 4, WIN_SCORE = 11;
static const float    SPEEDS[3] = { 95, 130, 165 };   // before the 4th hit, to the 12th, after
static const float    PAD_V = 95;

struct Paddle { float y, target, aim; };   // y: top; aim: where on the paddle it means to take the ball

static PixFb  fb;
static int    W, H, pad_x[2], score[2], hits, serve_to;
static Paddle pad[2];
static float  bx, by, vx, vy, serve_t, over_t, beat_t;
static bool   in_play, stopping, done, oom;
static uint32_t since_touch;

// 7-segment digits (a, b, c, d, e, f, g as bits 0..6), drawn as thick bars
static const uint8_t SEG[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };

static void draw_digit(int x, int y, int d, lv_color_t c) {
    const int w = 10, h = 16, t = 2;
    uint8_t s = SEG[d];
    if (s & 0x01) pixfb_fill(fb, x, y, w, t, c);                      // a
    if (s & 0x02) pixfb_fill(fb, x + w - t, y, t, h / 2 + 1, c);      // b
    if (s & 0x04) pixfb_fill(fb, x + w - t, y + h / 2, t, h / 2, c);  // c
    if (s & 0x08) pixfb_fill(fb, x, y + h - t, w, t, c);              // d
    if (s & 0x10) pixfb_fill(fb, x, y + h / 2, t, h / 2, c);          // e
    if (s & 0x20) pixfb_fill(fb, x, y, t, h / 2 + 1, c);              // f
    if (s & 0x40) pixfb_fill(fb, x, y + h / 2 - 1, w, t, c);          // g
}

static void draw_score(int x, int v, lv_color_t c) {   // right-aligned at x
    if (v >= 10) { draw_digit(x - 23, 3, v / 10, c); }
    draw_digit(x - 10, 3, v % 10, c);
}

static void serve() {
    // From the middle, toward the side that lost the last point, at a random slant
    in_play = false;
    serve_t = 0;
    bx = W / 2.0f - BALL / 2.0f;
    by = scene_randf(H * 0.25f, H * 0.7f);
    hits = 0;
}

static void launch() {
    in_play = true;
    float sp = SPEEDS[0];
    vx = serve_to ? sp : -sp;
    vy = scene_randf(-0.35f, 0.35f) * sp;
    for (int s = 0; s < 2; s++) pad[s].aim = scene_randf(-0.8f, 0.8f);
}

static void new_game() {
    score[0] = score[1] = 0;
    over_t = -1;
    serve_to = random(0, 2);
    serve();
}

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_PONG_BG);
    done = oom;
    if (oom) return;
    if (!pixfb_bg_save(fb)) { done = oom = true; return; }
    pad_x[0] = 8; pad_x[1] = w - 8 - PAD_W;
    for (int s = 0; s < 2; s++) pad[s] = { (h - PAD_H) / 2.0f, (h - PAD_H) / 2.0f, 0 };
    new_game();
    beat_t = 0;
    stopping = false;
    since_touch = AUTO_RESUME_MS;
}

static float speed() { return SPEEDS[hits < 4 ? 0 : hits < 12 ? 1 : 2]; }

static void bounce(int side) {
    // Which of the paddle's eight segments took it sets the new angle
    float rel = (by + BALL / 2.0f - pad[side].y) / PAD_H;   // 0 top .. 1 bottom
    int seg = (int)(rel * 8);
    if (seg < 0) seg = 0;
    if (seg > 7) seg = 7;
    static const float SLOPE[8] = { -0.9f, -0.6f, -0.3f, -0.1f, 0.1f, 0.3f, 0.6f, 0.9f };
    hits++;
    float sp = speed();
    vy = SLOPE[seg] * sp;
    vx = (side == 0 ? 1 : -1) * sqrtf(fmaxf(sp * sp - vy * vy, sp * sp * 0.4f));
    // The one who has to return it next picks where to take it on the paddle
    // (and sometimes misjudges it)
    int other = 1 - side;
    pad[other].aim = random(0, 100) < 35 ? (random(0, 2) ? 1.6f : -1.6f) : scene_randf(-0.8f, 0.8f);
}

// Where the ball will be when it reaches this paddle's side, folding it off
// the top and bottom walls
static float predict_y(int side) {
    float x_to = side == 0 ? pad_x[0] + PAD_W : pad_x[1] - BALL;
    float t = (x_to - bx) / vx;
    if (t < 0) return by;
    float span = H - BALL, y = fmodf(fabsf(by + vy * t), 2 * span);
    return y > span ? 2 * span - y : y;
}

static void steer(int side, float dt, bool computer) {
    Paddle &p = pad[side];
    if (computer && !stopping) {   // wrapping up: they stop chasing, so the rally ends
        bool coming = in_play && (side == 0 ? vx < 0 : vx > 0);
        float y = coming ? predict_y(side) : H / 2.0f;
        p.target = y + BALL / 2.0f - PAD_H / 2.0f - p.aim * PAD_H / 2.0f;
    }
    float d = p.target - p.y, step = PAD_V * dt;
    p.y += d > step ? step : d < -step ? -step : d;
    if (p.y < TOP_GAP) p.y = TOP_GAP;           // the original paddles stop just short of the top
    if (p.y > H - PAD_H) p.y = H - PAD_H;
}

static void update(float dt) {
    since_touch += (uint32_t)(dt * 1000);
    if (beat_t > 0) beat_t -= dt;
    if (over_t >= 0) {   // game over: the winner's score blinks, the ball bounces about on its own
        over_t += dt;
        bx += vx * dt; by += vy * dt;
        if (bx < 0 || bx > W - BALL) vx = -vx;
        if (by < 0 || by > H - BALL) vy = -vy;
        if (over_t > 4 && !stopping) new_game();
        if (stopping) done = true;
        return;
    }
    steer(0, dt, true);
    steer(1, dt, since_touch >= AUTO_RESUME_MS);
    if (!in_play) {
        if (stopping) { done = true; return; }
        if ((serve_t += dt) > 1.0f) launch();
        return;
    }

    bx += vx * dt; by += vy * dt;
    if (by < 0)         { by = 0; vy = fabsf(vy); }
    if (by > H - BALL)  { by = H - BALL; vy = -fabsf(vy); }
    for (int s = 0; s < 2; s++) {
        bool toward = s == 0 ? vx < 0 : vx > 0;
        float face = s == 0 ? pad_x[0] + PAD_W : pad_x[1] - BALL;
        bool at_face = s == 0 ? bx <= face && bx > face - 4 : bx >= face && bx < face + 4;
        if (toward && at_face && !stopping && by + BALL > pad[s].y && by < pad[s].y + PAD_H) { bx = face; bounce(s); }
    }
    // Past a paddle: a point to the other side
    if (bx < -BALL || bx > W) {
        int winner = bx < 0 ? 1 : 0;
        if (!stopping) score[winner]++;
        serve_to = 1 - winner;
        if (score[winner] >= WIN_SCORE) {
            over_t = 0;
            bx = W / 2.0f; by = H / 2.0f;
            vx = 60 * (winner ? -1 : 1); vy = 45;
        } else serve();
    }
}

static void draw() {
    pixfb_bg_restore(fb, 0, 0, W, H);
    lv_color_t fg = COL_SCENE_PONG_FG;
    // The net: a dashed line down the middle, brighter on a beat
    lv_color_t net = beat_t > 0 ? pixfb_mix(COL_SCENE_PONG_NET, fg, beat_t / 0.15f) : COL_SCENE_PONG_NET;
    for (int y = 0; y < H; y += 6) pixfb_fill(fb, W / 2 - 1, y, 2, 3, net);
    // Scores, either side of the net; the winner's blinks when the game's over
    for (int s = 0; s < 2; s++) {
        bool blink = over_t >= 0 && score[s] >= WIN_SCORE && fmodf(over_t, 0.6f) > 0.35f;
        if (!blink) draw_score(s == 0 ? W / 2 - 14 : W / 2 + 14 + (score[1] >= 10 ? 23 : 10), score[s], fg);
    }
    for (int s = 0; s < 2; s++) pixfb_fill(fb, pad_x[s], (int)pad[s].y, PAD_W, PAD_H, fg);
    if (in_play || over_t >= 0 || (serve_t > 0.3f && !stopping)) pixfb_fill(fb, (int)bx, (int)by, BALL, BALL, fg);
}

static void tick(uint32_t dt_ms) {
    if (oom || done) return;
    update(dt_ms / 1000.0f);
    draw();
    pixfb_flush(fb);
}

static void request_stop() { stopping = true; }
static bool is_done()      { return done; }

static void touch(SceneTouch type, int, int y) {
    if (type == SCENE_TOUCH_RELEASE || stopping) return;
    since_touch = 0;
    pad[1].target = y - PAD_H / 2.0f;
}

static void event(const SceneEvent &e) {
    if (e.type == SCENE_EV_BEAT) beat_t = 0.15f;
}

static void finish() { pixfb_free(fb); }

const Scene scene_pong = { "pong", start, tick, request_stop, is_done, touch, finish, event, true };
SCENE_REGISTER(scene_pong, "Pong");
