#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Space Invaders — a marching alien formation, a cannon that plays itself, and
// bombs coming down. Cleared or landed waves are replaced by a faster one.
// The cannon plays it like a sharpshooter: it works out where the march will
// have taken the aliens by the time its shot gets there, fires only when the
// shot will hit, whittles down the outer columns (so the formation has
// further to go before each step down), and only ever moves somewhere no
// falling bomb will reach before it can get out of the way.
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
static const float    CANNON_V = 70, SHOT_V = 130, BOMB_V = 38;   // px/s

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

// Where the formation's top-left will be ms from now, marching as update() does
static void predict(float ms, int minc, int maxc, float &px, float &py) {
    float x = fx, y = fy, t = ms, acc = step_acc;
    int d = dir;
    while (acc + t >= step_ms) {
        t -= step_ms - acc;
        acc = 0;
        int left = (int)x + minc * COL_PITCH, right = (int)x + maxc * COL_PITCH + AW;
        if ((d > 0 && right + 2 >= area_w - 2) || (d < 0 && left - 2 <= 2)) { y += 3; d = -d; }
        else x += 2 * d;
    }
    px = x;
    py = y;
}

// How long a shot fired now takes to reach the bottom of row r, and where
// the formation will be by then (two passes, to allow for a step down)
static float shot_time(int r, int minc, int maxc, float &px, float &py) {
    float y0 = area_h - CANNON_H - 3, t = 0;
    py = fy;
    for (int k = 0; k < 2; k++) {
        t = (y0 - (py + r * ROW_PITCH + AH - 1)) / SHOT_V * 1000;
        predict(t, minc, maxc, px, py);
    }
    return t;
}

// Would a shot fired from x now hit anything?
static bool shot_hits(float x, int minc, int maxc, const int lowest[]) {
    for (int c = minc; c <= maxc; c++) {
        if (lowest[c] < 0) continue;
        float px, py;
        shot_time(lowest[c], minc, maxc, px, py);
        int ax = (int)px + c * COL_PITCH;
        if (x >= ax + 1 && x < ax + AW - 1) return true;
    }
    return false;
}

// Would a cannon heading straight for x (and then waiting there) be clear of
// every bomb while it's low enough to hit?
static bool path_safe(float x) {
    float cy = area_h - CANNON_H - 1, dist = fabsf(x - cannon_x), sgn = x > cannon_x ? 1 : -1;
    for (int i = 0; i < MAX_BOMBS; i++) {
        const Bomb &b = bombs[i];
        if (!b.alive) continue;
        float t0 = fmaxf(0, (cy - 3 - b.y) / BOMB_V), t1 = (area_h - b.y) / BOMB_V;
        float p0 = cannon_x + sgn * fminf(CANNON_V * t0, dist), p1 = cannon_x + sgn * fminf(CANNON_V * t1, dist);
        float lo = fminf(p0, p1), hi = fmaxf(p0, p1), reach = CANNON_W / 2.0f + 2;
        if (hi > b.x - reach && lo < b.x + reach) return false;
    }
    return true;
}

// The soonest a shot can meet column c's lowest alien (row r): walk the march
// ahead one step at a time and take the first stretch it stands still long
// enough for the cannon to get under it and the shot to climb up. Late in a
// wave the march outruns the cannon, so this waits ahead rather than chasing
static bool intercept(int c, int r, int minc, int maxc, float &sx, float &t_hit) {
    float x = fx, y = fy, t0 = 0, t1 = (float)step_ms - (float)step_acc;
    float y0 = area_h - CANNON_H - 3, half = CANNON_W / 2.0f;
    int d = dir;
    while (t0 < 6000) {
        float ax = (int)x + c * COL_PITCH + AW / 2.0f;
        sx = fminf(fmaxf(ax, half), area_w - half);
        float climb = (y0 - (y + r * ROW_PITCH + AH - 1)) / SHOT_V * 1000;
        float hit = fmaxf(fabsf(sx - cannon_x) / CANNON_V * 1000 + climb, t0);
        if (fabsf(sx - ax) < 2 && hit + 10 < t1) { t_hit = hit; return true; }
        int left = (int)x + minc * COL_PITCH, right = (int)x + maxc * COL_PITCH + AW;
        if ((d > 0 && right + 2 >= area_w - 2) || (d < 0 && left - 2 <= 2)) { y += 3; d = -d; }
        else x += 2 * d;
        t0 = t1;
        t1 += step_ms;
    }
    return false;
}

static void autopilot() {
    int lowest[MAX_COLS], minc = cols, maxc = -1;
    for (int c = 0; c < cols; c++) {
        lowest[c] = -1;
        for (int r = 0; r < ROWS; r++) if (slot[r][c] == 1) lowest[c] = r;
        if (lowest[c] >= 0) { if (c < minc) minc = c; if (c > maxc) maxc = c; }
    }
    float half = CANNON_W / 2.0f, want = cannon_x;
    if (maxc >= 0) {
        if (shot_hits(cannon_x, minc, maxc, lowest)) fire();
        // Pick a column: the soonest to hit, but the outer ones count as
        // sooner, and so do the lowest aliens once they're getting near
        float best = 1e9f, danger = fy + ROWS * ROW_PITCH > area_h * 0.55f ? 0.4f : 0;
        for (int c = minc; c <= maxc; c++) {
            if (lowest[c] < 0) continue;
            float x, t;
            if (!intercept(c, lowest[c], minc, maxc, x, t)) continue;
            float cost = t / 1000 - danger * lowest[c];
            if (c == minc || c == maxc) cost -= 0.35f;
            if (cost < best) { best = cost; want = x; }
        }
    }
    // Go there by a way no bomb crosses; if that's blocked, the nearest spot
    // that is. Small corrections wait, so it doesn't twitch
    if (fabsf(want - target_x) < 3 && path_safe(target_x)) return;
    target_x = want;
    if (!path_safe(want)) {
        float best = 1e9f;
        for (float x = half; x <= area_w - half; x += 1)
            if (fabsf(x - want) < best && path_safe(x)) { best = fabsf(x - want); target_x = x; }
    }
}

static void update(uint32_t dt_ms) {
    float dt = dt_ms / 1000.0f;
    since_touch += dt_ms;

    // Formation march; it speeds up as it thins out and with each wave
    int alive = alive_count();
    step_ms = 60 + (uint32_t)(340.0f * alive / (ROWS * cols)) - (wave > 5 ? 50 : wave * 10);
    // Leftover time carries over, so the march keeps the pace autopilot predicts
    step_acc += dt_ms;
    while (step_acc >= step_ms && alive > 0) {
        step_acc -= step_ms;
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
        float d = target_x - cannon_x, step = CANNON_V * dt;
        cannon_x += d > step ? step : d < -step ? -step : d;
        if (cannon_x < CANNON_W / 2.0f) cannon_x = CANNON_W / 2.0f;
        if (cannon_x > area_w - CANNON_W / 2.0f) cannon_x = area_w - CANNON_W / 2.0f;
    }

    // Shot
    if (shot_alive) {
        shot_y -= SHOT_V * dt;
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
        b.y += BOMB_V * dt;
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
    // In steps of at most 20 ms, so a slow frame doesn't let a shot or a bomb skip past
    for (uint32_t left = dt_ms; left > 0; ) {
        uint32_t step = left > 20 ? (left + 1) / ((left + 19) / 20) : left;
        update(step);
        left -= step;
    }
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
