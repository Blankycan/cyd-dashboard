#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Space Invaders — a marching alien formation, a cannon that plays itself, and
// bombs coming down. Cleared or landed waves are replaced by a faster one.
// When asked to stop, the remaining aliens chain-explode and it reports done.
// Touch: drag to move the cannon, tap to fire. The computer takes back over
// AUTO_RESUME_MS after your last touch.

static const uint32_t AUTO_RESUME_MS = 5000;
static const int      AW = 8, AH = 6;                 // alien sprite size
static const int      COL_PITCH = 14, ROW_PITCH = 10;
static const int      MAX_COLS = 10, ROWS = 3;
static const int      MAX_BOMBS = 4;
static const uint32_t BOOM_MS = 250;
static const int      CANNON_W = 9, CANNON_H = 4;

static const uint16_t ALIEN[3][2][AH] = {
    {   // squid
        { 0b00011000, 0b00111100, 0b01111110, 0b11011011, 0b11111111, 0b00100100 },
        { 0b00011000, 0b00111100, 0b01111110, 0b11011011, 0b11111111, 0b01011010 },
    },
    {   // crab
        { 0b00100100, 0b10111101, 0b11011011, 0b11111111, 0b01111110, 0b01000010 },
        { 0b00100100, 0b00111100, 0b11011011, 0b11111111, 0b10111101, 0b10000001 },
    },
    {   // octopus
        { 0b00111100, 0b01111110, 0b11011011, 0b11111111, 0b00100100, 0b01011010 },
        { 0b00111100, 0b01111110, 0b11011011, 0b11111111, 0b01011010, 0b10000001 },
    },
};
static const uint16_t BOOM[AH]   = { 0b01000010, 0b10100101, 0b00011000, 0b00011000, 0b10100101, 0b01000010 };
static const uint16_t CANNON[CANNON_H] = { 0b000010000, 0b000111000, 0b111111111, 0b111111111 };

struct Rect { int x, y, w, h; };
struct Bomb { bool alive; float x, y; };

static PixFb    fb;
static int      area_w, area_h, cols;
static uint8_t  slot[ROWS][MAX_COLS];      // 0 dead, 1 alive, 2 exploding
static uint32_t boom_ms[ROWS][MAX_COLS];
static float    fx, fy;                    // formation top-left
static int      dir, frame, wave;
static uint32_t step_acc, step_ms;
static float    cannon_x, target_x;
static bool     cannon_dead;
static uint32_t cannon_dead_ms;
static bool     shot_alive;
static float    shot_x, shot_y;
static Bomb     bombs[MAX_BOMBS];
static bool     stopping, done, oom;
static uint32_t chain_acc, since_touch;
static Rect     prev_formation, prev_cannon, prev_shot, prev_bombs[MAX_BOMBS];

static int alive_count() {
    int n = 0;
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < cols; c++) n += slot[r][c] == 1;
    return n;
}

static int alien_x(int c) { return (int)fx + c * COL_PITCH; }
static int alien_y(int r) { return (int)fy + r * ROW_PITCH; }

static void new_wave() {
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < cols; c++) slot[r][c] = 1;
    fx = 4;
    fy = 3;
    dir = 1;
    step_acc = 0;
    wave++;
    for (int i = 0; i < MAX_BOMBS; i++) bombs[i].alive = false;
}

static void start(lv_obj_t *area, int w, int h) {
    area_w = w;
    area_h = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_ARCADE_BG);
    done = oom;
    if (oom) return;
    cols = (w - 24) / COL_PITCH;
    if (cols > MAX_COLS) cols = MAX_COLS;
    if (cols < 3) cols = 3;
    wave = 0;
    new_wave();
    cannon_x = target_x = w / 2.0f;
    cannon_dead = false;
    shot_alive = false;
    stopping = false;
    chain_acc = 0;
    since_touch = AUTO_RESUME_MS;
    prev_formation = prev_cannon = prev_shot = { 0, 0, 0, 0 };
    for (int i = 0; i < MAX_BOMBS; i++) prev_bombs[i] = { 0, 0, 0, 0 };
}

static void explode(int r, int c) { slot[r][c] = 2; boom_ms[r][c] = 0; }

static void fire() {
    if (shot_alive || cannon_dead || stopping) return;
    shot_alive = true;
    shot_x = cannon_x;
    shot_y = area_h - CANNON_H - 3;
}

// Pick a column to aim at and keep clear of bombs falling close by
static void autopilot() {
    int best = -1;
    float best_d = 1e9f;
    for (int c = 0; c < cols; c++) {
        bool any = false;
        for (int r = 0; r < ROWS; r++) any |= slot[r][c] == 1;
        if (!any) continue;
        float d = fabsf(alien_x(c) + AW / 2.0f - cannon_x);
        if (d < best_d) { best_d = d; best = c; }
    }
    if (best >= 0) target_x = alien_x(best) + AW / 2.0f;
    for (int i = 0; i < MAX_BOMBS; i++) {
        const Bomb &b = bombs[i];
        if (b.alive && b.y > area_h - 35 && fabsf(b.x - cannon_x) < 8)
            target_x = cannon_x + (b.x < cannon_x ? 14 : -14);
    }
    if (best >= 0 && best_d < 2.5f) fire();
}

static void update(uint32_t dt_ms) {
    float dt = dt_ms / 1000.0f;
    since_touch += dt_ms;

    // Formation march; it speeds up as it thins out and with each wave
    int alive = alive_count();
    step_ms = 60 + (uint32_t)(340.0f * alive / (ROWS * cols)) - (wave > 5 ? 50 : wave * 10);
    step_acc += dt_ms;
    if (step_acc >= step_ms && alive > 0) {
        step_acc = 0;
        frame ^= 1;
        int minc = cols, maxc = -1;
        for (int r = 0; r < ROWS; r++) for (int c = 0; c < cols; c++)
            if (slot[r][c] == 1) { if (c < minc) minc = c; if (c > maxc) maxc = c; }
        int left = alien_x(minc), right = alien_x(maxc) + AW;
        if ((dir > 0 && right + 2 >= area_w - 2) || (dir < 0 && left - 2 <= 2)) { fy += 3; dir = -dir; }
        else fx += 2 * dir;
    }

    for (int r = 0; r < ROWS; r++) for (int c = 0; c < cols; c++)
        if (slot[r][c] == 2 && (boom_ms[r][c] += dt_ms) >= BOOM_MS) slot[r][c] = 0;

    if (stopping) {
        chain_acc += dt_ms;
        if (chain_acc >= 70) {
            chain_acc = 0;
            int pick = random(0, alive + 1), n = 0;
            for (int r = 0; r < ROWS && pick >= 0; r++) for (int c = 0; c < cols; c++)
                if (slot[r][c] == 1 && n++ == pick) { explode(r, c); pick = -1; break; }
        }
    } else if (alive == 0) {
        new_wave();
    }

    // Landed: the wave wins, send a fresh one
    int lowest = -1;
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < cols; c++) if (slot[r][c] == 1) lowest = r;
    if (!stopping && lowest >= 0 && alien_y(lowest) + AH >= area_h - CANNON_H - 4) new_wave();

    // Cannon
    if (cannon_dead) {
        if ((cannon_dead_ms += dt_ms) > 1500) { cannon_dead = false; cannon_x = target_x = area_w / 2.0f; }
    } else {
        if (since_touch >= AUTO_RESUME_MS && !stopping) autopilot();
        float d = target_x - cannon_x, step = 70.0f * dt;
        cannon_x += d > step ? step : d < -step ? -step : d;
        if (cannon_x < CANNON_W / 2.0f) cannon_x = CANNON_W / 2.0f;
        if (cannon_x > area_w - CANNON_W / 2.0f) cannon_x = area_w - CANNON_W / 2.0f;
    }

    // Shot
    if (shot_alive) {
        shot_y -= 130.0f * dt;
        if (shot_y < 0) shot_alive = false;
        for (int r = 0; r < ROWS && shot_alive; r++) for (int c = 0; c < cols; c++) {
            if (slot[r][c] != 1) continue;
            int ax = alien_x(c), ay = alien_y(r);
            if (shot_x >= ax && shot_x < ax + AW && shot_y >= ay && shot_y < ay + AH) {
                explode(r, c);
                shot_alive = false;
                break;
            }
        }
    }

    // Bombs, dropped by the lowest alien of a random column
    if (!stopping && random(0, 1000) < (int)(dt_ms * (1 + wave / 3))) {
        int c = random(0, cols);
        for (int r = ROWS - 1; r >= 0; r--) {
            if (slot[r][c] != 1) continue;
            for (int i = 0; i < MAX_BOMBS; i++) if (!bombs[i].alive) {
                bombs[i] = { true, alien_x(c) + AW / 2.0f, (float)alien_y(r) + AH };
                break;
            }
            break;
        }
    }
    float cy = area_h - CANNON_H - 1;
    for (int i = 0; i < MAX_BOMBS; i++) {
        Bomb &b = bombs[i];
        if (!b.alive) continue;
        b.y += 38.0f * dt;
        if (b.y > area_h) { b.alive = false; continue; }
        if (!cannon_dead && b.y + 3 >= cy && fabsf(b.x - cannon_x) <= CANNON_W / 2.0f) {
            b.alive = false;
            cannon_dead = true;
            cannon_dead_ms = 0;
        }
    }

    if (stopping) {
        bool any = alive_count() > 0 || shot_alive;
        for (int r = 0; r < ROWS; r++) for (int c = 0; c < cols; c++) any |= slot[r][c] == 2;
        for (int i = 0; i < MAX_BOMBS; i++) any |= bombs[i].alive;
        done = !any;
    }
}

static void draw() {
    lv_color_t bg = COL_SCENE_ARCADE_BG;

    // Erase everything drawn last frame, then draw everything again
    pixfb_fill(fb, prev_formation.x, prev_formation.y, prev_formation.w, prev_formation.h, bg);
    pixfb_fill(fb, prev_cannon.x, prev_cannon.y, prev_cannon.w, prev_cannon.h, bg);
    pixfb_fill(fb, prev_shot.x, prev_shot.y, prev_shot.w, prev_shot.h, bg);
    for (int i = 0; i < MAX_BOMBS; i++)
        pixfb_fill(fb, prev_bombs[i].x, prev_bombs[i].y, prev_bombs[i].w, prev_bombs[i].h, bg);

    int x0 = area_w, y0 = area_h, x1 = 0, y1 = 0;
    for (int r = 0; r < ROWS; r++) {
        lv_color_t col = r == 0 ? COL_SCENE_INV_ALIEN_1 : r == 1 ? COL_SCENE_INV_ALIEN_2 : COL_SCENE_INV_ALIEN_3;
        for (int c = 0; c < cols; c++) {
            if (!slot[r][c]) continue;
            int ax = alien_x(c), ay = alien_y(r);
            if (slot[r][c] == 1) pixfb_sprite(fb, ax, ay, ALIEN[r][frame], AW, AH, col);
            else                 pixfb_sprite(fb, ax, ay, BOOM, AW, AH, COL_SCENE_INV_BOOM);
            if (ax < x0) x0 = ax;
            if (ay < y0) y0 = ay;
            if (ax + AW > x1) x1 = ax + AW;
            if (ay + AH > y1) y1 = ay + AH;
        }
    }
    prev_formation = x1 > x0 ? Rect{ x0, y0, x1 - x0, y1 - y0 } : Rect{ 0, 0, 0, 0 };

    int cx = (int)cannon_x - CANNON_W / 2, cy = area_h - CANNON_H - 1;
    if (cannon_dead) {
        if (cannon_dead_ms < 600) pixfb_sprite(fb, cx, cy - 2, BOOM, AW, AH, COL_SCENE_INV_BOOM);
        prev_cannon = { cx, cy - 2, AW, AH };
    } else if (!stopping || !done) {
        pixfb_sprite(fb, cx, cy, CANNON, CANNON_W, CANNON_H, COL_SCENE_INV_CANNON);
        prev_cannon = { cx, cy, CANNON_W, CANNON_H };
    }

    prev_shot = { 0, 0, 0, 0 };
    if (shot_alive) {
        pixfb_fill(fb, (int)shot_x, (int)shot_y, 1, 4, COL_SCENE_INV_SHOT);
        prev_shot = { (int)shot_x, (int)shot_y, 1, 4 };
    }
    for (int i = 0; i < MAX_BOMBS; i++) {
        prev_bombs[i] = { 0, 0, 0, 0 };
        if (!bombs[i].alive) continue;
        int bx = (int)bombs[i].x, by = (int)bombs[i].y;
        pixfb_fill(fb, bx, by, 1, 3, COL_SCENE_INV_BOMB);
        prev_bombs[i] = { bx, by, 1, 3 };
    }
}

static void tick(uint32_t dt_ms) {
    if (oom) return;
    update(dt_ms);
    draw();
    pixfb_flush(fb);
}

static void request_stop() { stopping = true; shot_alive = false; }
static bool is_done()      { return done; }

static void touch(SceneTouch type, int x, int) {
    if (type == SCENE_TOUCH_RELEASE) return;
    since_touch = 0;
    target_x = x;
    if (type == SCENE_TOUCH_PRESS) fire();
}

static void finish() { pixfb_free(fb); }

const Scene scene_invaders = { "invaders", start, tick, request_stop, is_done, touch, finish, nullptr, true };
SCENE_REGISTER(scene_invaders, "Space Invaders");
