#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Missile Command — enemy missiles streak down toward six cities, and three
// bases fire back: each counter-missile bursts into an expanding fireball
// that takes out anything flying through it (and those go off too, so they
// chain). Some enemy missiles split in mid-air. It plays itself.
//
// Kept to the arcade rules: each base has 10 missiles a wave (the stack on
// its hill), and a base that's hit loses what it had left; only so many can
// be in the air at once; the centre base's missiles are the fast ones.
// Targets can't be set down among the cities, so your own fireballs never
// reach them. Scoring: 25 a missile, and at the end of each wave 5 for every
// missile left and 100 for every city standing, all times the wave's
// multiplier (x1 for waves 1-2, x2 for 3-4, ... up to x6). Lost cities stay
// lost, but every 10,000 points earns one back at the next wave. When the
// last city falls it's THE END: a big blast, and a new game.
//
// When asked to stop, the sky is cleared in one last chain of explosions.
// Touch: tap to fire at that spot from the nearest base with missiles left.
// The computer takes back over AUTO_RESUME_MS after your last touch.
// Events: a Claude session finishing sets off a flash that clears the sky.

static const uint32_t AUTO_RESUME_MS = 5000;
static const int      MAX_ENEMY = 16, MAX_SHOTS = 8, MAX_BLASTS = 24, CITIES = 6, BASES = 3, AMMO = 10;
static const float    SHOT_V = 120, SHOT_V_FAST = 200;   // side bases, centre base
static const float    BLAST_R = 11, BLAST_GROW = 0.45f, BLAST_HOLD = 0.25f, BLAST_FADE = 0.4f;
static const long     BONUS_CITY_EVERY = 10000;

// '1' city walls, '2' lit windows; rubble after a hit
static const char *const CITY_ART[] = {
    "...1.......1....",
    "..111..1..111...",
    "..121.111.121.1.",
    "1.111.121.111111",
    "1112111111121121",
    "1111112111111111",
};
static const char *const RUBBLE_ART[] = { "..1....1..1.....", ".111.1.111111.1.", "1111111111111111" };
static const char *const X_ART[] = { "1...1", "1...1", ".1.1.", "..1..", ".1.1.", "1...1", "1...1" };
static const char *const THE_END_ART[] = {   // 3x5 letters doubled up: THE END
    "111.1.1.111...111.1...1.11.",
    ".1..1.1.1.....1...11..1.1.1",
    ".1..111.11....11..1.1.1.1.1",
    ".1..1.1.1.....1...1..11.1.1",
    ".1..1.1.111...111.1...1.11.",
};

enum Phase { PH_PLAY, PH_TALLY, PH_END };
struct Enemy { bool alive; float sx, sy, x, y, vx, vy; int target; bool splits; float split_y; };
struct Shot  { bool alive; float sx, sy, x, y, tx, ty, vx, vy; };
struct Blast { bool alive, ours; float x, y, t; };   // ours: set off by our missiles, so it scores

static PixFb fb;
static int   W, H, ground_y, wave, to_launch, tally_city, bonus_cities;
static int   city_x[CITIES], base_x[BASES], ammo[BASES];
static bool  city_up[CITIES];
static Enemy enemy[MAX_ENEMY];
static Shot  shots[MAX_SHOTS];
static Blast blasts[MAX_BLASTS];
static float launch_acc, fire_cool, flash_t, blink_t, clear_acc, wave_t, tally_t, end_t;
static long  score, next_bonus;
static long  hi_score;   // best since the board started
static Phase phase;
static bool  stopping, done, oom;
static int   claude_was;
static uint32_t since_touch;

static int mult() { return wave >= 11 ? 6 : (wave + 1) / 2; }
static int hill_top() { return ground_y - 12; }   // where missiles leave from
static int aim_floor() { return ground_y - 20; }  // nothing can be targeted lower (keeps our blasts off the cities)

static void add_score(long pts) {
    score += pts;
    if (score >= next_bonus) { bonus_cities++; next_bonus += BONUS_CITY_EVERY; }
    if (score > hi_score) hi_score = score;
}

static void paint_backdrop() {
    pixfb_fill(fb, 0, 0, W, H, COL_SCENE_ARCADE_BG);
    // Flat ground with a steep mound for each base
    for (int x = 0; x < W; x++) {
        int y = ground_y;
        for (int b = 0; b < BASES; b++) {
            int d = abs(x - base_x[b]), top = ground_y - (14 - d);
            if (d < 14 && top < y) y = top < hill_top() ? hill_top() : top;
        }
        pixfb_fill(fb, x, y, 1, H - y, COL_SCENE_MC_GROUND);
    }
    pixfb_touch(fb, 0, 0, W, H);
    pixfb_bg_save(fb);
}

static void start_wave() {
    wave++;
    to_launch = 6 + wave * 2;
    if (to_launch > 20) to_launch = 20;
    launch_acc = 2;
    wave_t = 0;
    for (int b = 0; b < BASES; b++) ammo[b] = AMMO;   // bases are restocked every wave
    for (int i = 0; i < CITIES && bonus_cities > 0; i++)   // cities earned back return now
        if (!city_up[i]) { city_up[i] = true; bonus_cities--; }
    phase = PH_PLAY;
}

static void new_game() {
    wave = 0;
    score = 0;
    next_bonus = BONUS_CITY_EVERY;
    bonus_cities = 0;
    for (int i = 0; i < CITIES; i++) city_up[i] = true;
    start_wave();
}

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_ARCADE_BG);
    done = oom;
    if (oom) return;
    ground_y = h - 6;
    // Bases at both edges and in the middle, three cities between each pair
    base_x[0] = 12; base_x[1] = w / 2; base_x[2] = w - 13;
    for (int s = 0; s < 2; s++) {
        float lo = base_x[s] + 15, hi = base_x[s + 1] - 15;
        for (int k = 0; k < 3; k++) city_x[s * 3 + k] = (int)(lo + (hi - lo) * (k + 0.5f) / 3) - 8;
    }
    paint_backdrop();
    if (!fb.bg) { done = oom = true; return; }
    for (int i = 0; i < MAX_ENEMY; i++) enemy[i].alive = false;
    for (int i = 0; i < MAX_SHOTS; i++) shots[i].alive = false;
    for (int i = 0; i < MAX_BLASTS; i++) blasts[i].alive = false;
    new_game();
    fire_cool = flash_t = blink_t = clear_acc = 0;
    claude_was = scene_ctx().claude_working;
    stopping = false;
    since_touch = AUTO_RESUME_MS;
}

static float target_x(int t) { return t < CITIES ? city_x[t] + 8 : base_x[t - CITIES]; }
static float target_y(int t) { return t < CITIES ? ground_y - 3 : hill_top(); }

static void spawn_enemy(float sx, float sy, float speed) {
    for (int i = 0; i < MAX_ENEMY; i++) {
        Enemy &e = enemy[i];
        if (e.alive) continue;
        // Mostly go for a standing city, sometimes a base
        int t = random(0, CITIES + BASES);
        for (int k = 0; k < 6 && t < CITIES && !city_up[t]; k++) t = random(0, CITIES + BASES);
        float tx = target_x(t) + scene_randf(-3, 3), ty = target_y(t);
        float dx = tx - sx, dy = ty - sy, d = sqrtf(dx * dx + dy * dy);
        e = { true, sx, sy, sx, sy, dx / d * speed, dy / d * speed, t,
              sy < 5 && random(0, 100) < 5 + wave * 3, scene_randf(H * 0.25f, H * 0.5f) };
        return;
    }
}

static void add_blast(float x, float y, bool ours) {
    for (int i = 0; i < MAX_BLASTS; i++)
        if (!blasts[i].alive) { blasts[i] = { true, ours, x, y, 0 }; return; }
}

static float blast_radius(const Blast &b) {
    if (b.t < BLAST_GROW) return BLAST_R * b.t / BLAST_GROW;
    if (b.t < BLAST_GROW + BLAST_HOLD) return BLAST_R;
    return BLAST_R * (1 - (b.t - BLAST_GROW - BLAST_HOLD) / BLAST_FADE);
}

static int shots_in_air() {
    int n = 0;
    for (int i = 0; i < MAX_SHOTS; i++) n += shots[i].alive;
    return n;
}

static int pick_base(float tx) {   // the nearest base with missiles left, or -1
    int best = -1;
    for (int b = 0; b < BASES; b++)
        if (ammo[b] > 0 && (best < 0 || fabsf(base_x[b] - tx) < fabsf(base_x[best] - tx))) best = b;
    return best;
}

static float shot_speed(int b) { return b == 1 ? SHOT_V_FAST : SHOT_V; }

static bool fire_at(float tx, float ty) {
    if (phase != PH_PLAY) return false;
    int b = pick_base(tx);
    if (b < 0 || shots_in_air() >= MAX_SHOTS) return false;
    if (ty > aim_floor()) ty = aim_floor();
    for (int i = 0; i < MAX_SHOTS; i++) {
        Shot &s = shots[i];
        if (s.alive) continue;
        float sx = base_x[b], sy = hill_top() - 1, v = shot_speed(b);
        float dx = tx - sx, dy = ty - sy, d = sqrtf(dx * dx + dy * dy) + 0.01f;
        s = { true, sx, sy, sx, sy, tx, ty, dx / d * v, dy / d * v };
        ammo[b]--;
        return true;
    }
    return false;
}

static void autopilot(float dt) {
    if ((fire_cool -= dt) > 0) return;
    // The enemy missile that will land soonest and isn't covered yet
    int pick = -1;
    float soonest = 1e9f;
    for (int i = 0; i < MAX_ENEMY; i++) {
        const Enemy &e = enemy[i];
        if (!e.alive || e.y < H * 0.35f) continue;   // let them come a way down first
        float t = (ground_y - e.y) / e.vy;
        bool covered = false;
        for (int s = 0; s < MAX_SHOTS && !covered; s++) {
            if (!shots[s].alive) continue;
            float dx = shots[s].tx - e.x, dy = shots[s].ty - e.y;
            covered = dx * dx + dy * dy < 30 * 30;
        }
        for (int b = 0; b < MAX_BLASTS && !covered; b++) {
            if (!blasts[b].alive) continue;
            float dx = blasts[b].x - e.x, dy = blasts[b].y - e.y;
            covered = dx * dx + dy * dy < 16 * 16 && blasts[b].t < BLAST_GROW + BLAST_HOLD;
        }
        if (!covered && t < soonest) { soonest = t; pick = i; }
    }
    if (pick < 0) return;
    // Lead the target: where it will be when a shot gets there and the blast opens
    const Enemy &e = enemy[pick];
    float tx = e.x, ty = e.y;
    for (int it = 0; it < 3; it++) {
        int b = pick_base(tx);
        if (b < 0) return;
        float dx = tx - base_x[b], dy = ty - hill_top();
        float t = sqrtf(dx * dx + dy * dy) / shot_speed(b) + BLAST_GROW * 0.5f;
        tx = e.x + e.vx * t; ty = e.y + e.vy * t;
    }
    if (ty > aim_floor()) return;   // too low to reach now; leave it
    tx += scene_randf(-6, 6); ty += scene_randf(-6, 6);   // not a perfect shot
    if (fire_at(tx, ty)) fire_cool = scene_randf(0.6f, 1.1f);
}

static void update(float dt) {
    since_touch += (uint32_t)(dt * 1000);
    blink_t += dt;
    wave_t += dt;
    if (flash_t > 0) flash_t -= dt;

    // Launch this wave's missiles from the top in salvos of a few at once
    if (!stopping && phase == PH_PLAY && to_launch > 0 && (launch_acc -= dt) <= 0) {
        int n = random(2, 5);
        for (int k = 0; k < n && to_launch > 0; k++, to_launch--)
            spawn_enemy(scene_randf(4, W - 4), 0, scene_randf(9, 12) + fminf(wave, 8) * 1.2f);
        launch_acc = fmaxf(1.5f, scene_randf(2.5f, 4.5f) - wave * 0.2f);
    }

    for (int i = 0; i < MAX_ENEMY; i++) {
        Enemy &e = enemy[i];
        if (!e.alive) continue;
        e.x += e.vx * dt; e.y += e.vy * dt;
        if (e.splits && e.y >= e.split_y) {   // it splits in three
            e.splits = false;
            float sp = sqrtf(e.vx * e.vx + e.vy * e.vy);
            for (int k = 0; k < 2; k++) spawn_enemy(e.x, e.y, sp);   // the new trails start here
        }
        if (e.y >= target_y(e.target)) {
            e.alive = false;
            add_blast(e.x, e.y, false);
            if (e.target < CITIES) city_up[e.target] = false;
            else ammo[e.target - CITIES] = 0;   // a hit base loses what it had left
        }
    }

    for (int i = 0; i < MAX_SHOTS; i++) {
        Shot &s = shots[i];
        if (!s.alive) continue;
        s.x += s.vx * dt; s.y += s.vy * dt;
        if ((s.tx - s.x) * s.vx + (s.ty - s.y) * s.vy <= 0) { s.alive = false; add_blast(s.tx, s.ty, true); }
    }

    for (int i = 0; i < MAX_BLASTS; i++) {
        Blast &b = blasts[i];
        if (!b.alive) continue;
        b.t += dt;
        if (b.t > BLAST_GROW + BLAST_HOLD + BLAST_FADE) { b.alive = false; continue; }
        float r = blast_radius(b);
        for (int k = 0; k < MAX_ENEMY; k++) {   // anything flying through goes off too
            Enemy &e = enemy[k];
            if (!e.alive) continue;
            float dx = e.x - b.x, dy = e.y - b.y;
            if (dx * dx + dy * dy > r * r) continue;
            e.alive = false;
            add_blast(e.x, e.y, b.ours);
            if (b.ours && !stopping) add_score(25L * mult());
        }
    }

    bool busy = false;   // anything still in the air?
    for (int i = 0; i < MAX_ENEMY; i++) busy |= enemy[i].alive;
    for (int i = 0; i < MAX_BLASTS; i++) busy |= blasts[i].alive;
    for (int i = 0; i < MAX_SHOTS; i++) busy |= shots[i].alive;

    if (stopping) {
        // One last chain: the enemy missiles go off a few at a time
        clear_acc += dt * 8;
        while (clear_acc >= 1) {
            clear_acc -= 1;
            for (int i = 0; i < MAX_ENEMY; i++)
                if (enemy[i].alive) { enemy[i].alive = false; add_blast(enemy[i].x, enemy[i].y, false); break; }
        }
        done = !busy && (phase != PH_END || end_t > 1.5f);
        if (phase == PH_END) end_t += dt;
        return;
    }

    switch (phase) {
    case PH_PLAY:
        if (since_touch >= AUTO_RESUME_MS) autopilot(dt);
        if (to_launch == 0 && !busy) { phase = PH_TALLY; tally_t = 0; tally_city = -1; }
        break;
    case PH_TALLY:
        // Count the leftover missiles one by one, then the cities, then go on
        tally_t += dt;
        while (tally_t >= 0.06f) {
            int b = -1;
            for (int k = 0; k < BASES && b < 0; k++) if (ammo[k] > 0) b = k;
            if (b >= 0) { tally_t -= 0.06f; ammo[b]--; add_score(5L * mult()); continue; }
            if (tally_t < 0.3f) break;
            tally_t -= 0.3f;
            int next = tally_city + 1;
            while (next < CITIES && !city_up[next]) next++;
            if (next < CITIES) { tally_city = next; add_score(100L * mult()); continue; }
            if (tally_city < CITIES) { tally_city = CITIES; tally_t = -0.7f; break; }   // a beat on the total
            bool any_city = false;
            for (int k = 0; k < CITIES; k++) any_city |= city_up[k];
            if (any_city || bonus_cities > 0) start_wave();
            else { phase = PH_END; end_t = 0; }
            break;
        }
        break;
    case PH_END:
        if ((end_t += dt) > 4) new_game();
        break;
    }
}

static void draw() {
    pixfb_bg_restore(fb, 0, 0, W, H);
    lv_color_t flash = COL_SCENE_MC_FLASH;
    if (flash_t > 0) pixfb_fill(fb, 0, 0, W, aim_floor(), pixfb_mix(COL_SCENE_ARCADE_BG, flash, flash_t / 0.5f));
    lv_color_t cycle[] = { COL_SCENE_MC_BLAST_1, COL_SCENE_MC_BLAST_2, COL_SCENE_MC_BLAST_3 };

    // Cities (the one being counted blinks) and each base's stack of missiles
    lv_color_t city_pal[] = { COL_SCENE_MC_CITY, COL_SCENE_MC_WINDOW };
    lv_color_t lit_pal[] = { COL_SCENE_MC_WINDOW, COL_SCENE_MC_HEAD };
    lv_color_t rubble_pal[] = { COL_SCENE_MC_GROUND };
    for (int i = 0; i < CITIES; i++) {
        bool counting = phase == PH_TALLY && i == tally_city;
        if (city_up[i]) pixfb_art(fb, city_x[i], ground_y - 6, CITY_ART, 6, counting ? lit_pal : city_pal);
        else            pixfb_art(fb, city_x[i], ground_y - 3, RUBBLE_ART, 3, rubble_pal);
    }
    for (int b = 0; b < BASES; b++) {
        // 10 missiles in a pyramid of 1, 2, 3, 4; they're used from the top
        int slot = 0;
        for (int row = 0; row < 4; row++)
            for (int k = 0; k <= row; k++, slot++) {
                if (slot < AMMO - ammo[b]) continue;
                int x = base_x[b] - row * 2 + k * 4 - 1, y = hill_top() + 1 + row * 3;
                pixfb_fill(fb, x, y, 2, 2, COL_SCENE_MC_BASE);
            }
    }

    // Enemy trails and their blinking warheads, then our shots
    bool on = fmodf(blink_t, 0.3f) < 0.15f;
    for (int i = 0; i < MAX_ENEMY; i++) {
        const Enemy &e = enemy[i];
        if (!e.alive) continue;
        pixfb_line(fb, (int)e.sx, (int)e.sy, (int)e.x, (int)e.y, COL_SCENE_MC_ENEMY);
        pixfb_fill(fb, (int)e.x - 1, (int)e.y - 1, 2, 2, on ? COL_SCENE_MC_HEAD : COL_SCENE_MC_ENEMY);
    }
    for (int i = 0; i < MAX_SHOTS; i++) {
        const Shot &s = shots[i];
        if (!s.alive) continue;
        pixfb_line(fb, (int)s.sx, (int)s.sy, (int)s.x, (int)s.y, COL_SCENE_MC_SHOT);
        pixfb_fill(fb, (int)s.x - 1, (int)s.y - 1, 2, 2, COL_SCENE_MC_HEAD);
        // The X that marks where it's going
        int tx = (int)s.tx, ty = (int)s.ty;
        for (int k = -2; k <= 2; k++) { pixfb_px(fb, tx + k, ty + k, COL_SCENE_MC_SHOT); pixfb_px(fb, tx + k, ty - k, COL_SCENE_MC_SHOT); }
    }

    // Fireballs, flickering through the colours as in the original
    for (int i = 0; i < MAX_BLASTS; i++) {
        const Blast &b = blasts[i];
        if (!b.alive) continue;
        int r = (int)(blast_radius(b) + 0.5f);
        if (r < 1) continue;
        pixfb_fill_circle(fb, (int)b.x, (int)b.y, r, cycle[((int)(b.t * 14) + i) % 3]);
    }

    // THE END: one huge flickering blast over the city, then the words
    if (phase == PH_END) {
        float t = fminf(1, end_t / 1.2f), fade = end_t > 2.2f ? fmaxf(0, 1 - (end_t - 2.2f) / 0.8f) : 1;
        int r = (int)(H * 0.9f * t * fade);
        if (r > 0) pixfb_fill_circle(fb, W / 2, ground_y - H / 3, r, cycle[(int)(end_t * 14) % 3]);
        if (end_t > 1.2f) {
            lv_color_t pal[] = { COL_SCENE_MC_ENEMY };
            for (int dy = 0; dy < 2; dy++)   // letters drawn twice as big, pixel by pixel
                for (int dx = 0; dx < 2; dx++)
                    for (int row = 0; row < 5; row++) {
                        const char *line = THE_END_ART[row];
                        for (int c = 0; line[c]; c++)
                            if (line[c] == '1') pixfb_px(fb, W / 2 - 27 + c * 2 + dx, H / 3 + row * 2 + dy, pal[0]);
                    }
            pixfb_touch(fb, W / 2 - 28, H / 3 - 1, 58, 12);
        }
    }

    // Score top left, the best so far in the middle; "N X" (points multiplier) as a wave begins
    pixfb_number(fb, 3, 2, score, COL_SCENE_MC_ENEMY);
    pixfb_number(fb, W / 2 - pixfb_number_width(hi_score) / 2, 2, hi_score, COL_SCENE_MC_CITY);
    if (phase == PH_PLAY && wave_t < 2.5f && !stopping) {
        lv_color_t pal[] = { COL_SCENE_MC_SHOT };
        int x = W / 2 - 7, y = H / 2 - 8;
        pixfb_number(fb, x, y, mult(), COL_SCENE_MC_SHOT);
        pixfb_art(fb, x + 9, y, X_ART, 7, pal);
    }
}

static void tick(uint32_t dt_ms) {
    if (oom || done) return;
    update(dt_ms / 1000.0f);
    draw();
    pixfb_flush(fb);
}

static void request_stop() { stopping = true; }
static bool is_done()      { return done; }

static void touch(SceneTouch type, int x, int y) {
    if (type != SCENE_TOUCH_PRESS || stopping) return;
    since_touch = 0;
    fire_at(x, y);
}

static void event(const SceneEvent &e) {
    if (e.type != SCENE_EV_CLAUDE) return;
    if (e.value < claude_was && !stopping && phase == PH_PLAY) {   // a session finished: a flash clears the sky
        flash_t = 0.5f;
        for (int i = 0; i < MAX_ENEMY; i++)
            if (enemy[i].alive) { enemy[i].alive = false; add_blast(enemy[i].x, enemy[i].y, true); add_score(25L * mult()); }
    }
    claude_was = e.value;
}

static void finish() { pixfb_free(fb); }

const Scene scene_missile = { "missile", start, tick, request_stop, is_done, touch, finish, event };
SCENE_REGISTER(scene_missile, "Missile Command");
