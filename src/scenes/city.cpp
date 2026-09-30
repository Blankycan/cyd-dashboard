#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Night city — a skyline in three depths, its windows switching on and off,
// a train crossing on an elevated track, a plane blinking across the sky. The
// sky follows the real clock: day, dusk, and night (with stars), and the sun
// or moon arcs across it through the hours. Birds live on the rooftops: at dusk
// (and dawn) a murmuration wheels over the city, as night falls it settles on
// the roofs, antennas and the rail, and by day a few pigeons hop between roofs
// and now and then circle the block together. When asked to stop, the lights
// go out building by building, the birds fly off, and it's done.
// Touch: tap near birds to scatter them; tap a building to light up all its
// windows; tap the sky for a shooting star.
// Events: typing switches office lights on; a Claude session finishing sends
// a shooting star; each beat makes the flock swerve; a new hour repaints the
// sky (and sends the birds to roost, or up into the air).

static const int MAX_BUILDINGS = 40;
static const int MAX_WINDOWS   = 420;
static const int MAX_BIRDS     = 44;
static const int MAX_PERCHES   = 160;
static const float NEIGHBOUR = 20.0f, SEPARATE = 7.0f;
static const float MIN_SPEED = 22.0f, MAX_SPEED = 44.0f;

struct Building { int16_t x, top, w; uint8_t layer; };
struct Window   { int16_t x, y; uint8_t b; bool on; };

enum BirdState : uint8_t { BIRD_OFF, BIRD_FLOCK, BIRD_HOMING, BIRD_PERCHED, BIRD_LEAVING };
// timer: flocking, seconds until it heads for a perch (< 0: keep flying);
// perched, seconds until it hops to another one (< 0: stay put)
struct Bird  { float x, y, vx, vy, timer, flap; int16_t px, py, perch; uint8_t state, pw, ph; bool left, drawn_perched; };
struct Perch { int16_t x, y; bool taken, on_track; };   // top-left of a sitting bird

// Flying: wings up, wings down (5x2, centred on the bird); sitting: 4x3, facing right or left
static const uint16_t BIRD_FLY[2][2] = { { 0b10001, 0b01110 }, { 0b01110, 0b10001 } };
static const uint16_t BIRD_SIT[2][3] = { { 0b0011, 0b1111, 0b0110 }, { 0b1100, 0b1111, 0b0110 } };

static PixFb    fb;
static int      W, H, track_y;
static Building bld[MAX_BUILDINGS];
static Window   win[MAX_WINDOWS];
static int      n_bld, n_win, hour;
static float    train_x, train_wait, plane_x, plane_y, plane_wait, blink_t, toggle_acc;
static float    star_x, star_y, star_t, out_acc;
static bool     train_on, plane_on, star_on, stopping, done, oom;
static int      claude_was;   // working Claude sessions before the latest change
static int      prev_train_x = -1000, prev_plane_x = -1000, prev_plane_y, prev_star_x = -1000, prev_star_y;
static Bird     birds[MAX_BIRDS];
static Perch    perch[MAX_PERCHES];
static int      n_perch;
static float    goal_t, swerve, flock_t;   // flock_t: until the next night shuffle or daytime round

enum Sky { SKY_DAY, SKY_DUSK, SKY_NIGHT };
static Sky bird_sky;
static Sky sky_for(int h) {
    if (h < 0) return SKY_NIGHT;
    if (h >= 8 && h < 17) return SKY_DAY;
    if ((h >= 17 && h < 20) || (h >= 6 && h < 8)) return SKY_DUSK;
    return SKY_NIGHT;
}

static lv_color_t window_color(const Window &w) {
    return w.on ? COL_SCENE_CITY_LIGHT : bld[w.b].layer == 2 ? COL_SCENE_CITY_NEAR : bld[w.b].layer == 1 ? COL_SCENE_CITY_MID : COL_SCENE_CITY_FAR;
}

static void set_window(Window &w, bool on) {
    w.on = on;
    lv_color_t c = window_color(w);
    pixfb_fill(fb, w.x, w.y, 2, 1, c);
    fb.bg[w.y * W + w.x] = c;
    if (w.x + 1 < W) fb.bg[w.y * W + w.x + 1] = c;
}

static void paint_backdrop() {
    Sky sky = sky_for(hour);
    lv_color_t top = sky == SKY_DAY ? COL_SCENE_CITY_DAY_TOP : sky == SKY_DUSK ? COL_SCENE_CITY_DUSK_TOP : COL_SCENE_CITY_NIGHT_TOP;
    lv_color_t low = sky == SKY_DAY ? COL_SCENE_CITY_DAY_LOW : sky == SKY_DUSK ? COL_SCENE_CITY_DUSK_LOW : COL_SCENE_CITY_NIGHT_LOW;
    for (int y = 0; y < H; y++) pixfb_fill(fb, 0, y, W, 1, pixfb_mix(top, low, (float)y / H));
    randomSeed(W * 7 + 11);
    if (sky == SKY_NIGHT)
        for (int i = 0; i < W / 5; i++) pixfb_fill(fb, random(0, W), random(0, H / 2), 1, 1, COL_SCENE_CITY_STAR);
    // Sun by day, moon otherwise, arcing across the sky through its hours
    if (hour >= 0) {
        bool sun = sky == SKY_DAY;
        float t = sun ? (hour - 8) / 9.0f : fmodf(hour + 4.0f, 24) / 14.0f;   // 0 → 1 across its span
        t = fminf(1, fmaxf(0, t));
        int cx = (int)(W * (0.1f + 0.8f * t)), cy = (int)(H * (0.45f - 0.3f * sinf(t * 3.14159f)));
        if (sun) pixfb_glow(fb, cx, cy, 14, COL_SCENE_CITY_SUN, 0.5f);
        pixfb_fill_circle(fb, cx, cy, sun ? 5 : 4, sun ? COL_SCENE_CITY_SUN : COL_SCENE_CITY_MOON);
    }
    // Buildings, far to near
    for (int i = 0; i < n_bld; i++) {
        const Building &b = bld[i];
        lv_color_t c = b.layer == 2 ? COL_SCENE_CITY_NEAR : b.layer == 1 ? COL_SCENE_CITY_MID : COL_SCENE_CITY_FAR;
        pixfb_fill(fb, b.x, b.top, b.w, H - b.top, c);
        if (b.layer == 2 && b.w > 10 && i % 3 == 0) pixfb_fill(fb, b.x + b.w / 2, b.top - 4, 1, 4, c);   // antenna
    }
    // The elevated track in front of everything
    pixfb_fill(fb, 0, track_y, W, 1, COL_SCENE_CITY_TRACK);
    for (int x = 4; x < W; x += 24) pixfb_fill(fb, x, track_y, 2, H - track_y, COL_SCENE_CITY_TRACK);
    randomSeed(esp_random());
    for (int i = 0; i < n_win; i++) {   // windows go straight into the backdrop
        lv_color_t c = window_color(win[i]);
        pixfb_fill(fb, win[i].x, win[i].y, 2, 1, c);
    }
    pixfb_bg_save(fb);
}

static void build_city() {
    randomSeed(W * 131 + H);
    n_bld = n_win = 0;
    for (int layer = 0; layer < 3; layer++) {
        int x = -random(0, 8);
        while (x < W && n_bld < MAX_BUILDINGS) {
            int w = random(8, 20 + layer * 4);
            int hgt = layer == 0 ? random(H / 3, H * 2 / 3) : layer == 1 ? random(H / 4, H / 2) : random(H / 6, H * 2 / 5);
            Building &b = bld[n_bld];
            b = { (int16_t)x, (int16_t)(H - hgt), (int16_t)w, (uint8_t)layer };
            // A grid of windows, sparser in the distance
            int step_x = layer == 0 ? 5 : 4, step_y = layer == 0 ? 5 : 4;
            for (int wy = b.top + 3; wy < track_y - 1; wy += step_y)
                for (int wx = x + 2; wx < x + w - 2; wx += step_x)
                    if (n_win < MAX_WINDOWS && wx >= 0 && wx + 1 < W && random(0, 100) < 70)
                        win[n_win++] = { (int16_t)wx, (int16_t)wy, (uint8_t)n_bld, false };
            n_bld++;
            x += w + random(-2, 3);
        }
    }
    // Drop windows that a nearer building stands in front of
    int kept = 0;
    for (int i = 0; i < n_win; i++) {
        const Window &w = win[i];
        bool hidden = false;
        for (int j = 0; j < n_bld && !hidden; j++) {
            const Building &b = bld[j];
            hidden = b.layer > bld[w.b].layer && w.x + 1 >= b.x && w.x < b.x + b.w && w.y >= b.top;
        }
        if (!hidden) win[kept++] = w;
    }
    n_win = kept;
    // Perches: along the skyline (roofs with open sky behind them, so the birds
    // read as silhouettes), on the antenna tips, and along the rail
    n_perch = 0;
    for (int i = 0; i < n_bld; i++) {
        const Building &b = bld[i];
        if (b.layer == 2 && b.w > 10 && i % 3 == 0 && n_perch < MAX_PERCHES) {
            int ax = b.x + b.w / 2;
            bool backed = false;
            for (int j = 0; j < n_bld && !backed; j++)
                backed = j != i && ax + 2 >= bld[j].x && ax - 1 < bld[j].x + bld[j].w && bld[j].top < b.top - 4;
            if (!backed) perch[n_perch++] = { (int16_t)(ax - 1), (int16_t)(b.top - 7), false, false };
        }
        for (int x = b.x + 1; x + 4 <= b.x + b.w && n_perch < MAX_PERCHES; x += 6) {
            if (x < 0 || x + 4 > W) continue;
            bool hidden = false;
            for (int j = 0; j < n_bld && !hidden; j++) {
                const Building &o = bld[j];
                hidden = j != i && x + 4 > o.x && x < o.x + o.w && (o.top < b.top || (o.top == b.top && o.layer > b.layer));
            }
            if (!hidden) perch[n_perch++] = { (int16_t)x, (int16_t)(b.top - 3), false, false };
        }
    }
    for (int x = 3; x + 4 <= W && n_perch < MAX_PERCHES; x += 9)
        perch[n_perch++] = { (int16_t)x, (int16_t)(track_y - 3), false, true };
    randomSeed(esp_random());
}

// ── Birds ────────────────────────────────────────────────────────────────

static int pick_perch() {   // a random free perch, or -1
    if (!n_perch) return -1;
    // Mostly the skyline, where they stand out against the sky; the rail now and then
    bool rail_ok = random(0, 5) == 0;
    int start = random(0, n_perch);
    for (int k = 0; k < n_perch; k++) {
        int p = (start + k * 7) % n_perch;   // strides of 7 spread the flock over the city
        if (!perch[p].taken && (rail_ok || !perch[p].on_track)) return p;
    }
    for (int p = 0; p < n_perch; p++) if (!perch[p].taken) return p;   // in case the strides missed some
    return -1;
}

static void release_perch(Bird &b) {
    if (b.perch >= 0) perch[b.perch].taken = false;
    b.perch = -1;
}

static void leave(Bird &b) { release_perch(b); b.state = BIRD_LEAVING; }

static void go_home(Bird &b) {   // head for a free perch, or out of town if there's none
    release_perch(b);
    int p = pick_perch();
    if (p < 0) { leave(b); return; }
    perch[p].taken = true;
    b.perch = p;
    b.state = BIRD_HOMING;
}

static void take_off(Bird &b) {
    release_perch(b);
    b.state = BIRD_FLOCK;
    b.vx = scene_randf(-20, 20); b.vy = scene_randf(-35, -25);
    b.timer = bird_sky == SKY_DUSK ? -1 : scene_randf(5, 9);
}

static void land(Bird &b) {
    b.state = BIRD_PERCHED;
    b.left = b.vx < 0;
    b.vx = b.vy = 0;
    b.x = perch[b.perch].x + 2; b.y = perch[b.perch].y + 1;
    b.timer = bird_sky == SKY_DAY ? scene_randf(4, 25) : -1;
}

// How many birds each sky has, and what they're doing. `instant` places them
// straight away (at the start); otherwise they fly off, in, or down to roost.
static void set_bird_sky(Sky s, bool instant) {
    bird_sky = s;
    int want = s == SKY_DUSK ? MAX_BIRDS : s == SKY_NIGHT ? 30 : 8;
    flock_t = scene_randf(10, 30);
    for (int i = 0; i < MAX_BIRDS; i++) {
        Bird &b = birds[i];
        b.drawn_perched = false;
        if (instant) {
            b = { 0, 0, 0, 0, -1, scene_randf(0, 1), -1000, 0, -1, BIRD_OFF, 0, 0, random(0, 2) == 0, false };
            if (i >= want) continue;
            if (s == SKY_DUSK) {   // fly in from the left as a loose cloud
                b.x = scene_randf(-50, -6); b.y = scene_randf(H * 0.1f, H * 0.4f);
                b.vx = scene_randf(25, 35); b.vy = scene_randf(-5, 5);
                b.state = BIRD_FLOCK;
            } else {
                int p = pick_perch();
                if (p < 0) continue;
                perch[p].taken = true; b.perch = p;
                land(b);
                b.left = random(0, 2) == 0;
            }
            continue;
        }
        if (i >= want) { if (b.state != BIRD_OFF) leave(b); continue; }
        if (s == SKY_DUSK) {
            if (b.state == BIRD_OFF) {   // newcomers arrive from either side
                bool from_left = random(0, 2) == 0;
                b.x = from_left ? scene_randf(-40, -6) : scene_randf(W + 6, W + 40); b.y = scene_randf(H * 0.1f, H * 0.4f);
                b.vx = from_left ? 30 : -30; b.vy = 0;
                b.px = -1000; b.perch = -1;
            }
            if (b.state == BIRD_PERCHED || b.state == BIRD_HOMING) take_off(b);
            b.state = BIRD_FLOCK; b.timer = -1;
        } else if (b.state == BIRD_FLOCK || b.state == BIRD_LEAVING) {
            b.state = BIRD_FLOCK; b.timer = scene_randf(0.5f, 12);   // they come down a few at a time
        } else if (b.state == BIRD_PERCHED) {
            b.timer = s == SKY_DAY ? scene_randf(4, 25) : -1;
        }
    }
}

static bool scare_birds(int x, int y) {   // true if any bird was near enough to startle
    bool hit = false;
    for (int i = 0; i < MAX_BIRDS; i++) {
        Bird &b = birds[i];
        if (b.state == BIRD_OFF || b.state == BIRD_LEAVING) continue;
        float dx = b.x - x, dy = b.y - y, d2 = dx * dx + dy * dy;
        if (d2 > 26 * 26) continue;
        hit = true;
        if (b.state != BIRD_FLOCK) take_off(b);
        float d = sqrtf(d2) + 0.5f;
        b.vx += dx / d * 60; b.vy += dy / d * 60 - 15;
    }
    if (hit) swerve = fminf(2.5f, swerve + 1);
    return hit;
}

static void tick_birds(float dt) {
    // The point the flock chases: a slow figure-of-eight over the rooftops
    goal_t += dt * (0.35f + swerve);
    swerve *= expf(-2.0f * dt);
    float gx = W * (0.5f + 0.38f * sinf(goal_t)), gy = H * (0.26f + 0.12f * sinf(goal_t * 2.1f));

    // Now and then: at night a few birds shuffle to new spots; by day the
    // pigeons all go up and circle the block
    if (!stopping && bird_sky != SKY_DUSK && (flock_t -= dt) <= 0) {
        flock_t = bird_sky == SKY_NIGHT ? scene_randf(15, 35) : scene_randf(35, 70);
        int pick = random(0, MAX_BIRDS);
        const Bird &c = birds[pick];
        for (int i = 0; i < MAX_BIRDS; i++) {
            Bird &b = birds[i];
            if (b.state != BIRD_PERCHED) continue;
            float dx = b.x - c.x, dy = b.y - c.y;
            if (bird_sky == SKY_DAY || (c.state == BIRD_PERCHED && dx * dx + dy * dy < 24 * 24)) take_off(b);
        }
    }

    // The train startles birds sitting on the rail just ahead of it
    if (train_on) {
        float front = train_x + 5 * 15;
        for (int i = 0; i < MAX_BIRDS; i++) {
            Bird &b = birds[i];
            if (b.state == BIRD_PERCHED && perch[b.perch].on_track && b.x > front - 4 && b.x < front + 28) take_off(b);
        }
    }

    for (int i = 0; i < MAX_BIRDS; i++) {
        Bird &b = birds[i];
        switch (b.state) {
        case BIRD_OFF: case BIRD_PERCHED:
            if (b.state == BIRD_PERCHED && b.timer > 0 && (b.timer -= dt) <= 0) {   // a pigeon hops to another roof
                b.vy = -30; b.vx = scene_randf(-15, 15);
                go_home(b);
            }
            continue;
        case BIRD_FLOCK: {
            float cx = 0, cy = 0, ax = 0, ay = 0, sx = 0, sy = 0;
            int count = 0;
            for (int j = 0; j < MAX_BIRDS; j++) {
                const Bird &o = birds[j];
                if (j == i || o.state != BIRD_FLOCK) continue;
                float dx = o.x - b.x, dy = o.y - b.y;
                if (fabsf(dx) > NEIGHBOUR || fabsf(dy) > NEIGHBOUR) continue;
                float d2 = dx * dx + dy * dy;
                if (d2 > NEIGHBOUR * NEIGHBOUR) continue;
                cx += o.x; cy += o.y; ax += o.vx; ay += o.vy; count++;
                if (d2 < SEPARATE * SEPARATE && d2 > 0.01f) { sx -= dx / d2; sy -= dy / d2; }
            }
            float fx = 0, fy = 0;
            if (count) {
                fx += (cx / count - b.x) * 0.9f + (ax / count - b.vx) * 0.8f;   // stay with them, match them
                fy += (cy / count - b.y) * 0.9f + (ay / count - b.vy) * 0.8f;
            }
            fx += sx * 90; fy += sy * 90;                                   // but not too close
            fx += (gx - b.x) * 0.35f; fy += (gy - b.y) * 0.35f;             // follow the wandering point
            if (b.y > H * 0.55f) fy -= (b.y - H * 0.55f) * 6;               // keep above the streets
            if (b.y < 3) fy += (3 - b.y) * 6;
            b.vx += fx * dt; b.vy += fy * dt;
            float sp = sqrtf(b.vx * b.vx + b.vy * b.vy);
            float lim = sp > MAX_SPEED ? MAX_SPEED / sp : sp < MIN_SPEED && sp > 0.01f ? MIN_SPEED / sp : 1;
            b.vx *= lim; b.vy *= lim;
            if (b.timer > 0 && (b.timer -= dt) <= 0) go_home(b);
            break;
        }
        case BIRD_HOMING: case BIRD_LEAVING: {
            float tx, ty;
            if (b.state == BIRD_HOMING) { tx = perch[b.perch].x + 2; ty = perch[b.perch].y + 1; }
            else { tx = b.x > W / 2 ? W + 40 : -40; ty = -30; }
            float dx = tx - b.x, dy = ty - b.y, d = sqrtf(dx * dx + dy * dy);
            if (b.state == BIRD_HOMING && d < 1.5f) { land(b); continue; }
            if (b.state == BIRD_HOMING && d < 8) { b.vx = dx * 5; b.vy = dy * 5; }   // flutter the last bit straight in
            else {
                float sp = b.state == BIRD_LEAVING ? MAX_SPEED : fminf(MAX_SPEED, 12 + d * 1.5f);
                float k = fminf(1, 2.5f * dt);
                b.vx += (dx / d * sp - b.vx) * k; b.vy += (dy / d * sp - b.vy) * k;
            }
            break;
        }
        }
        b.x += b.vx * dt; b.y += b.vy * dt;
        b.flap += dt * 5;
        if (b.state == BIRD_LEAVING && (b.x < -8 || b.x > W + 8 || b.y < -8)) b.state = BIRD_OFF;
    }
}

static void erase_birds() {   // sitting birds stay drawn; everything else is wiped for redrawing
    for (int i = 0; i < MAX_BIRDS; i++) {
        Bird &b = birds[i];
        if (b.px <= -1000 || (b.state == BIRD_PERCHED && b.drawn_perched)) continue;
        pixfb_bg_restore(fb, b.px, b.py, b.pw, b.ph);
        b.px = -1000;
    }
}

static void draw_birds() {
    lv_color_t c = COL_SCENE_CITY_BIRD;
    if (bird_sky == SKY_NIGHT) c = pixfb_mix(c, COL_SCENE_CITY_MOON, 0.4f);   // caught in the city's glow
    for (int i = 0; i < MAX_BIRDS; i++) {
        Bird &b = birds[i];
        if (b.state == BIRD_OFF) continue;
        if (b.state == BIRD_PERCHED) {
            const Perch &p = perch[b.perch];
            if (b.drawn_perched) {
                // Unchanged: rewrite the pixels (something may have been drawn
                // over them) without marking them dirty again
                for (int r = 0; r < 3; r++)
                    for (int col = 0; col < 4; col++)
                        if (BIRD_SIT[b.left][r] & (1u << (3 - col))) pixfb_px(fb, p.x + col, p.y + r, c);
            } else pixfb_sprite(fb, p.x, p.y, BIRD_SIT[b.left], 4, 3, c);
            b.drawn_perched = true;
            b.px = p.x; b.py = p.y; b.pw = 4; b.ph = 3;
        } else {
            int x = (int)b.x - 2, y = (int)b.y - 1;
            pixfb_sprite(fb, x, y, BIRD_FLY[(int)b.flap & 1], 5, 2, c);
            b.drawn_perched = false;
            b.px = x; b.py = y; b.pw = 5; b.ph = 2;
        }
    }
}

static bool birds_gone() {
    for (int i = 0; i < MAX_BIRDS; i++) if (birds[i].state != BIRD_OFF) return false;
    return true;
}

static float lit_share() {   // share of windows on at this hour
    Sky s = sky_for(hour);
    if (s == SKY_DAY) return 0.12f;
    if (s == SKY_DUSK) return 0.55f;
    return (hour >= 0 && hour < 5) ? 0.2f : 0.4f;
}

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_CITY_NIGHT_TOP);
    done = oom;
    if (oom) return;
    track_y = h - 12;
    hour = scene_ctx().hour;
    build_city();
    float share = lit_share();
    for (int i = 0; i < n_win; i++) win[i].on = scene_randf(0, 1) < share;
    paint_backdrop();
    if (!fb.bg) { done = oom = true; return; }
    train_on = plane_on = star_on = false;
    train_wait = scene_randf(3, 10);
    plane_wait = scene_randf(8, 20);
    toggle_acc = out_acc = blink_t = 0;
    stopping = false;
    prev_train_x = prev_plane_x = prev_star_x = -1000;
    claude_was = scene_ctx().claude_working;
    goal_t = scene_randf(0, 6.28f);
    swerve = 0;
    set_bird_sky(sky_for(hour), true);
}

static void shooting_star(float x, float y) {
    star_on = true; star_x = x; star_y = y; star_t = 0;
}

static void tick_frame(uint32_t dt_ms) {
    float dt = dt_ms / 1000.0f;
    blink_t += dt;
    erase_birds();

    // Windows switch on and off over time (or all go out when stopping)
    if (stopping) {
        out_acc += dt * 250;
        while (out_acc >= 1) {
            out_acc -= 1;
            for (int tries = 0; tries < 20; tries++) {
                Window &w = win[random(0, n_win)];
                if (w.on) { set_window(w, false); break; }
            }
        }
        bool any = false;
        for (int i = 0; i < n_win && !any; i++) any = win[i].on;
        done = !any && !train_on && !plane_on && !star_on && birds_gone();
    } else {
        toggle_acc += dt * 3;
        while (toggle_acc >= 1) {
            toggle_acc -= 1;
            Window &w = win[random(0, n_win)];
            set_window(w, scene_randf(0, 1) < lit_share());
        }
    }

    // Train: a row of lit carriages sliding along the track
    const int CARS = 5, CAR_W = 14;
    if (prev_train_x > -1000) pixfb_bg_restore(fb, prev_train_x, track_y - 5, CARS * (CAR_W + 1) + 1, 5);
    prev_train_x = -1000;
    if (!train_on && !stopping && (train_wait -= dt) <= 0) { train_on = true; train_x = -CARS * (CAR_W + 1); }
    if (train_on) {
        train_x += 38 * dt;
        if (train_x > W) { train_on = false; train_wait = scene_randf(15, 35); }
        else {
            int x0 = (int)train_x;
            for (int c = 0; c < CARS; c++) {
                int x = x0 + c * (CAR_W + 1);
                pixfb_fill(fb, x, track_y - 5, CAR_W, 5, COL_SCENE_CITY_TRAIN);
                for (int wx = x + 2; wx < x + CAR_W - 2; wx += 3) pixfb_fill(fb, wx, track_y - 4, 2, 2, COL_SCENE_CITY_LIGHT);
            }
            prev_train_x = x0;
        }
    }

    // Plane: two blinking lights crossing high up
    if (prev_plane_x > -1000) pixfb_bg_restore(fb, prev_plane_x, prev_plane_y, 4, 1);
    prev_plane_x = -1000;
    if (!plane_on && !stopping && (plane_wait -= dt) <= 0) { plane_on = true; plane_x = W + 4; plane_y = scene_randf(4, H / 4); }
    if (plane_on) {
        plane_x -= (stopping ? 60 : 14) * dt;   // hurries off when the scene is wrapping up
        if (plane_x < -4) { plane_on = false; plane_wait = scene_randf(20, 40); }
        else {
            int x = (int)plane_x, y = (int)plane_y;
            pixfb_fill(fb, x, y, 1, 1, COL_SCENE_CITY_PLANE);
            if (fmodf(blink_t, 1.0f) < 0.5f) pixfb_fill(fb, x + 3, y, 1, 1, COL_SCENE_CITY_BLINK);
            prev_plane_x = x; prev_plane_y = y;
        }
    }

    // Shooting star: a short streak racing down-left, fading out
    if (prev_star_x > -1000) pixfb_bg_restore(fb, prev_star_x, prev_star_y, 10, 6);
    prev_star_x = -1000;
    if (star_on) {
        star_t += dt;
        if (star_t > 0.6f) star_on = false;
        else {
            float x = star_x - star_t * 90, y = star_y + star_t * 45;
            lv_color_t c = pixfb_mix(COL_SCENE_CITY_STAR, COL_SCENE_CITY_NIGHT_TOP, star_t / 0.6f);
            pixfb_line(fb, (int)x, (int)y, (int)x + 8, (int)y - 4, c);
            prev_star_x = (int)x; prev_star_y = (int)y - 4;
        }
    }

    // Birds last, in front of everything
    tick_birds(dt);
    draw_birds();
}

static void tick(uint32_t dt_ms) {
    if (oom || done) return;
    tick_frame(dt_ms);
    pixfb_flush(fb);
}

static void request_stop() {
    stopping = true;
    for (int i = 0; i < MAX_BIRDS; i++) if (birds[i].state != BIRD_OFF) leave(birds[i]);
}
static bool is_done()      { return done; }

static void touch(SceneTouch type, int x, int y) {
    if (type != SCENE_TOUCH_PRESS || stopping) return;
    if (scare_birds(x, y)) return;
    // Nearest building under the finger (front first), else the sky
    for (int layer = 2; layer >= 0; layer--)
        for (int i = 0; i < n_bld; i++) {
            const Building &b = bld[i];
            if (b.layer != layer || x < b.x || x >= b.x + b.w || y < b.top) continue;
            for (int k = 0; k < n_win; k++) if (win[k].b == i) set_window(win[k], true);
            return;
        }
    shooting_star(x, y);
}

static void event(const SceneEvent &e) {
    if (stopping) return;
    if (e.type == SCENE_EV_KEY && e.key == SCENE_KEY_CHAR) set_window(win[random(0, n_win)], true);
    if (e.type == SCENE_EV_CLAUDE) {   // a session finished
        if (e.value < claude_was) shooting_star(scene_randf(W * 0.4f, W), scene_randf(4, H / 4));
        claude_was = e.value;
    }
    if (e.type == SCENE_EV_BEAT) swerve = fminf(2.5f, swerve + 0.4f + e.strength / 100.0f);
    if (e.type == SCENE_EV_HOUR) {
        Sky was = sky_for(hour);
        hour = e.value;
        paint_backdrop();   // wipes the sitting birds too; set_bird_sky marks them for a full redraw
        if (sky_for(hour) != was) set_bird_sky(sky_for(hour), false);
        else for (int i = 0; i < MAX_BIRDS; i++) birds[i].drawn_perched = false;
    }
}

static void finish() { pixfb_free(fb); }

const Scene scene_city = { "city", start, tick, request_stop, is_done, touch, finish, event };
