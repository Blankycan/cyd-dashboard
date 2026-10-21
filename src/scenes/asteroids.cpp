#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Asteroids — vector-outline rocks drifting and spinning on a wrapping field,
// split into smaller rocks when shot. The ship flies itself like a good
// player: it shoots the rocks on a collision course first, leads every shot to
// where the rock will be, doesn't waste shots on rocks already as good as hit,
// and side-steps anything it can't shoot in time.
// Cleared fields get a fresh, bigger wave. When asked to stop, the ship warps
// out and the rocks drift off the edges (no more wrapping) until it's done.
// Touch: the ship turns toward your finger, thrusts if it's far away, and
// fires while you hold. The computer takes back over AUTO_RESUME_MS later.

static const uint32_t AUTO_RESUME_MS = 5000;
static const int      MAX_ROCKS   = 16;
static const int      MAX_SHOTS   = 6;
static const int      MAX_DEBRIS  = 24;
static const int      ROCK_VERTS  = 9;
static const float    SHIP_R      = 5.0f;
static const float    TURN_RATE   = 4.0f;      // rad/s
static const float    THRUST      = 60.0f;     // px/s²
static const float    SHOT_SPEED  = 110.0f;
static const float    SHOT_LIFE_S = 1.1f;
static const float    ROCK_R[3]   = { 12.0f, 7.0f, 4.0f };

struct Rect  { int x, y, w, h; };
struct Rock  { bool alive; int size; float x, y, vx, vy, a, spin; float r[ROCK_VERTS]; Rect prev; };
struct Shot  { bool alive; float x, y, vx, vy, life; Rect prev; };
struct Debris{ bool alive; float x, y, vx, vy, life; Rect prev; };

static PixFb  fb;
static int    area_w, area_h, wave;
static Rock   rocks[MAX_ROCKS];
static Shot   shots[MAX_SHOTS];
static Debris debris[MAX_DEBRIS];
static float  sx, sy, svx, svy, sa;          // ship
static bool   ship_alive, thrusting, holding;
static float  ship_timer, invuln, fire_cd, warp;   // warp: 1 → 0 while warping out
static float  finger_x, finger_y;
static uint32_t since_touch;
static bool   stopping, done, oom;
static Rect   prev_ship;

static float wrap_d(float d, float span) {   // shortest signed distance on a wrapping axis
    if (d >  span / 2) d -= span;
    if (d < -span / 2) d += span;
    return d;
}
static float angle_diff(float a, float b) {
    float d = fmodf(a - b + 3.14159f * 3, 6.28318f) - 3.14159f;
    return d;
}

static void spawn_rock(int size, float x, float y) {
    for (int i = 0; i < MAX_ROCKS; i++) {
        Rock &k = rocks[i];
        if (k.alive) continue;
        k.alive = true;
        k.size  = size;
        k.x = x; k.y = y;
        float speed = scene_randf(8, 18) * (1.0f + size * 0.4f);
        float dir   = scene_randf(0, 6.28f);
        k.vx = cosf(dir) * speed;
        k.vy = sinf(dir) * speed;
        k.a = scene_randf(0, 6.28f);
        k.spin = scene_randf(-1.2f, 1.2f);
        for (int v = 0; v < ROCK_VERTS; v++) k.r[v] = ROCK_R[size] * scene_randf(0.7f, 1.1f);
        k.prev = { 0, 0, 0, 0 };
        return;
    }
}

static void new_wave() {
    wave++;
    int n = 2 + wave;
    if (n > 5) n = 5;
    for (int i = 0; i < n; i++) {
        // Spawn along the edges, away from the ship
        float x = random(0, 2) ? scene_randf(0, area_w * 0.2f) : scene_randf(area_w * 0.8f, area_w);
        spawn_rock(0, x, scene_randf(0, area_h));
    }
}

static void reset_ship() {
    sx = area_w / 2.0f; sy = area_h / 2.0f;
    svx = svy = 0;
    sa = -1.5708f;
    ship_alive = true;
    invuln = 2.0f;
}

static void start(lv_obj_t *area, int w, int h) {
    area_w = w;
    area_h = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_ARCADE_BG);
    done = oom;
    if (oom) return;
    for (int i = 0; i < MAX_ROCKS; i++)  rocks[i].alive = false;
    for (int i = 0; i < MAX_SHOTS; i++)  shots[i] = {};
    for (int i = 0; i < MAX_DEBRIS; i++) debris[i] = {};
    wave = 0;
    reset_ship();
    new_wave();
    stopping = holding = thrusting = false;
    fire_cd = ship_timer = 0;
    warp = 1.0f;
    since_touch = AUTO_RESUME_MS;
    prev_ship = { 0, 0, 0, 0 };
}

static void burst(float x, float y, int n) {
    for (int i = 0; i < MAX_DEBRIS && n > 0; i++) {
        Debris &d = debris[i];
        if (d.alive) continue;
        float a = scene_randf(0, 6.28f), s = scene_randf(15, 45);
        d = { true, x, y, cosf(a) * s, sinf(a) * s, scene_randf(0.3f, 0.7f), d.prev };
        n--;
    }
}

static void fire() {
    if (fire_cd > 0 || !ship_alive || stopping) return;
    for (int i = 0; i < MAX_SHOTS; i++) {
        Shot &s = shots[i];
        if (s.alive) continue;
        s = { true, sx + cosf(sa) * SHIP_R, sy + sinf(sa) * SHIP_R,
              svx + cosf(sa) * SHOT_SPEED, svy + sinf(sa) * SHOT_SPEED, SHOT_LIFE_S, s.prev };
        fire_cd = 0.28f;
        return;
    }
}

// Turn toward a target angle; true once roughly facing it
static bool steer(float want, float dt) {
    float d = angle_diff(want, sa);
    float step = TURN_RATE * dt;
    sa += d > step ? step : d < -step ? -step : d;
    return fabsf(d) < 0.12f;
}

// Where a shot fired now meets a rock (relative position p, velocity v to the
// ship): the time, or -1 if it can't catch it
static float intercept_t(float px, float py, float vx, float vy) {
    float a = vx * vx + vy * vy - SHOT_SPEED * SHOT_SPEED;   // < 0: shots outrun rocks
    float b = 2 * (px * vx + py * vy), c = px * px + py * py;
    float disc = b * b - 4 * a * c;
    if (disc < 0) return -1;
    float t = (-b - sqrtf(disc)) / (2 * a);
    return t > 0 ? t : -1;
}

// Closest approach of a relative track (p, v): the time and the distance then
static void closest(float px, float py, float vx, float vy, float &t, float &d) {
    float vv = vx * vx + vy * vy;
    t = vv > 0.01f ? -(px * vx + py * vy) / vv : 0;
    if (t < 0) t = 0;
    float cx = px + vx * t, cy = py + vy * t;
    d = sqrtf(cx * cx + cy * cy);
}

static void autopilot(float dt) {
    thrusting = false;
    // Score every rock: the ones on a collision course come first (soonest
    // first), the rest by how long it takes to turn and get a shot there.
    // Rocks a shot already in the air will hit are left alone.
    Rock *best = nullptr, *threat = nullptr;
    float best_cost = 1e9f, best_aim = 0, threat_t = 1e9f, threat_px = 0, threat_py = 0;
    for (int i = 0; i < MAX_ROCKS; i++) {
        Rock &k = rocks[i];
        if (!k.alive) continue;
        float R = ROCK_R[k.size];
        float px = wrap_d(k.x - sx, area_w), py = wrap_d(k.y - sy, area_h);
        float vx = k.vx - svx, vy = k.vy - svy;
        float ct, cd;
        closest(px, py, vx, vy, ct, cd);
        bool danger = ct < 2.0f && cd < R + SHIP_R + 5;

        bool covered = false;
        for (int j = 0; j < MAX_SHOTS && !covered; j++) {
            const Shot &s = shots[j];
            if (!s.alive) continue;
            float st, sd;
            closest(wrap_d(k.x - s.x, area_w), wrap_d(k.y - s.y, area_h), k.vx - s.vx, k.vy - s.vy, st, sd);
            covered = st < s.life && sd < R - 0.5f;
        }
        if (covered) continue;
        if (danger && ct < threat_t) { threat_t = ct; threat = &k; threat_px = px; threat_py = py; }

        float t = intercept_t(px, py, vx, vy);
        if (t < 0) continue;
        float aim = atan2f(py + vy * t, px + vx * t);
        float cost = fabsf(angle_diff(aim, sa)) / TURN_RATE + t;
        if (t > SHOT_LIFE_S * 0.9f) cost += 3;   // out of range for now
        if (danger) cost -= 4 - ct;              // threats first, the soonest most
        if (cost < best_cost) { best_cost = cost; best = &k; best_aim = aim; }
    }

    if (threat) {
        // Can we shoot it before it gets here? If not, get out of its way:
        // thrust away from where it passes closest, or failing that along
        // whichever side-step of its track is nearer our nose
        bool shootable = threat == best && best_cost + 4 - threat_t < threat_t * 0.8f;
        if (!shootable && threat_t < 1.0f) {
            float vx = threat->vx - svx, vy = threat->vy - svy;
            float cx = threat_px + vx * threat_t, cy = threat_py + vy * threat_t;
            float side = atan2f(vy, vx) + 1.5708f;
            if (fabsf(angle_diff(side, sa)) > 1.5708f) side += 3.14159f;
            float away = atan2f(-cy, -cx);
            if (cx * cx + cy * cy > 4 && fabsf(angle_diff(away, sa)) < 1.6f) side = away;
            thrusting = fabsf(angle_diff(side, sa)) < 0.9f;
            steer(side, dt);
            return;
        }
    }
    if (!best) return;
    // Aim to within the rock's apparent size before firing
    float R = ROCK_R[best->size];
    float px = wrap_d(best->x - sx, area_w), py = wrap_d(best->y - sy, area_h);
    float dist = sqrtf(px * px + py * py) + 1;
    float tol = fmaxf(0.03f, fminf(0.12f, (R - 1) / dist));
    if (steer(best_aim, dt) && fabsf(angle_diff(best_aim, sa)) < tol && best_cost < 3) fire();
    // Close in on far-off rocks, slowly, and only with nothing on a collision course
    float speed2 = svx * svx + svy * svy;
    thrusting = !threat && dist > SHOT_SPEED * SHOT_LIFE_S * 0.7f && speed2 < 400 &&
                fabsf(angle_diff(atan2f(py, px), sa)) < 0.5f;
}

static void wrap_pos(float &x, float &y) {
    if (x < 0) x += area_w; else if (x >= area_w) x -= area_w;
    if (y < 0) y += area_h; else if (y >= area_h) y -= area_h;
}

static void update(float dt) {
    fire_cd -= dt;
    invuln  -= dt;

    if (stopping) {
        warp -= dt * 1.5f;   // ship shrinks away
        if (warp < 0) { warp = 0; ship_alive = false; }
    } else if (!ship_alive) {
        if ((ship_timer -= dt) <= 0) reset_ship();
    } else if (since_touch < AUTO_RESUME_MS) {
        float dx = finger_x - sx, dy = finger_y - sy;
        bool facing = steer(atan2f(dy, dx), dt);
        thrusting = holding && sqrtf(dx * dx + dy * dy) > 30;
        if (holding && facing) fire();
    } else {
        autopilot(dt);
    }

    if (ship_alive && !stopping) {
        if (thrusting) { svx += cosf(sa) * THRUST * dt; svy += sinf(sa) * THRUST * dt; }
        float drag = expf(-0.6f * dt);
        svx *= drag; svy *= drag;
        sx += svx * dt; sy += svy * dt;
        wrap_pos(sx, sy);
    }

    int alive_rocks = 0;
    for (int i = 0; i < MAX_ROCKS; i++) {
        Rock &k = rocks[i];
        if (!k.alive) continue;
        float boost = stopping ? 3.0f : 1.0f;   // hurry off screen when wrapping up
        k.x += k.vx * dt * boost;
        k.y += k.vy * dt * boost;
        k.a += k.spin * dt;
        float R = ROCK_R[k.size];
        if (stopping) {
            if (k.x < -R || k.x > area_w + R || k.y < -R || k.y > area_h + R) { k.alive = false; continue; }
        } else {
            wrap_pos(k.x, k.y);
        }
        alive_rocks++;

        if (ship_alive && !stopping && invuln <= 0) {
            float dx = wrap_d(k.x - sx, area_w), dy = wrap_d(k.y - sy, area_h);
            if (dx * dx + dy * dy < (R + SHIP_R - 1) * (R + SHIP_R - 1)) {
                ship_alive = false;
                ship_timer = 2.0f;
                burst(sx, sy, 12);
            }
        }
    }

    for (int i = 0; i < MAX_SHOTS; i++) {
        Shot &s = shots[i];
        if (!s.alive) continue;
        s.x += s.vx * dt; s.y += s.vy * dt;
        wrap_pos(s.x, s.y);
        if ((s.life -= dt) <= 0) { s.alive = false; continue; }
        for (int j = 0; j < MAX_ROCKS; j++) {
            Rock &k = rocks[j];
            if (!k.alive) continue;
            float dx = k.x - s.x, dy = k.y - s.y, R = ROCK_R[k.size];
            if (dx * dx + dy * dy > R * R) continue;
            s.alive = false;
            k.alive = false;
            // Copy first: spawn_rock() reuses the first free slot, which is
            // this one, so `k` stops describing the rock that was hit
            int size = k.size;
            float x = k.x, y = k.y;
            burst(x, y, 4 + 2 * (2 - size));
            if (size < 2) { spawn_rock(size + 1, x, y); spawn_rock(size + 1, x, y); }
            break;
        }
    }

    for (int i = 0; i < MAX_DEBRIS; i++) {
        Debris &d = debris[i];
        if (!d.alive) continue;
        d.x += d.vx * dt; d.y += d.vy * dt;
        if ((d.life -= dt) <= 0) d.alive = false;
    }

    if (!stopping && alive_rocks == 0) new_wave();

    if (stopping) {
        bool any = alive_rocks > 0 || ship_alive;
        for (int i = 0; i < MAX_DEBRIS; i++) any |= debris[i].alive;
        for (int i = 0; i < MAX_SHOTS; i++)  any |= shots[i].alive;
        done = !any;
    }
}

static void erase(Rect &r) {
    pixfb_fill(fb, r.x, r.y, r.w, r.h, COL_SCENE_ARCADE_BG);
    r = { 0, 0, 0, 0 };
}

static Rect poly(const float *px, const float *py, int n, lv_color_t col) {
    int x0 = 9999, y0 = 9999, x1 = -9999, y1 = -9999;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        pixfb_line(fb, (int)px[i], (int)py[i], (int)px[j], (int)py[j], col);
        if (px[i] < x0) x0 = px[i];
        if (py[i] < y0) y0 = py[i];
        if (px[i] > x1) x1 = px[i];
        if (py[i] > y1) y1 = py[i];
    }
    return { x0, y0, x1 - x0 + 1, y1 - y0 + 1 };
}

static void draw() {
    for (int i = 0; i < MAX_ROCKS; i++)  erase(rocks[i].prev);
    for (int i = 0; i < MAX_SHOTS; i++)  erase(shots[i].prev);
    for (int i = 0; i < MAX_DEBRIS; i++) erase(debris[i].prev);
    erase(prev_ship);

    for (int i = 0; i < MAX_ROCKS; i++) {
        Rock &k = rocks[i];
        if (!k.alive) continue;
        float px[ROCK_VERTS], py[ROCK_VERTS];
        for (int v = 0; v < ROCK_VERTS; v++) {
            float a = k.a + v * 6.28318f / ROCK_VERTS;
            px[v] = k.x + cosf(a) * k.r[v];
            py[v] = k.y + sinf(a) * k.r[v];
        }
        k.prev = poly(px, py, ROCK_VERTS, COL_SCENE_AST_ROCK);
    }

    for (int i = 0; i < MAX_SHOTS; i++) {
        Shot &s = shots[i];
        if (!s.alive) continue;
        pixfb_fill(fb, (int)s.x, (int)s.y, 1, 1, COL_SCENE_AST_SHOT);
        s.prev = { (int)s.x, (int)s.y, 1, 1 };
    }
    for (int i = 0; i < MAX_DEBRIS; i++) {
        Debris &d = debris[i];
        if (!d.alive) continue;
        pixfb_fill(fb, (int)d.x, (int)d.y, 1, 1, COL_SCENE_AST_DEBRIS);
        d.prev = { (int)d.x, (int)d.y, 1, 1 };
    }

    // Blink while invulnerable after a respawn
    bool visible = ship_alive && (invuln <= 0 || ((int)(invuln * 8) & 1));
    if (visible) {
        float r = SHIP_R * warp;
        float px[3] = { sx + cosf(sa) * r * 1.4f, sx + cosf(sa + 2.5f) * r, sx + cosf(sa - 2.5f) * r };
        float py[3] = { sy + sinf(sa) * r * 1.4f, sy + sinf(sa + 2.5f) * r, sy + sinf(sa - 2.5f) * r };
        prev_ship = poly(px, py, 3, COL_SCENE_AST_SHIP);
        if (thrusting && !stopping && random(0, 2)) {
            float fx = sx - cosf(sa) * r * 1.3f, fy = sy - sinf(sa) * r * 1.3f;
            pixfb_line(fb, (int)((px[1] + px[2]) / 2), (int)((py[1] + py[2]) / 2), (int)fx, (int)fy,
                       COL_SCENE_AST_THRUST);
            int x0 = (int)fminf(fx, prev_ship.x), y0 = (int)fminf(fy, prev_ship.y);
            int x1 = (int)fmaxf(fx, prev_ship.x + prev_ship.w), y1 = (int)fmaxf(fy, prev_ship.y + prev_ship.h);
            prev_ship = { x0, y0, x1 - x0 + 1, y1 - y0 + 1 };
        }
    }
}

static void tick(uint32_t dt_ms) {
    if (oom) return;
    since_touch += dt_ms;
    update(dt_ms / 1000.0f);
    draw();
    pixfb_flush(fb);
}

static void request_stop() { stopping = true; }
static bool is_done()      { return done; }

static void touch(SceneTouch type, int x, int y) {
    since_touch = 0;
    finger_x = x;
    finger_y = y;
    holding = type != SCENE_TOUCH_RELEASE;
}

static void finish() { pixfb_free(fb); }

const Scene scene_asteroids = { "asteroids", start, tick, request_stop, is_done, touch, finish, nullptr, true };
SCENE_REGISTER(scene_asteroids, "Asteroids");
