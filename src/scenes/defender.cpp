#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// Defender — Williams, 1981. A ship guarding the humanoids of a wrap-around
// planet, the scanner at the top showing all of it. Landers come down to
// grab humanoids and carry them up; one that reaches the top becomes a
// mutant. Shoot a lander that's carrying someone and they fall: catch them
// (500) and set them down (500), or they land by themselves if the drop is
// short (250) and die if it isn't. Bombers leave mines; pods burst into
// swarmers when hit; linger too long in a wave and baiters come after you.
// Lose every humanoid and the planet explodes: every lander becomes a mutant
// until the planet's rebuilt every fifth wave. A wave ends when its landers
// are gone, with a bonus for each humanoid left (100 x the wave, up to 5).
// Arcade points: lander, mutant, swarmer 150; baiter 200; bomber 250; pod
// 1000. Three ships and three smart bombs, one more of each every 10,000.
// The ship reverses with the camera sweeping it across the screen, as in the
// original, so there's room to see ahead. It plays itself. When asked to
// stop, a smart bomb clears the screen and the ship jumps to hyperspace.
// Touch: hold in the play area to fly: up or down toward your finger,
// thrusting when your finger's ahead of the ship, turning round when it's
// behind; it fires while you hold. Tap the scanner for a smart bomb. The
// computer takes back over AUTO_RESUME_MS after your last touch.
// Events: a Claude session finishing earns a smart bomb; the stars twinkle
// on the beat.

static const uint32_t AUTO_RESUME_MS = 5000;
static const int      WORLD = 2048, HUD_H = 16, MAX_ENEMY = 28, MAX_HUMANS = 10, MAX_SHOTS = 16;
static const int      MAX_LASERS = 4, MAX_PARTS = 90, MAX_STARS = 40, SCAN_W = 96;
static const float    SHIP_MAX_V = 150, SHIP_VY = 62, LASER_V = 700;

enum EType : uint8_t { E_LANDER, E_MUTANT, E_BOMBER, E_POD, E_SWARMER, E_BAITER };
enum LState : uint8_t { L_SEEK, L_DESCEND, L_RISE };
enum HState : uint8_t { H_GROUND, H_CARRIED, H_FALLING, H_RIDING, H_DEAD };
enum Phase  : uint8_t { PH_PLAY, PH_DEAD, PH_WAVE_END, PH_OVER, PH_LEAVING };

struct Enemy { bool alive; uint8_t type, state; float x, y, vx, vy, t, fire_t; int human; };
struct Human { uint8_t state; float x, y, vy, fall_from; int carrier; };
struct Shot  { bool alive, mine; float x, y, vx, vy, t; };
struct Laser { bool alive; float x, y, len, t; int dir; };
struct Part  { bool alive; float x, y, vx, vy, t; lv_color_t c; };

// Art: facing right ('1' hull, '2' cockpit, '3' trim)
static const char *const SHIP[] = { "11..........", "11111122....", ".11111111111", "1111111333..", "11.........." };
static const char *const LANDER[] = { "..111..", ".12121.", "1111111", ".11111.", "1.1.1.1", "1..1..1" };
static const char *const MUTANT[] = { "1.1.1.1", ".12221.", "1222221", ".12221.", "1.1.1.1" };
static const char *const BOMBER[] = { "111111", "1.22.1", "122221", "1.22.1", "111111" };
static const char *const POD[]    = { "1.1.1", ".121.", "12221", ".121.", "1.1.1" };
static const char *const SWARMER[] = { ".1.", "121", ".1." };
static const char *const BAITER[] = { "..111111..", ".12222221.", "1111111111", ".1.1..1.1." };
static const char *const HUMAN[]  = { ".1.", "111", ".1.", "1.1" };
static const char *const LIFE[]   = { "11...", "11111", "11..." };

static PixFb fb;
static int   W, H, play_top, scan_x;
static float ship_x, ship_y, ship_vx, cam_x, cam_off;   // cam_off: the ship's place on screen
static int   facing;                                     // 1 right, -1 left
static Enemy enemy[MAX_ENEMY];
static Human humans[MAX_HUMANS];
static Shot  shots[MAX_SHOTS];
static Laser lasers[MAX_LASERS];
static Part  parts[MAX_PARTS];
static float star_x[MAX_STARS], star_y[MAX_STARS];
static Phase phase;
static float phase_t, wave_t, fire_cool, spawn_t, flash_t, beat_t, think_t, bomb_cool, thrust_t;
static int   wave, lives, bombs, landers_left, wave_bonus;
static long  score, next_extra, hi_score;
static bool  planet_ok, stopping, done, oom;
static int   claude_was;
static uint32_t since_touch;
static bool  holding;
static float hold_x, hold_y;
static int   target_kind; static float target_x, target_y;   // the computer's current aim

static float wrapx(float x) { x = fmodf(x, WORLD); return x < 0 ? x + WORLD : x; }
static float wrapd(float d) { d = fmodf(d + WORLD / 2.0f, WORLD); if (d < 0) d += WORLD; return d - WORLD / 2.0f; }
static float screen_x(float x) { return wrapd(x - cam_x - W / 2.0f) + W / 2.0f; }   // may be off either side

static float ground_y(float x) {   // the mountains' top at world x
    if (!planet_ok) return H - 1;
    float a = x * 6.2832f / WORLD;
    float m = 5 + 4 * sinf(a * 5) + 3 * sinf(a * 13 + 1) + 2 * sinf(a * 29 + 2);
    return H - 2 - (m < 0 ? 0 : m);
}

static void add_score(long pts) {
    score += pts;
    while (score >= next_extra) { next_extra += 10000; if (lives < 9) lives++; if (bombs < 9) bombs++; }
    if (score > hi_score) hi_score = score;
}

static void burst(float x, float y, lv_color_t c, int n) {
    for (int i = 0, k = 0; i < MAX_PARTS && k < n; i++) {
        if (parts[i].alive) continue;
        float a = scene_randf(0, 6.2832f), v = scene_randf(20, 70);
        parts[i] = { true, x, y, cosf(a) * v, sinf(a) * v, 0, c };
        k++;
    }
}

static lv_color_t type_color(int t) {
    switch (t) {
        case E_LANDER:  return COL_SCENE_DEF_LANDER;
        case E_MUTANT:  return COL_SCENE_DEF_MUTANT;
        case E_BOMBER:  return COL_SCENE_DEF_BOMBER;
        case E_POD:     return COL_SCENE_DEF_POD;
        case E_SWARMER: return COL_SCENE_DEF_SWARMER;
        default:        return COL_SCENE_DEF_BAITER;
    }
}

static int spawn(uint8_t type, float x, float y) {
    for (int i = 0; i < MAX_ENEMY; i++)
        if (!enemy[i].alive) {
            float dir = random(0, 2) ? 1 : -1;
            enemy[i] = { true, type, L_SEEK, wrapx(x), y, dir * scene_randf(18, 30), 0, 0, scene_randf(1.5f, 3.5f), -1 };
            return i;
        }
    return -1;
}

static float away_from_ship() {   // a world x off screen, so things don't appear on top of you
    return wrapx(ship_x + (random(0, 2) ? 1 : -1) * scene_randf(W * 0.8f, WORLD / 2.0f));
}

static void start_wave() {
    wave++;
    wave_t = spawn_t = 0;
    landers_left = 15 + (wave > 4 ? 5 : 0);
    for (int i = 0; i < MAX_ENEMY; i++) enemy[i].alive = false;
    for (int i = 0; i < MAX_SHOTS; i++) shots[i].alive = false;
    // Every fifth wave the planet (and its humanoids) is back
    if (wave == 1 || (wave - 1) % 5 == 0) {
        planet_ok = true;
        for (int i = 0; i < MAX_HUMANS; i++) {
            float x = i * (WORLD / (float)MAX_HUMANS) + scene_randf(0, 60);
            humans[i] = { H_GROUND, x, 0, 0, 0, -1 };
            humans[i].y = ground_y(x) - 4;
        }
    }
    int bombers = 2 + wave / 2, pods = 1 + wave / 2;
    if (bombers > 5) bombers = 5;
    if (pods > 4) pods = 4;
    for (int i = 0; i < bombers; i++) spawn(E_BOMBER, away_from_ship(), scene_randf(play_top + 10, H - 25));
    for (int i = 0; i < pods; i++)    spawn(E_POD, away_from_ship(), scene_randf(play_top + 10, H - 25));
    phase = PH_PLAY;
    phase_t = 0;
}

static void reset_ship() {
    ship_y = (play_top + H) / 2.0f;
    ship_vx = 0;
    facing = 1;
    cam_off = W * 0.25f;
    cam_x = wrapx(ship_x - cam_off);
}

static void new_game() {
    score = 0;
    next_extra = 10000;
    lives = 3; bombs = 3;
    wave = 0;
    ship_x = 100;
    reset_ship();
    start_wave();
}

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_ARCADE_BG);
    done = oom;
    if (oom) return;
    play_top = HUD_H + 1;
    scan_x = (w - SCAN_W) / 2;
    // The backdrop is only the HUD's frame: everything else moves
    pixfb_fill(fb, 0, 0, w, h, COL_SCENE_ARCADE_BG);
    pixfb_fill(fb, 0, HUD_H, w, 1, COL_SCENE_DEF_FRAME);
    pixfb_fill(fb, scan_x - 1, 0, 1, HUD_H, COL_SCENE_DEF_FRAME);
    pixfb_fill(fb, scan_x + SCAN_W, 0, 1, HUD_H, COL_SCENE_DEF_FRAME);
    pixfb_touch(fb, 0, 0, w, h);
    if (!pixfb_bg_save(fb)) { done = oom = true; return; }
    for (int i = 0; i < MAX_STARS; i++) { star_x[i] = scene_randf(0, WORLD); star_y[i] = scene_randf(play_top + 2, H - 20); }
    for (int i = 0; i < MAX_PARTS; i++) parts[i].alive = false;
    for (int i = 0; i < MAX_LASERS; i++) lasers[i].alive = false;
    new_game();
    fire_cool = flash_t = beat_t = think_t = bomb_cool = thrust_t = 0;
    claude_was = scene_ctx().claude_working;
    holding = stopping = false;
    since_touch = AUTO_RESUME_MS;
}

// --- The ship's weapons ---------------------------------------------------------

static void fire() {
    if (fire_cool > 0 || phase != PH_PLAY) return;
    for (int i = 0; i < MAX_LASERS; i++)
        if (!lasers[i].alive) {
            lasers[i] = { true, wrapx(ship_x + facing * 7), ship_y + 2, 0, 0, facing };
            fire_cool = 0.16f;
            return;
        }
}

static long enemy_points(int t) {
    switch (t) { case E_BOMBER: return 250; case E_POD: return 1000; case E_BAITER: return 200; default: return 150; }
}

static void kill_enemy(int i, bool scored) {
    Enemy &e = enemy[i];
    e.alive = false;
    burst(e.x, e.y, type_color(e.type), 10);
    if (scored) add_score(enemy_points(e.type));
    if (e.type == E_LANDER && e.human >= 0) {   // whoever it carried falls
        Human &h = humans[e.human];
        if (h.state == H_CARRIED) { h.state = H_FALLING; h.vy = 0; h.fall_from = h.y; h.carrier = -1; }
    }
    if (e.type == E_POD) {   // bursts into swarmers
        int n = random(4, 7);
        for (int k = 0; k < n; k++) {
            int s = spawn(E_SWARMER, e.x + scene_randf(-6, 6), e.y + scene_randf(-5, 5));
            if (s >= 0) { enemy[s].vx = scene_randf(-60, 60); enemy[s].vy = scene_randf(-40, 40); }
        }
    }
}

static bool on_screen(float x, float margin = 0) { float sx = screen_x(x); return sx > -margin && sx < W + margin; }

static void smart_bomb() {
    if (bombs <= 0 || phase != PH_PLAY) return;
    bombs--;
    flash_t = 0.25f;
    for (int i = 0; i < MAX_ENEMY; i++) if (enemy[i].alive && on_screen(enemy[i].x)) kill_enemy(i, true);
    for (int i = 0; i < MAX_SHOTS; i++) if (shots[i].alive && on_screen(shots[i].x)) shots[i].alive = false;
}

static void enemy_fire(const Enemy &e, float speed) {
    if (!on_screen(e.x, -4)) return;   // they only shoot when you can see them
    for (int i = 0; i < MAX_SHOTS; i++)
        if (!shots[i].alive) {
            float dx = wrapd(ship_x - e.x), dy = ship_y + 2 - e.y, d = sqrtf(dx * dx + dy * dy) + 0.01f;
            // Aimed a little ahead of the ship, as the arcade's shots are
            dx += ship_vx * d / speed * 0.5f;
            d = sqrtf(dx * dx + dy * dy) + 0.01f;
            shots[i] = { true, false, e.x, e.y, dx / d * speed, dy / d * speed, 0 };
            return;
        }
}

static void drop_mine(const Enemy &e) {
    for (int i = 0; i < MAX_SHOTS; i++)
        if (!shots[i].alive) { shots[i] = { true, true, e.x, e.y + 3, 0, 0, 0 }; return; }
}

// --- The computer ---------------------------------------------------------------

static void autopilot(float dt, float &want_vy, bool &thrust, bool &want_reverse) {
    if ((think_t -= dt) <= 0) {
        think_t = 0.25f;
        // Priorities: someone falling, bringing a rescued humanoid down, a lander
        // making off with someone, then whatever's nearest
        target_kind = 0;
        float best = 1e9f;
        for (int i = 0; i < MAX_HUMANS; i++)
            if (humans[i].state == H_FALLING) {
                float d = fabsf(wrapd(humans[i].x - ship_x));
                if (d < best) { best = d; target_kind = 1; target_x = humans[i].x; target_y = humans[i].y - 4; }
            }
        for (int i = 0; i < MAX_HUMANS && !target_kind; i++)
            if (humans[i].state == H_RIDING) { target_kind = 2; target_x = ship_x + facing * 40; target_y = ground_y(ship_x) - 8; }
        best = 1e9f;
        if (!target_kind)
            for (int i = 0; i < MAX_ENEMY; i++) {
                const Enemy &e = enemy[i];
                if (!e.alive) continue;
                float d = fabsf(wrapd(e.x - ship_x)) + fabsf(e.y - ship_y) * 2;
                if (e.type == E_LANDER && e.state == L_RISE) d *= 0.3f;   // save them first
                if (e.type == E_BAITER || e.type == E_MUTANT) d *= 0.7f;
                if (d < best) { best = d; target_kind = 3; target_x = e.x; target_y = e.y - 2; }
            }
    }
    // Keep refreshing the target's position if it's an enemy
    if (target_kind == 3) {
        float best = 1e9f;
        for (int i = 0; i < MAX_ENEMY; i++) {
            const Enemy &e = enemy[i];
            if (!e.alive) continue;
            float d = fabsf(wrapd(e.x - target_x)) + fabsf(e.y - target_y);
            if (d < best) { best = d; target_x = e.x; target_y = e.y - 2; }
        }
    }
    if (!target_kind) { thrust = true; want_vy = 0; return; }   // patrol

    float dx = wrapd(target_x - ship_x), dy = target_y - ship_y;
    int want_dir = dx >= 0 ? 1 : -1;
    bool shooting = target_kind == 3;
    float gap = shooting ? 55 : 2;   // enemies: stand off and shoot; humanoids: fly right to them
    if (want_dir != facing && fabsf(dx) > (shooting ? 20 : 6)) want_reverse = true;
    thrust = fabsf(dx) > gap + 20 || (fabsf(dx) > gap && ship_vx * facing < 40);
    if (shooting && fabsf(dx) < gap * 0.6f) thrust = false;
    want_vy = fmaxf(-SHIP_VY, fminf(SHIP_VY, dy * 6));
    // Keep clear of anything close by: off the throttle and slip past it vertically
    for (int i = 0; i < MAX_ENEMY; i++) {
        const Enemy &e = enemy[i];
        if (!e.alive) continue;
        float edx = wrapd(e.x - ship_x), edy = e.y - (ship_y + 2);
        if (fabsf(edx) < 34 && fabsf(edy) < 9) {
            if (edx * facing > -6) thrust = false;
            if (fabsf(edx) < 22) want_vy = edy > 0 ? -SHIP_VY : SHIP_VY;
        }
    }
    // Dodge anything coming close
    for (int i = 0; i < MAX_SHOTS; i++) {
        const Shot &s = shots[i];
        if (!s.alive) continue;
        float sdx = wrapd(s.x - ship_x), sdy = s.y - (ship_y + 2);
        if (fabsf(sdx) < 22 && fabsf(sdy) < 7) want_vy = sdy > 0 ? -SHIP_VY : SHIP_VY;
    }
    if (shooting && fabsf(dy) < 3 && dx * facing > 4 && dx * facing < W * 0.7f) fire();
    // A smart bomb when it's getting crowded, or something's right on top of us
    int near = 0, close = 0;
    for (int i = 0; i < MAX_ENEMY; i++) {
        if (!enemy[i].alive || !on_screen(enemy[i].x)) continue;
        near++;
        if (fabsf(wrapd(enemy[i].x - ship_x)) < 16 && fabsf(enemy[i].y - ship_y) < 10) close++;
    }
    if (bomb_cool <= 0 && (near >= 6 || close) && bombs > 0) { smart_bomb(); bomb_cool = 6; }
}

// --- Update -------------------------------------------------------------------

static void lose_ship() {
    phase = PH_DEAD;
    phase_t = 0;
    burst(ship_x, ship_y + 2, COL_SCENE_DEF_SHIP, 24);
    burst(ship_x, ship_y + 2, COL_SCENE_DEF_FLAME, 16);
    for (int i = 0; i < MAX_HUMANS; i++)   // a humanoid you were carrying falls
        if (humans[i].state == H_RIDING) { humans[i].state = H_FALLING; humans[i].vy = 0; humans[i].fall_from = humans[i].y; }
    for (int i = 0; i < MAX_SHOTS; i++) shots[i].alive = false;
}

static void update_ship(float dt) {
    bool thrust = false, reverse = false;
    float want_vy = 0;
    bool you = since_touch < AUTO_RESUME_MS;
    if (stopping) thrust = false;
    else if (you) {
        if (holding) {
            float sx = screen_x(ship_x);
            float ahead = (hold_x - sx) * facing;
            if (hold_y < play_top) {}   // on the scanner: that's the smart bomb (handled on press)
            else {
                want_vy = fmaxf(-SHIP_VY, fminf(SHIP_VY, (hold_y - (ship_y + 2)) * 6));
                if (ahead < -16) reverse = true;
                else if (ahead > 16) thrust = true;
                fire();
            }
        }
    } else autopilot(dt, want_vy, thrust, reverse);

    if (reverse) facing = -facing;
    if (thrust) { ship_vx += facing * 240 * dt; thrust_t += dt; }
    ship_vx -= ship_vx * fminf(1, (thrust ? 0.6f : 1.4f) * dt);   // drag, stronger off the throttle
    if (fabsf(ship_vx) > SHIP_MAX_V) ship_vx = ship_vx > 0 ? SHIP_MAX_V : -SHIP_MAX_V;
    ship_x = wrapx(ship_x + ship_vx * dt);
    ship_y += want_vy * dt;
    if (ship_y < play_top + 1) ship_y = play_top + 1;
    if (ship_y > H - 6) ship_y = H - 6;

    // The camera slides the ship toward a quarter of the way in from the side
    // it's leaving, so you see what's ahead: the famous reverse sweep
    float want_off = facing > 0 ? W * 0.25f : W * 0.75f;
    cam_off += (want_off - cam_off) * fminf(1, dt * 2.5f);
    cam_x = wrapx(ship_x - cam_off);
}

static void update_enemies(float dt) {
    for (int i = 0; i < MAX_ENEMY; i++) {
        Enemy &e = enemy[i];
        if (!e.alive) continue;
        e.t += dt;
        float dx = wrapd(ship_x - e.x), dy = ship_y - e.y;
        switch (e.type) {
        case E_LANDER:
            if (!planet_ok) { e.type = E_MUTANT; break; }
            if (e.state == L_SEEK) {
                // Drift along at cruising height; now and then pick a humanoid below to go for
                float cruise = (play_top + H) / 2.0f - 6;
                e.vy = (cruise - e.y) * 0.8f;
                if (e.human < 0 && random(0, 1000) < (int)(dt * 150)) {
                    int h = random(0, MAX_HUMANS);
                    bool taken = false;
                    for (int k = 0; k < MAX_ENEMY; k++) taken |= enemy[k].alive && enemy[k].human == h;
                    if (humans[h].state == H_GROUND && !taken) e.human = h;
                }
                if (e.human >= 0) {
                    const Human &h = humans[e.human];
                    if (h.state != H_GROUND) e.human = -1;
                    else {
                        float hx = wrapd(h.x - e.x);
                        e.vx = hx > 0 ? fminf(30, hx * 2 + 8) : fmaxf(-30, hx * 2 - 8);
                        if (fabsf(hx) < 2) { e.state = L_DESCEND; e.vx = 0; }
                    }
                }
            } else if (e.state == L_DESCEND) {
                const Human &h = humans[e.human];
                if (h.state != H_GROUND) { e.state = L_SEEK; e.human = -1; break; }
                e.x = h.x; e.vy = 18;
                if (e.y >= h.y - 7) { e.state = L_RISE; humans[e.human].state = H_CARRIED; humans[e.human].carrier = i; }
            } else {   // rising with someone
                e.vx = 0; e.vy = -11;
                if (e.y <= play_top + 3) {   // made it: the humanoid is lost, the lander mutates
                    humans[e.human].state = H_DEAD;
                    e.human = -1;
                    e.type = E_MUTANT;
                }
            }
            if ((e.fire_t -= dt) <= 0) { enemy_fire(e, 55); e.fire_t = scene_randf(1.5f, 3.5f); }
            break;
        case E_MUTANT:
            // Straight at you, twitching
            e.vx = (dx > 0 ? 1 : -1) * 55 + scene_randf(-30, 30);
            e.vy = fmaxf(-45, fminf(45, dy * 2)) + scene_randf(-40, 40);
            if ((e.fire_t -= dt) <= 0) { enemy_fire(e, 60); e.fire_t = scene_randf(1.2f, 2.5f); }
            break;
        case E_BOMBER:
            e.vy = sinf(e.t * 1.5f + i) * 12;
            if ((e.fire_t -= dt) <= 0) { drop_mine(e); e.fire_t = scene_randf(1.0f, 1.8f); }
            break;
        case E_POD:
            e.vx = (e.vx > 0 ? 1 : -1) * 10; e.vy = sinf(e.t + i) * 6;
            break;
        case E_SWARMER:
            e.vx += ((dx > 0 ? 1 : -1) * 85 - e.vx) * fminf(1, dt * 1.5f);
            e.vy += (fmaxf(-50, fminf(50, dy * 3)) - e.vy) * fminf(1, dt * 2);
            if ((e.fire_t -= dt) <= 0) { if (dx * e.vx > 0) enemy_fire(e, 60); e.fire_t = scene_randf(2, 4); }
            break;
        case E_BAITER:
            e.vx += ((dx > 0 ? 1 : -1) * (fabsf(ship_vx) + 35) - e.vx) * fminf(1, dt * 2);
            e.vy += (fmaxf(-50, fminf(50, dy * 3 - 10)) - e.vy) * fminf(1, dt * 2);
            if ((e.fire_t -= dt) <= 0) { enemy_fire(e, 70); e.fire_t = scene_randf(0.8f, 1.5f); }
            break;
        }
        e.x = wrapx(e.x + e.vx * dt);
        e.y += e.vy * dt;
        if (e.y < play_top + 2) { e.y = play_top + 2; e.vy = fabsf(e.vy); }
        if (e.y > H - 4)        { e.y = H - 4; e.vy = -fabsf(e.vy); }
        if (e.type == E_LANDER && e.state == L_RISE && e.human >= 0) { humans[e.human].x = e.x; humans[e.human].y = e.y + 6; }
    }
}

static void update_humans(float dt) {
    for (int i = 0; i < MAX_HUMANS; i++) {
        Human &h = humans[i];
        switch (h.state) {
        case H_GROUND:   // wander a little along the ground
            h.x = wrapx(h.x + sinf(wave_t * 0.3f + i * 1.7f) * 3 * dt);
            h.y = ground_y(h.x) - 4;
            break;
        case H_FALLING: {
            h.vy += 25 * dt;
            h.y += h.vy * dt;
            float gy = ground_y(h.x) - 4;
            // Caught?
            if (phase == PH_PLAY && fabsf(wrapd(h.x - ship_x)) < 7 && h.y > ship_y - 2 && h.y < ship_y + 7) {
                h.state = H_RIDING; add_score(500);
            } else if (h.y >= gy) {
                h.y = gy;
                if (gy - h.fall_from > 28) { h.state = H_DEAD; burst(h.x, h.y, COL_SCENE_DEF_HUMAN, 6); }
                else { h.state = H_GROUND; add_score(250); }
            }
            break;
        }
        case H_RIDING:   // hanging under the ship until it's low enough to let go
            h.x = wrapx(ship_x - facing * 2); h.y = ship_y + 5;
            if (h.y >= ground_y(h.x) - 6) { h.state = H_GROUND; h.y = ground_y(h.x) - 4; add_score(500); }
            break;
        default: break;
        }
    }
}

static void update(float dt) {
    since_touch += (uint32_t)(dt * 1000);
    if (flash_t > 0) flash_t -= dt;
    if (beat_t > 0) beat_t -= dt;
    if (fire_cool > 0) fire_cool -= dt;
    if (bomb_cool > 0) bomb_cool -= dt;
    phase_t += dt;
    wave_t += dt;

    for (int i = 0; i < MAX_PARTS; i++) {
        Part &p = parts[i];
        if (!p.alive) continue;
        p.t += dt; p.x = wrapx(p.x + p.vx * dt); p.y += p.vy * dt;
        if (p.t > 0.7f) p.alive = false;
    }
    for (int i = 0; i < MAX_LASERS; i++) {
        Laser &l = lasers[i];
        if (!l.alive) continue;
        // The beam stays on the ship's nose (at the height it was fired), so a
        // fast ship doesn't outrun its own laser
        if (l.dir == facing && phase == PH_PLAY) {
            float nose = wrapx(ship_x + facing * 7), moved = wrapd(nose - l.x) * l.dir;
            l.x = nose;
            l.len -= moved;
            if (l.len < 0) l.len = 0;
        }
        float before = l.len;
        l.len += LASER_V * dt; l.t += dt;
        if (l.len > W || l.t > 0.4f) { l.alive = false; continue; }
        // Anything the tip swept past on this line goes
        for (int k = 0; k < MAX_ENEMY; k++) {
            Enemy &e = enemy[k];
            if (!e.alive || fabsf(e.y - l.y) > 3.5f) continue;
            float along = wrapd(e.x - l.x) * l.dir;
            if (along >= before - 4 && along <= l.len + 4) { kill_enemy(k, !stopping); l.alive = false; break; }
        }
    }

    if (phase == PH_LEAVING) {   // wrapping up: the ship's gone to hyperspace
        if (phase_t > 1.2f) done = true;
        return;
    }
    if (stopping && phase != PH_LEAVING) {
        // A last smart bomb, then off into hyperspace
        flash_t = 0.25f;
        for (int i = 0; i < MAX_ENEMY; i++) if (enemy[i].alive && on_screen(enemy[i].x)) kill_enemy(i, false);
        for (int i = 0; i < MAX_SHOTS; i++) shots[i].alive = false;
        burst(ship_x, ship_y + 2, COL_SCENE_DEF_LASER_1, 20);
        phase = PH_LEAVING;
        phase_t = 0;
        return;
    }

    switch (phase) {
    case PH_DEAD:
        if (phase_t > 2) {
            if (--lives <= 0) { phase = PH_OVER; phase_t = 0; break; }
            // Respawn with the space around you cleared, as the arcade does
            for (int i = 0; i < MAX_ENEMY; i++)
                if (enemy[i].alive && fabsf(wrapd(enemy[i].x - ship_x)) < W) enemy[i].x = away_from_ship();
            reset_ship();
            phase = PH_PLAY;
        }
        return;
    case PH_OVER:
        if (phase_t > 3.5f) new_game();
        return;
    case PH_WAVE_END:
        if (phase_t > 3) start_wave();
        return;
    default: break;
    }

    update_ship(dt);
    update_enemies(dt);
    update_humans(dt);

    // Landers come in fives; baiters after a minute
    int lander_count = 0, alive = 0;
    for (int i = 0; i < MAX_ENEMY; i++) {
        if (!enemy[i].alive) continue;
        alive++;
        lander_count += enemy[i].type == E_LANDER || enemy[i].type == E_MUTANT;
    }
    if ((spawn_t -= dt) <= 0 && landers_left > 0 && lander_count < 8) {
        for (int k = 0; k < 5 && landers_left > 0; k++, landers_left--) spawn(E_LANDER, away_from_ship(), play_top + 4);
        spawn_t = 14;
    }
    if (wave_t > 60 && random(0, 1000) < (int)(dt * 100)) spawn(E_BAITER, away_from_ship(), play_top + 10);

    // Shots and mines
    for (int i = 0; i < MAX_SHOTS; i++) {
        Shot &s = shots[i];
        if (!s.alive) continue;
        s.t += dt;
        s.x = wrapx(s.x + s.vx * dt); s.y += s.vy * dt;
        if (s.t > (s.mine ? 7 : 3) || s.y < play_top || s.y > H) { s.alive = false; continue; }
        if (fabsf(wrapd(s.x - ship_x)) < 5 && fabsf(s.y - (ship_y + 2)) < 3) { s.alive = false; lose_ship(); return; }
    }
    // Collisions with the ship
    for (int i = 0; i < MAX_ENEMY; i++) {
        const Enemy &e = enemy[i];
        if (e.alive && fabsf(wrapd(e.x - ship_x)) < 6 && fabsf(e.y - (ship_y + 2)) < 4) { kill_enemy(i, true); lose_ship(); return; }
    }

    // All humanoids lost: the planet explodes
    if (planet_ok) {
        bool any = false;
        for (int i = 0; i < MAX_HUMANS; i++) any |= humans[i].state != H_DEAD;
        if (!any) {
            planet_ok = false;
            flash_t = 0.6f;
            for (int k = 0; k < 40; k++) burst(wrapx(cam_x + scene_randf(0, W)), H - scene_randf(2, 12), COL_SCENE_DEF_TERRAIN, 1);
        }
    }

    // Wave over when its landers (and what became of them) are gone
    if (landers_left == 0 && lander_count == 0) {
        int saved = 0;
        for (int i = 0; i < MAX_HUMANS; i++) saved += humans[i].state != H_DEAD;
        wave_bonus = saved * 100 * (wave < 5 ? wave : 5);
        add_score(wave_bonus);
        for (int i = 0; i < MAX_HUMANS; i++) if (humans[i].state == H_RIDING || humans[i].state == H_FALLING) { humans[i].state = H_GROUND; humans[i].y = ground_y(humans[i].x) - 4; }
        phase = PH_WAVE_END;
        phase_t = 0;
    }
    (void)alive;
}

// --- Drawing --------------------------------------------------------------------

static void draw_sprite(const char *const *art, int rows, float x, float y, lv_color_t c1, lv_color_t c2, bool flip = false) {
    float sx = screen_x(x);
    if (sx < -16 || sx > W + 16) return;
    lv_color_t pal[] = { c1, c2, c2 };
    int w = strlen(art[0]);
    pixfb_art(fb, (int)sx - w / 2, (int)y - rows / 2, art, rows, pal, flip);
}

static void draw_scanner() {
    // The whole planet squeezed into the box at the top, centred on the ship
    float scale = (float)SCAN_W / WORLD, sy = (float)(HUD_H - 3) / (H - play_top);
    auto sx = [&](float x) { return scan_x + SCAN_W / 2 + (int)(wrapd(x - ship_x) * scale); };
    auto syy = [&](float y) { return 1 + (int)((y - play_top) * sy); };
    if (planet_ok)
        for (int k = 0; k < SCAN_W; k += 2) {
            float x = ship_x + (k - SCAN_W / 2) / scale;
            pixfb_px(fb, scan_x + k, syy(ground_y(x)), COL_SCENE_DEF_TERRAIN);
        }
    // The part of the world on screen, as brackets
    int l = sx(cam_x), r = sx(cam_x + W);
    pixfb_fill(fb, l, 0, 1, 2, COL_SCENE_DEF_FRAME); pixfb_fill(fb, r, 0, 1, 2, COL_SCENE_DEF_FRAME);
    pixfb_fill(fb, l, HUD_H - 2, 1, 2, COL_SCENE_DEF_FRAME); pixfb_fill(fb, r, HUD_H - 2, 1, 2, COL_SCENE_DEF_FRAME);
    for (int i = 0; i < MAX_HUMANS; i++) if (humans[i].state != H_DEAD) pixfb_px(fb, sx(humans[i].x), syy(humans[i].y), COL_SCENE_DEF_HUMAN);
    for (int i = 0; i < MAX_ENEMY; i++) if (enemy[i].alive) pixfb_fill(fb, sx(enemy[i].x), syy(enemy[i].y), 2, 1, type_color(enemy[i].type));
    if (phase == PH_PLAY) pixfb_fill(fb, sx(ship_x) - 1, syy(ship_y), 3, 1, COL_SCENE_DEF_SHIP);
}

static void draw() {
    pixfb_bg_restore(fb, 0, 0, W, H);
    if (flash_t > 0 && fmodf(flash_t, 0.1f) < 0.05f) pixfb_fill(fb, 0, play_top, W, H - play_top, COL_SCENE_DEF_FLASH);

    // Stars (drifting at a quarter of the ship's pace) and the mountains
    for (int i = 0; i < MAX_STARS; i++) {
        int x = (int)wrapd(star_x[i] - cam_x * 0.25f);
        x = ((x % W) + W) % W;
        bool twinkle = beat_t > 0 && i % 3 == 0;
        pixfb_px(fb, x, (int)star_y[i], twinkle ? COL_SCENE_DEF_LASER_1 : COL_SCENE_DEF_STAR);
    }
    if (planet_ok) {
        int prev = (int)ground_y(cam_x);
        for (int x = 1; x < W; x++) {
            int y = (int)ground_y(cam_x + x);
            pixfb_line(fb, x - 1, prev, x, y, COL_SCENE_DEF_TERRAIN);
            prev = y;
        }
    }

    for (int i = 0; i < MAX_HUMANS; i++) {
        const Human &h = humans[i];
        if (h.state == H_DEAD) continue;
        draw_sprite(HUMAN, 4, h.x, h.y, COL_SCENE_DEF_HUMAN, COL_SCENE_DEF_HUMAN);
    }
    for (int i = 0; i < MAX_ENEMY; i++) {
        const Enemy &e = enemy[i];
        if (!e.alive) continue;
        lv_color_t c = type_color(e.type), c2 = COL_SCENE_DEF_ACCENT;
        switch (e.type) {
            case E_LANDER:  draw_sprite(LANDER, 6, e.x, e.y, c, c2); break;
            case E_MUTANT:  draw_sprite(MUTANT, 5, e.x, e.y, c, COL_SCENE_DEF_LANDER); break;
            case E_BOMBER:  draw_sprite(BOMBER, 5, e.x, e.y, c, c2); break;
            case E_POD:     draw_sprite(POD, 5, e.x, e.y, c, COL_SCENE_DEF_SWARMER); break;
            case E_SWARMER: draw_sprite(SWARMER, 3, e.x, e.y, c, c2); break;
            default:        draw_sprite(BAITER, 4, e.x, e.y, c, c2); break;
        }
    }
    for (int i = 0; i < MAX_SHOTS; i++) {
        const Shot &s = shots[i];
        if (!s.alive) continue;
        float x = screen_x(s.x);
        if (x < 0 || x >= W) continue;
        bool on = fmodf(s.t, 0.2f) < 0.1f;
        pixfb_fill(fb, (int)x, (int)s.y, 2, 2, s.mine ? (on ? COL_SCENE_DEF_BOMBER : COL_SCENE_DEF_SHOT) : COL_SCENE_DEF_SHOT);
    }
    // Lasers: a long streak with a bright tip and a flickering, coloured tail
    lv_color_t lcol[] = { COL_SCENE_DEF_LASER_1, COL_SCENE_DEF_LASER_2, COL_SCENE_DEF_LASER_3 };
    for (int i = 0; i < MAX_LASERS; i++) {
        const Laser &l = lasers[i];
        if (!l.alive) continue;
        float x0 = screen_x(l.x);
        for (int k = 0; k < (int)l.len; k += 2) {
            int x = (int)(x0 + k * l.dir);
            if (x < 0 || x >= W) break;
            lv_color_t c = k > l.len - 12 ? COL_SCENE_DEF_SHIP : lcol[((k / 6) + (int)(l.t * 30)) % 3];
            pixfb_fill(fb, x, (int)l.y, 2, 1, c);
        }
    }
    for (int i = 0; i < MAX_PARTS; i++) {
        const Part &p = parts[i];
        if (!p.alive) continue;
        float x = screen_x(p.x);
        if (x >= 0 && x < W) pixfb_px(fb, (int)x, (int)p.y, p.c);
    }
    if (phase == PH_PLAY) {
        draw_sprite(SHIP, 5, ship_x, ship_y + 2, COL_SCENE_DEF_SHIP, COL_SCENE_DEF_COCKPIT, facing < 0);
        if (thrust_t > 0 && fmodf(thrust_t, 0.1f) < 0.07f && fabsf(ship_vx) > 10) {   // the exhaust
            int fx = (int)screen_x(ship_x) - facing * 8;
            pixfb_fill(fb, facing > 0 ? fx - 3 : fx, (int)ship_y + 2, 4, 1, COL_SCENE_DEF_FLAME);
        }
    }

    // HUD: score, ships and smart bombs on the left; the scanner; the wave on the right
    lv_color_t text = COL_SCENE_DEF_TEXT;
    pixfb_number(fb, 3, 1, score, text, 6);
    lv_color_t lp[] = { COL_SCENE_DEF_SHIP };
    for (int i = 0; i < lives - 1 && i < 5; i++) pixfb_art(fb, 3 + i * 7, 10, LIFE, 3, lp);   // the ships in reserve
    for (int i = 0; i < bombs && i < 5; i++) pixfb_fill(fb, 44 + i * 4, 10, 2, 3, COL_SCENE_DEF_ACCENT);
    draw_scanner();
    int rx = scan_x + SCAN_W + 6;
    int ww = pixfb_text(fb, rx, 1, "WAVE", COL_SCENE_DEF_DIM);
    pixfb_number(fb, rx + ww + 4, 1, wave, text);
    pixfb_number(fb, rx, 9 - 1, hi_score, COL_SCENE_DEF_DIM, 6);

    // Messages
    int cy = play_top + (H - play_top) / 2 - 8;
    if (phase == PH_WAVE_END) {
        char buf[24];
        snprintf(buf, sizeof(buf), "WAVE %d COMPLETED", wave);
        pixfb_text(fb, (W - pixfb_text_width(buf)) / 2, cy, buf, text);
        snprintf(buf, sizeof(buf), "BONUS %d", wave_bonus);
        pixfb_text(fb, (W - pixfb_text_width(buf)) / 2, cy + 10, buf, COL_SCENE_DEF_ACCENT);
    }
    if (phase == PH_OVER) pixfb_text(fb, (W - pixfb_text_width("GAME OVER")) / 2, cy + 4, "GAME OVER", text);
    pixfb_touch(fb, 0, 0, W, H);
}

static void tick(uint32_t dt_ms) {
    if (oom || done) return;
    update(dt_ms / 1000.0f);
    draw();
    pixfb_flush(fb);
}

static void request_stop() { stopping = true; holding = false; }
static bool is_done()      { return done; }

static void touch(SceneTouch type, int x, int y) {
    if (stopping) return;
    since_touch = 0;
    if (type == SCENE_TOUCH_PRESS && y < play_top) { smart_bomb(); holding = false; return; }
    holding = type != SCENE_TOUCH_RELEASE;
    hold_x = x; hold_y = y;
}

static void event(const SceneEvent &e) {
    if (e.type == SCENE_EV_BEAT) beat_t = 0.12f;
    if (e.type == SCENE_EV_CLAUDE) {
        if (e.value < claude_was && !stopping && bombs < 9) bombs++;   // a session finished: a smart bomb earned
        claude_was = e.value;
    }
}

static void finish() { pixfb_free(fb); }

const Scene scene_defender = { "defender", start, tick, request_stop, is_done, touch, finish, event, true };
SCENE_REGISTER(scene_defender, "Defender");
