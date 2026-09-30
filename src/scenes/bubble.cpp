#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>
#include <string.h>

// Bubble shooter — Puzzle Bobble's rules, turned on its side to suit the
// wide screen (so the bubbles can be bigger): a honeycomb of coloured bubbles
// hangs from the "ceiling" on the right, the launcher on the left fires one
// bubble at a time (bouncing off the top and bottom walls), and three or more of a colour touching
// pop. Anything left hanging by nothing falls. Popped bubbles are 10 points
// each; dropped ones are worth 20 for one, doubling for each more (10 x 2^n).
// The next bubble waits beside the launcher, and it's always a colour that's
// still in play. Every 8 shots the ceiling comes down a row (it shakes for
// the last two); a bubble past the dotted line ends the game. Clear
// the field for the next round. It plays itself, aiming for pops and drops,
// and now and then misses its mark. When asked to stop, the whole field
// drops.
// Touch: drag to aim, let go to fire. The computer takes back over
// AUTO_RESUME_MS after your last touch.
// Events: with music playing the launcher glows on the beat; a Claude
// session finishing loads a star bubble (from Puzzle Bobble 2), which clears
// every bubble of the colour it hits.

static const uint32_t AUTO_RESUME_MS = 5000;
static const int      COLS = 8, MAX_ROWS = 24, D = 11, COLORS = 6, STAR = 7, SHOTS_PER_DROP = 8;
static const float    RH = 9.53f;   // row pitch: D * sqrt(3) / 2, so the rows nest

// The game runs in its own upright coordinates (x across the 8 lanes, y down
// from the ceiling toward the launcher), and is drawn turned a quarter-turn:
// the ceiling on the right, the launcher on the left.
static const int      MAX_FX = 64;
static const float    SHOT_V = 230, AIM_V = 1.6f, MAX_AIM = 1.35f;   // radians a second; ~77 degrees

struct Fx { bool alive, falling; float x, y, vy, t; uint8_t color; };   // a pop, or a bubble dropping away

static PixFb    fb;
static int      W, H, field_x, launch_x, launch_y, danger_y, rows;   // game coordinates
static int      ceil_sx, lane_sy;   // on screen: where the ceiling is, and the top of the lanes
static int      start_rows;         // how deep a new round's pattern is
static uint8_t  grid[MAX_ROWS][COLS];      // 0 empty, 1..COLORS, STAR
static int      drops, shots, round_no, cur_color, next_color;
static long     score, hi_score;           // hi: best since the board started
static float    aim, goal_aim, aim_wait, over_t, beat_t, shake_t;
static bool     shot_on, over, stopping, done, oom;
static float    shx, shy, svx, svy;   // the shot, in game coordinates
static uint8_t  shot_color;
static Fx       fx[MAX_FX];
static int      claude_was;
static bool     star_next;
static uint32_t since_touch;

static int  row_len(int r) { return r & 1 ? COLS - 1 : COLS; }
static int  top_y()        { return 1 + drops * RH; }
static float cell_x(int r, int c) { return field_x + D / 2.0f + c * D + (r & 1 ? D / 2.0f : 0); }
static float cell_y(int r)        { return top_y() + D / 2.0f + r * RH; }
static float scr_x(float gy)      { return ceil_sx - gy; }   // game to screen
static float scr_y(float gx)      { return lane_sy + gx; }

static lv_color_t bubble_color(int c) {
    switch (c) {
        case 1: return COL_SCENE_BUB_1;
        case 2: return COL_SCENE_BUB_2;
        case 3: return COL_SCENE_BUB_3;
        case 4: return COL_SCENE_BUB_4;
        case 5: return COL_SCENE_BUB_5;
        case 6: return COL_SCENE_BUB_6;
        default: return COL_SCENE_BUB_STAR;
    }
}

// Hex neighbours: same row either side, and two in the rows above and below
// (which two depends on whether this row is shifted right)
static int neighbours(int r, int c, int out[6][2]) {
    int n = 0;
    auto add = [&](int rr, int cc) { if (rr >= 0 && rr < rows && cc >= 0 && cc < row_len(rr)) { out[n][0] = rr; out[n][1] = cc; n++; } };
    add(r, c - 1); add(r, c + 1);
    int a = r & 1 ? c : c - 1, b = r & 1 ? c + 1 : c;
    add(r - 1, a); add(r - 1, b); add(r + 1, a); add(r + 1, b);
    return n;
}

static int colors_in_play(uint8_t list[COLORS]) {
    bool have[COLORS + 1] = {};
    for (int r = 0; r < rows; r++) for (int c = 0; c < row_len(r); c++) if (grid[r][c] >= 1 && grid[r][c] <= COLORS) have[grid[r][c]] = true;
    int n = 0;
    for (int k = 1; k <= COLORS; k++) if (have[k]) list[n++] = k;
    return n;
}

static int random_color_in_play() {
    uint8_t list[COLORS];
    int n = colors_in_play(list);
    return n ? list[random(0, n)] : random(1, COLORS + 1);
}

static void plan();

static void new_round() {
    // A symmetric pattern of clusters, half the field deep; more colours as the rounds go on
    memset(grid, 0, sizeof(grid));
    drops = shots = 0;
    int ncol = 4 + round_no / 2;
    if (ncol > COLORS) ncol = COLORS;
    for (int r = 0; r < start_rows; r++)
        for (int c = 0; c < (row_len(r) + 1) / 2; c++) {
            int color = random(1, ncol + 1);
            if (c > 0 && random(0, 100) < 35) color = grid[r][c - 1];            // grow clusters sideways
            else if (r > 0 && random(0, 100) < 30) color = grid[r - 1][c];       // and down
            grid[r][c] = color;
            grid[r][row_len(r) - 1 - c] = color;                                  // mirrored
        }
    cur_color = random_color_in_play();
    next_color = random_color_in_play();
    over = false;
    plan();
}

static void new_game() {
    score = 0;
    round_no = 1;
    new_round();
}

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_BUB_BG);
    done = oom;
    if (oom) return;
    field_x = 0;
    launch_x = COLS * D / 2;
    // The field spans the whole width: the ceiling at the right edge, the
    // launcher at the left, the dotted line just in front of it
    ceil_sx = w - 3;
    lane_sy = (h - COLS * D) / 2;
    launch_y = ceil_sx - 9;
    danger_y = launch_y - 12;
    rows = (int)((danger_y - 1 - D / 2.0f) / RH) + 2;   // one past the line, to tell when it's crossed
    if (rows > MAX_ROWS) rows = MAX_ROWS;
    start_rows = rows / 2;
    pixfb_fill(fb, 0, 0, w, h, COL_SCENE_BUB_FIELD);
    pixfb_fill(fb, 0, lane_sy - 1, ceil_sx, 1, COL_SCENE_BUB_WALL);
    pixfb_fill(fb, 0, lane_sy + COLS * D, ceil_sx, 1, COL_SCENE_BUB_WALL);
    pixfb_fill(fb, ceil_sx, 0, w - ceil_sx, h, COL_SCENE_BUB_WALL);
    for (int y = lane_sy; y < lane_sy + COLS * D; y += 3) pixfb_px(fb, (int)scr_x(danger_y), y, COL_SCENE_BUB_WALL);
    pixfb_touch(fb, 0, 0, w, h);
    if (!pixfb_bg_save(fb)) { done = oom = true; return; }
    for (int i = 0; i < MAX_FX; i++) fx[i].alive = false;
    new_game();
    aim = goal_aim = 0;
    aim_wait = 0.6f;
    shot_on = false;
    beat_t = shake_t = 0;
    star_next = false;
    claude_was = scene_ctx().claude_working;
    stopping = false;
    since_touch = AUTO_RESUME_MS;
}

static void add_fx(float gx, float gy, uint8_t color, bool falling) {   // given in game coordinates, kept on screen
    for (int i = 0; i < MAX_FX; i++)
        if (!fx[i].alive) { fx[i] = { true, falling, scr_x(gy), scr_y(gx), falling ? scene_randf(-30, 0) : 0, 0, color }; return; }
}

// --- Flight and landing -------------------------------------------------------

// Is a bubble at (x, y) touching one in the grid? Only the rows it could
// reach are checked. `hit` gets the one it touched.
static bool touching(float x, float y, int &hit_r, int &hit_c) {
    hit_r = hit_c = -1;
    int r0 = (int)floorf((y - top_y() - D / 2.0f) / RH);
    for (int r = r0 - 1; r <= r0 + 2; r++) {
        if (r < 0 || r >= rows) continue;
        for (int c = 0; c < row_len(r); c++) {
            if (!grid[r][c]) continue;
            float dx = cell_x(r, c) - x, dy = cell_y(r) - y;
            if (dx * dx + dy * dy < (D - 1.5f) * (D - 1.5f)) { hit_r = r; hit_c = c; return true; }
        }
    }
    return false;
}

// Follow a shot from the launcher at `angle` to where it would stick; returns
// false if it wouldn't (it can't, really, but just in case). `hit` is the
// bubble it ran into (-1 for the ceiling).
static bool trace(float angle, int &out_r, int &out_c, int &hit_r, int &hit_c) {
    float x = launch_x, y = launch_y - 4, vx = sinf(angle), vy = -cosf(angle);
    float left = field_x + D / 2.0f, right = field_x + COLS * D - D / 2.0f;
    for (int step = 0; step < 400; step++) {
        x += vx * 1.5f; y += vy * 1.5f;
        if (x < left)  { x = 2 * left - x; vx = -vx; }
        if (x > right) { x = 2 * right - x; vx = -vx; }
        bool stuck = touching(x, y, hit_r, hit_c) || y <= top_y() + D / 2.0f;
        if (!stuck) continue;
        // Snap to the nearest free cell
        float best = 1e9f;
        out_r = -1;
        int r0 = (int)floorf((y - top_y() - D / 2.0f) / RH + 0.5f);
        for (int r = r0 - 1; r <= r0 + 1; r++) {
            if (r < 0 || r >= rows) continue;
            for (int c = 0; c < row_len(r); c++) {
                if (grid[r][c]) continue;
                float dx = cell_x(r, c) - x, dy = cell_y(r) - y, d = dx * dx + dy * dy;
                if (d < best) { best = d; out_r = r; out_c = c; }
            }
        }
        return out_r >= 0;
    }
    return false;
}

static int group(int r, int c, uint8_t color, uint8_t mark[MAX_ROWS][COLS]) {   // flood fill of one colour
    int stack[MAX_ROWS * COLS][2], sp = 0, n = 0;
    stack[sp][0] = r; stack[sp][1] = c; sp++;
    mark[r][c] = 1;
    while (sp) {
        sp--;
        int rr = stack[sp][0], cc = stack[sp][1];
        n++;
        int nb[6][2], k = neighbours(rr, cc, nb);
        for (int i = 0; i < k; i++) {
            int a = nb[i][0], b = nb[i][1];
            if (mark[a][b] || grid[a][b] != color) continue;
            mark[a][b] = 1;
            stack[sp][0] = a; stack[sp][1] = b; sp++;
        }
    }
    return n;
}

static int drop_floaters(bool apply) {   // bubbles no longer hanging from the ceiling
    static uint8_t held[MAX_ROWS][COLS];
    memset(held, 0, sizeof(held));
    int stack[MAX_ROWS * COLS][2], sp = 0;
    for (int c = 0; c < row_len(0); c++) if (grid[0][c]) { held[0][c] = 1; stack[sp][0] = 0; stack[sp][1] = c; sp++; }
    while (sp) {
        sp--;
        int nb[6][2], k = neighbours(stack[sp][0], stack[sp][1], nb);
        for (int i = 0; i < k; i++) {
            int a = nb[i][0], b = nb[i][1];
            if (held[a][b] || !grid[a][b]) continue;
            held[a][b] = 1;
            stack[sp][0] = a; stack[sp][1] = b; sp++;
        }
    }
    int n = 0;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < row_len(r); c++)
            if (grid[r][c] && !held[r][c]) {
                n++;
                if (apply) { add_fx(cell_x(r, c), cell_y(r), grid[r][c], true); grid[r][c] = 0; }
            }
    return n;
}

static bool past_line() {
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < row_len(r); c++)
            if (grid[r][c] && cell_y(r) + D / 2.0f > danger_y) return true;
    return false;
}

static void land(int r, int c, int hit_r, int hit_c) {
    static uint8_t mark[MAX_ROWS][COLS];
    grid[r][c] = shot_color;
    int popped = 0;
    if (shot_color == STAR) {
        // The star clears every bubble of the colour it touched
        uint8_t target = hit_r >= 0 ? grid[hit_r][hit_c] : 0;
        grid[r][c] = 0;
        add_fx(cell_x(r, c), cell_y(r), STAR, false);
        for (int rr = 0; rr < rows; rr++)
            for (int cc = 0; cc < row_len(rr); cc++)
                if (target && grid[rr][cc] == target) { add_fx(cell_x(rr, cc), cell_y(rr), target, false); grid[rr][cc] = 0; popped++; }
    } else {
        memset(mark, 0, sizeof(mark));
        if (group(r, c, shot_color, mark) >= 3)
            for (int rr = 0; rr < rows; rr++)
                for (int cc = 0; cc < row_len(rr); cc++)
                    if (mark[rr][cc]) { add_fx(cell_x(rr, cc), cell_y(rr), grid[rr][cc], false); grid[rr][cc] = 0; popped++; }
    }
    score += 10L * popped;
    if (popped) {
        int dropped = drop_floaters(true);
        if (dropped) score += 10L << (dropped < 17 ? dropped : 17);
    }
    if (score > hi_score) hi_score = score;

    // Every 8 shots, the ceiling comes down a row
    if (++shots >= SHOTS_PER_DROP) { shots = 0; drops++; }
    uint8_t list[COLORS];
    int n = colors_in_play(list);
    if (!n) { round_no++; new_round(); return; }   // cleared!
    if (past_line()) { over = true; over_t = 0; return; }
    // A colour can have gone from the field: then the waiting bubbles change to ones still there
    auto in_play = [&](int col) { if (col == STAR) return true; for (int k = 0; k < n; k++) if (list[k] == col) return true; return false; };
    if (!in_play(cur_color))  cur_color = random_color_in_play();
    if (!in_play(next_color)) next_color = random_color_in_play();
}

static void fire() {
    if (shot_on || over || stopping) return;
    shot_on = true;
    shx = launch_x; shy = launch_y - 4;
    svx = sinf(aim) * SHOT_V; svy = -cosf(aim) * SHOT_V;
    shot_color = cur_color;
    cur_color = next_color;
    next_color = star_next ? STAR : random_color_in_play();
    star_next = false;
}

// --- The computer ------------------------------------------------------------

static void plan() {
    static uint8_t mark[MAX_ROWS][COLS];
    float best = -1e9f, best_a = 0;
    int last_r = -1, last_c = -1;
    float last_v = 0;
    for (float a = -MAX_AIM; a <= MAX_AIM; a += 0.03f) {
        int r, c, hr, hc;
        if (!trace(a, r, c, hr, hc)) continue;
        float v;
        if (r == last_r && c == last_c) v = last_v;   // same cell as the last angle: same answer
        else {
            if (cur_color == STAR) {
                int n = 0;
                uint8_t t = hr >= 0 ? grid[hr][hc] : 0;
                for (int rr = 0; rr < rows; rr++) for (int cc = 0; cc < row_len(rr); cc++) n += t && grid[rr][cc] == t;
                v = n * 10;
            } else {
                grid[r][c] = cur_color;
                memset(mark, 0, sizeof(mark));
                int g = group(r, c, cur_color, mark);
                if (g >= 3) {
                    // Try it: how many would fall?
                    uint8_t saved[MAX_ROWS][COLS];
                    memcpy(saved, grid, sizeof(saved));
                    for (int rr = 0; rr < rows; rr++) for (int cc = 0; cc < row_len(rr); cc++) if (mark[rr][cc]) grid[rr][cc] = 0;
                    int f = drop_floaters(false);
                    memcpy(grid, saved, sizeof(saved));
                    v = 100 + g * 10 + f * 25;
                } else v = (g - 1) * 12 - r * 2;   // no pop: build up a group, high up
                grid[r][c] = 0;
            }
            last_r = r; last_c = c; last_v = v;
        }
        v += scene_randf(0, 1);   // break ties between angles
        if (v > best) { best = v; best_a = a; }
    }
    goal_aim = best_a;
    if (random(0, 100) < 6) goal_aim += scene_randf(-0.12f, 0.12f);   // a slip of the hand
    aim_wait = 0.25f;
}

// --- Update and draw ---------------------------------------------------------

static void update(float dt) {
    since_touch += (uint32_t)(dt * 1000);
    if (beat_t > 0) beat_t -= dt;
    shake_t += dt;

    bool any_fx = false;
    for (int i = 0; i < MAX_FX; i++) {
        Fx &f = fx[i];
        if (!f.alive) continue;
        any_fx = true;
        f.t += dt;
        if (f.falling) { f.vy += 260 * dt; f.y += f.vy * dt; if (f.y > H + 6) f.alive = false; }
        else if (f.t > 0.25f) f.alive = false;
    }

    if (stopping) {
        // Everything drops away
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < row_len(r); c++)
                if (grid[r][c]) { add_fx(cell_x(r, c), cell_y(r), grid[r][c], true); grid[r][c] = 0; }
        shot_on = false;
        done = !any_fx;
        return;
    }
    if (over) {   // the field goes grey, row by row, then a new game
        over_t += dt;
        if (over_t > 3) new_game();
        return;
    }

    if (shot_on) {
        float dist = SHOT_V * dt;
        int steps = (int)(dist / 1.5f) + 1;
        float left = field_x + D / 2.0f, right = field_x + COLS * D - D / 2.0f;
        for (int s = 0; s < steps && shot_on; s++) {
            shx += svx * dt / steps; shy += svy * dt / steps;
            if (shx < left)  { shx = 2 * left - shx; svx = -svx; }
            if (shx > right) { shx = 2 * right - shx; svx = -svx; }
            int hr, hc;
            bool stuck = touching(shx, shy, hr, hc) || shy <= top_y() + D / 2.0f;
            if (!stuck) continue;
            shot_on = false;
            // Snap to the nearest free cell around where it stopped
            float best = 1e9f;
            int br = -1, bc = 0, r0 = (int)floorf((shy - top_y() - D / 2.0f) / RH + 0.5f);
            for (int r = r0 - 1; r <= r0 + 1; r++) {
                if (r < 0 || r >= rows) continue;
                for (int c = 0; c < row_len(r); c++) {
                    if (grid[r][c]) continue;
                    float dx = cell_x(r, c) - shx, dy = cell_y(r) - shy, d = dx * dx + dy * dy;
                    if (d < best) { best = d; br = r; bc = c; }
                }
            }
            if (br >= 0) land(br, bc, hr, hc);
            else { over = true; over_t = 0; }
            if (!over && since_touch >= AUTO_RESUME_MS) plan();
        }
        return;
    }

    if (since_touch >= AUTO_RESUME_MS) {
        if (aim_wait > 0 && fabsf(goal_aim - aim) < 0.001f) {
            if ((aim_wait -= dt) <= 0) fire();
        } else {
            float d = goal_aim - aim, step = AIM_V * dt;   // the arrow turns at a steady rate
            aim += d > step ? step : d < -step ? -step : d;
        }
    }
}

static void draw_bubble_at(float x, float y, int color, lv_color_t *override = nullptr) {   // screen coordinates
    // Round, 9 px, with a glint top left and a shaded edge bottom right
    static const char *const ART[] = { "..11111..", ".1221111.", "122111111", "121111111", "111111111",
                                       "111111113", "111111133", ".1111333.", "..33333.." };
    static const char *const STAR_ART[] = { "....1....", "...111...", "111111111", ".1111111.", "..11111..",
                                            ".111.111.", ".11...11.", "........." };
    lv_color_t c = override ? *override : bubble_color(color);
    lv_color_t pal[] = { c, COL_SCENE_BUB_SHINE, pixfb_mix(c, COL_SCENE_BUB_FIELD, 0.45f) };
    int cx = (int)floorf(x), cy = (int)floorf(y);
    pixfb_art(fb, cx - 4, cy - 4, ART, 9, pal);
    if (color == STAR) {   // a star on it
        lv_color_t spal[] = { COL_SCENE_BUB_3 };
        pixfb_art(fb, cx - 4, cy - 3, STAR_ART, 8, spal);
    }
}

static void draw_bubble(float gx, float gy, int color, lv_color_t *override = nullptr) {   // game coordinates
    draw_bubble_at(scr_x(gy), scr_y(gx), color, override);
}

static void draw() {
    pixfb_bg_restore(fb, 0, 0, W, H);
    lv_color_t text = COL_SCENE_BUB_TEXT, dim = COL_SCENE_BUB_DIM;

    // Score and best along the top, the round along the bottom, in the empty
    // stretch by the launcher (the bubbles are drawn over them)
    int hx = 18;
    int sw = pixfb_number(fb, hx, 3, score, text, 6);
    int hw = pixfb_text(fb, hx + sw + 8, 3, "HI", dim);
    pixfb_number(fb, hx + sw + 8 + hw + 4, 3, hi_score, dim, 6);
    int rw = pixfb_text(fb, hx, H - 10, "ROUND", dim);
    pixfb_number(fb, hx + rw + 4, H - 10, round_no, text, 2);

    // The compressor: the ceiling block that comes down
    if (drops) pixfb_fill(fb, (int)scr_x(top_y() - 1), lane_sy, top_y() - 1, COLS * D, COL_SCENE_BUB_WALL);
    // Shake in the last two shots before it drops
    int shake = shots >= SHOTS_PER_DROP - 2 && !over ? ((int)(shake_t * 30) & 1 ? 1 : -1) : 0;

    lv_color_t grey = COL_SCENE_BUB_DEAD;
    int grey_rows = over ? (int)(over_t * 8) : -1;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < row_len(r); c++)
            if (grid[r][c]) draw_bubble(cell_x(r, c) + shake, cell_y(r), grid[r][c], r < grey_rows ? &grey : nullptr);

    for (int i = 0; i < MAX_FX; i++) {
        const Fx &f = fx[i];
        if (!f.alive) continue;
        if (f.falling) draw_bubble_at(f.x, f.y, f.color);
        else {   // a pop: a ring that bursts outward
            int r = 4 + (int)(f.t * 20);
            lv_color_t c = bubble_color(f.color);
            for (int k = 0; k < 8; k++) {
                float a = k * 0.785f;
                pixfb_px(fb, (int)(f.x + cosf(a) * r), (int)(f.y + sinf(a) * r), c);
            }
        }
    }

    // The launcher: its arrow, the loaded bubble, and the next one beside it
    lv_color_t arrow = beat_t > 0 ? pixfb_mix(COL_SCENE_BUB_ARROW, COL_SCENE_BUB_SHINE, beat_t / 0.15f) : COL_SCENE_BUB_ARROW;
    // (on screen the arrow points right at aim 0, and turns down for positive aim)
    int lx = (int)scr_x(launch_y), ly = (int)scr_y(launch_x);
    int ax = lx + (int)(cosf(aim) * 20), ay = ly + (int)(sinf(aim) * 20);
    pixfb_line(fb, lx, ly, ax, ay, arrow);
    pixfb_line(fb, lx - (int)(sinf(aim) * 1.5f), ly + (int)(cosf(aim) * 1.5f), ax, ay, arrow);
    pixfb_fill(fb, lx - 8, ly - 7, 7, 15, COL_SCENE_BUB_WALL);   // the launcher's base, behind the loaded bubble
    if (!shot_on && !over) draw_bubble(launch_x, launch_y, cur_color);
    if (!over) draw_bubble(COLS * D - D / 2.0f, launch_y + 2, next_color);   // bottom left, waiting
    if (shot_on) draw_bubble(shx, shy, shot_color);

    pixfb_touch(fb, 0, 0, W, H);
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
    if (stopping || over) return;
    since_touch = 0;
    float gx = y - lane_sy, gy = ceil_sx - x;   // screen to game
    float dx = gx - launch_x, dy = launch_y - gy;
    if (dy < 2) dy = 2;
    aim = goal_aim = fmaxf(-MAX_AIM, fminf(MAX_AIM, atan2f(dx, dy)));
    if (type == SCENE_TOUCH_RELEASE) fire();
}

static void event(const SceneEvent &e) {
    if (e.type == SCENE_EV_BEAT) beat_t = 0.15f;
    if (e.type == SCENE_EV_CLAUDE) {
        if (e.value < claude_was && !stopping) star_next = true;   // the bubble after next is a star
        claude_was = e.value;
    }
}

static void finish() { pixfb_free(fb); }

const Scene scene_bubble = { "bubble", start, tick, request_stop, is_done, touch, finish, event, true };
SCENE_REGISTER(scene_bubble, "Bubble shooter");
