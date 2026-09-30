#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>
#include <string.h>

// Tetris — as on the NES: a 10x20 well, the seven pieces with the NES
// rotation (no wall kicks), gravity timed in NES frames for each level, and
// no lock delay: a piece locks the moment it can't fall. Scored 40 / 100 /
// 300 / 1200 x (level + 1) for 1-4 lines, plus a point per row of soft drop.
// Cleared lines wipe out from the middle; a Tetris (four at once) flashes the
// screen. Each level has its own colours, and the well turns over to the next
// level every 10 lines, getting faster each time (it starts at level 0, as
// the NES does unless you pick another). The piece statistics
// are on the left, TOP / SCORE / LINES / LEVEL and the NEXT piece on the right. It plays itself, tapping the pieces
// into place and soft-dropping them, until the speed gets the better of it:
// then the curtain comes down and a new game starts. When asked to stop, the
// curtain comes down too.
// Touch: tap left or right of the falling piece to shift it, on it to rotate
// it; tap anywhere along the bottom (or drag down) to soft drop it. The
// computer takes back over AUTO_RESUME_MS after your last touch.
// Events: with music playing the well's frame pulses on the beat.

static const uint32_t AUTO_RESUME_MS = 5000;
static const int      COLS = 10, ROWS = 20, CELL = 4, START_LEVEL = 0;   // the NES default
static const int      TAP_FRAMES = 5, SOFT_FRAMES = 2;   // how fast the computer taps; soft drop, frames a row

enum Piece { P_T, P_J, P_Z, P_O, P_S, P_L, P_I, PIECES };
// NES rotation states, cells as (dx, dy) around the pivot, clockwise order
static const int8_t SHAPES[PIECES][4][4][2] = {
    { { {-1,0},{0,0},{1,0},{0,1} }, { {0,-1},{-1,0},{0,0},{0,1} }, { {-1,0},{0,0},{1,0},{0,-1} }, { {0,-1},{0,0},{1,0},{0,1} } },   // T
    { { {-1,0},{0,0},{1,0},{1,1} }, { {0,-1},{0,0},{-1,1},{0,1} }, { {-1,-1},{-1,0},{0,0},{1,0} }, { {0,-1},{1,-1},{0,0},{0,1} } }, // J
    { { {-1,0},{0,0},{0,1},{1,1} }, { {1,-1},{0,0},{1,0},{0,1} }, { {-1,0},{0,0},{0,1},{1,1} }, { {1,-1},{0,0},{1,0},{0,1} } },     // Z
    { { {-1,0},{0,0},{-1,1},{0,1} }, { {-1,0},{0,0},{-1,1},{0,1} }, { {-1,0},{0,0},{-1,1},{0,1} }, { {-1,0},{0,0},{-1,1},{0,1} } }, // O
    { { {0,0},{1,0},{-1,1},{0,1} }, { {0,-1},{0,0},{1,0},{1,1} }, { {0,0},{1,0},{-1,1},{0,1} }, { {0,-1},{0,0},{1,0},{1,1} } },     // S
    { { {-1,0},{0,0},{1,0},{-1,1} }, { {-1,-1},{0,-1},{0,0},{0,1} }, { {1,-1},{-1,0},{0,0},{1,0} }, { {0,-1},{0,0},{0,1},{1,1} } }, // L
    { { {-2,0},{-1,0},{0,0},{1,0} }, { {0,-2},{0,-1},{0,0},{0,1} }, { {-2,0},{-1,0},{0,0},{1,0} }, { {0,-2},{0,-1},{0,0},{0,1} } },  // I
};
static const int STATES[PIECES] = { 4, 4, 2, 1, 2, 4, 2 };
// NES block styles: T, O, I white with a coloured rim; J, S solid colour 1; Z, L solid colour 2
static const uint8_t STYLE[PIECES] = { 0, 1, 2, 0, 1, 2, 0 };
// Frames per row of gravity, levels 0..29 (60 frames a second)
static const uint8_t GRAVITY[30] = { 48, 43, 38, 33, 28, 23, 18, 13, 8, 6, 5, 5, 5, 4, 4, 4, 3, 3, 3,
                                     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1 };

enum Phase { PH_FALL, PH_CLEAR, PH_ENTRY, PH_CURTAIN, PH_OVER };

static PixFb    fb;
static int      W, H, well_x, well_y, panel_x, stats_x;
static uint8_t  well[ROWS][COLS];               // 0 empty, else piece + 1
static int      cur, rot, px, py, next_piece, level, lines, start_level;
static long     score, top_score;               // top: best since the board started
static int      stats[PIECES];
static Phase    phase;
static int      phase_frames, grav_frames, soft_frames, tap_frames, clear_rows[4], n_clear, curtain_row;
static int      entry_frames;   // the entry delay before the next piece
static float    frame_acc, flash_t, beat_t, over_t;
static int      goal_rot, goal_x, think_frames;   // the computer's plan for this piece
static bool     soft, soft_latched, stopping, done, oom;   // latched: a tap at the bottom drops it all the way
static uint32_t since_touch;
static int      touch_y0;

static const int8_t *cell_of(int p, int r, int i) { return SHAPES[p][r % STATES[p]][i]; }

static bool fits(int p, int r, int x, int y) {
    for (int i = 0; i < 4; i++) {
        const int8_t *c = cell_of(p, r, i);
        int cx = x + c[0], cy = y + c[1];
        if (cx < 0 || cx >= COLS || cy >= ROWS) return false;
        if (cy >= 0 && well[cy][cx]) return false;
    }
    return true;
}

// --- The computer ------------------------------------------------------------
// Tries every rotation and column for the current piece, drops it, and scores
// the result on the usual four things: total height, lines cleared, holes,
// and bumpiness. Once in a while it goes for a lesser spot.

static float evaluate(uint8_t g[ROWS][COLS]) {
    int heights[COLS], holes = 0, full = 0;
    for (int x = 0; x < COLS; x++) {
        heights[x] = 0;
        bool seen = false;
        for (int y = 0; y < ROWS; y++) {
            if (g[y][x]) { if (!seen) heights[x] = ROWS - y; seen = true; }
            else if (seen) holes++;
        }
    }
    for (int y = 0; y < ROWS; y++) {
        bool f = true;
        for (int x = 0; x < COLS && f; x++) f = g[y][x];
        full += f;
    }
    int agg = 0, bump = 0;
    for (int x = 0; x < COLS; x++) { agg += heights[x]; if (x) bump += abs(heights[x] - heights[x - 1]); }
    return -0.510066f * agg + 0.760666f * full - 0.35663f * holes - 0.184483f * bump;
}

static void plan() {
    static uint8_t g[ROWS][COLS];
    float best = -1e9f, second = -1e9f;
    int br = 0, bx = px, sr = 0, sx = px;
    for (int r = 0; r < STATES[cur]; r++)
        for (int x = -2; x < COLS + 2; x++) {
            if (!fits(cur, r, x, 0)) continue;
            int y = 0;
            while (fits(cur, r, x, y + 1)) y++;
            memcpy(g, well, sizeof(g));
            for (int i = 0; i < 4; i++) {
                const int8_t *c = cell_of(cur, r, i);
                if (y + c[1] >= 0) g[y + c[1]][x + c[0]] = 1;
            }
            float v = evaluate(g);
            if (v > best) { second = best; sr = br; sx = bx; best = v; br = r; bx = x; }
            else if (v > second) { second = v; sr = r; sx = x; }
        }
    bool lapse = random(0, 100) < 2 && second > -1e8f;
    goal_rot = lapse ? sr : br;
    goal_x = lapse ? sx : bx;
    think_frames = 5;
}

// --- The game ----------------------------------------------------------------

static int roll_piece(int prev) {
    int p = random(0, PIECES);
    if (p == prev) p = random(0, PIECES);   // the NES rerolls once on a repeat
    return p;
}

static void spawn() {
    cur = next_piece;
    next_piece = roll_piece(cur);
    rot = 0; px = 5; py = 0;
    stats[cur]++;
    grav_frames = soft_frames = tap_frames = 0;
    soft = soft_latched = false;
    if (!fits(cur, rot, px, py)) { phase = PH_CURTAIN; curtain_row = 0; phase_frames = 0; return; }
    phase = PH_FALL;
    plan();
}

static int lines_to_first_level_up() {   // the NES's odd first transition when starting past level 0
    int a = start_level * 10 + 10, b = start_level * 10 - 50;
    if (b < 100) b = 100;
    return a < b ? a : b;
}

static void new_game() {
    memset(well, 0, sizeof(well));
    memset(stats, 0, sizeof(stats));
    start_level = START_LEVEL;
    level = start_level;
    lines = 0;
    score = 0;
    next_piece = roll_piece(-1);
    spawn();
}

static void lock_piece() {
    int lowest = 0;
    for (int i = 0; i < 4; i++) {
        const int8_t *c = cell_of(cur, rot, i);
        int y = py + c[1];
        if (y >= 0) well[y][px + c[0]] = cur + 1;
        if (y > lowest) lowest = y;
    }
    n_clear = 0;
    for (int y = 0; y < ROWS; y++) {
        bool f = true;
        for (int x = 0; x < COLS && f; x++) f = well[y][x];
        if (f && n_clear < 4) clear_rows[n_clear++] = y;
    }
    // Entry delay: 10 frames when it locked near the bottom, 2 more for every 4 rows higher
    entry_frames = 10 + 2 * ((ROWS - 1 - lowest) / 4);
    if (entry_frames > 18) entry_frames = 18;
    phase = n_clear ? PH_CLEAR : PH_ENTRY;   // after a clear, the delay follows the wipe
    phase_frames = n_clear ? 0 : entry_frames;
}

static void finish_clear() {
    static const int POINTS[5] = { 0, 40, 100, 300, 1200 };
    score += (long)POINTS[n_clear] * (level + 1);
    if (n_clear == 4) flash_t = 0.3f;
    for (int k = 0; k < n_clear; k++) {   // rows fall in, top to bottom so indices stay right
        int y = clear_rows[k];
        memmove(&well[1][0], &well[0][0], (size_t)y * COLS);
        memset(&well[0][0], 0, COLS);
    }
    int before = lines;
    lines += n_clear;
    int first = lines_to_first_level_up();
    if (before < first && lines >= first) level++;
    else if (before >= first && lines / 10 > before / 10) level++;
    phase = PH_ENTRY;
    phase_frames = entry_frames;
}

static void frame(bool computer) {
    switch (phase) {
    case PH_FALL: {
        if (computer && !stopping) {
            if (think_frames > 0) think_frames--;
            else {
                if (tap_frames > 0) tap_frames--;
                else if (rot != goal_rot) {
                    int nr = (goal_rot - rot + STATES[cur]) % STATES[cur] == 3 ? (rot + 3) % 4 : (rot + 1) % 4;
                    nr %= STATES[cur];
                    if (fits(cur, nr, px, py)) rot = nr; else goal_rot = rot;   // blocked: give up on it
                    tap_frames = TAP_FRAMES;
                }
                if (tap_frames == 0 && px != goal_x) {
                    int d = goal_x > px ? 1 : -1;
                    if (fits(cur, rot, px + d, py)) px += d; else goal_x = px;
                    tap_frames = TAP_FRAMES;
                }
                soft = rot == goal_rot && px == goal_x;
            }
        }
        // Gravity, or the soft drop if that's quicker; no lock delay
        bool drop = false;
        if (soft && ++soft_frames >= SOFT_FRAMES) { soft_frames = 0; drop = true; if (fits(cur, rot, px, py + 1)) score++; }
        if (++grav_frames >= GRAVITY[level < 29 ? level : 29]) { grav_frames = 0; drop = true; }
        if (drop) {
            if (fits(cur, rot, px, py + 1)) py++;
            else lock_piece();
        }
        break;
    }
    case PH_CLEAR:
        if (++phase_frames >= 20) finish_clear();   // five steps of four frames, wiping out from the middle
        break;
    case PH_ENTRY:
        if (--phase_frames <= 0) { if (stopping) { phase = PH_CURTAIN; curtain_row = 0; phase_frames = 0; } else spawn(); }
        break;
    case PH_CURTAIN:
        if (++phase_frames >= 4) { phase_frames = 0; if (++curtain_row > ROWS) { phase = PH_OVER; over_t = 0; } }
        break;
    case PH_OVER:
        break;
    }
}

static void update(float dt) {
    since_touch += (uint32_t)(dt * 1000);
    if (flash_t > 0) flash_t -= dt;
    if (beat_t > 0) beat_t -= dt;
    if (score > top_score) top_score = score;
    if (phase == PH_OVER) {
        over_t += dt;
        if (stopping) done = true;
        else if (over_t > 2.5f) new_game();
        return;
    }
    if (stopping && phase == PH_FALL) { phase = PH_CURTAIN; curtain_row = 0; phase_frames = 0; }
    frame_acc += dt * 60;
    bool computer = since_touch >= AUTO_RESUME_MS;
    while (frame_acc >= 1) { frame_acc -= 1; frame(computer); }
}

// --- Drawing -----------------------------------------------------------------

static lv_color_t level_color(int which) {
    // Each level's two colours; they repeat every ten levels
    switch ((level % 10) * 2 + which) {
        case 0:  return COL_SCENE_TETRIS_L0_A;  case 1:  return COL_SCENE_TETRIS_L0_B;
        case 2:  return COL_SCENE_TETRIS_L1_A;  case 3:  return COL_SCENE_TETRIS_L1_B;
        case 4:  return COL_SCENE_TETRIS_L2_A;  case 5:  return COL_SCENE_TETRIS_L2_B;
        case 6:  return COL_SCENE_TETRIS_L3_A;  case 7:  return COL_SCENE_TETRIS_L3_B;
        case 8:  return COL_SCENE_TETRIS_L4_A;  case 9:  return COL_SCENE_TETRIS_L4_B;
        case 10: return COL_SCENE_TETRIS_L5_A;  case 11: return COL_SCENE_TETRIS_L5_B;
        case 12: return COL_SCENE_TETRIS_L6_A;  case 13: return COL_SCENE_TETRIS_L6_B;
        case 14: return COL_SCENE_TETRIS_L7_A;  case 15: return COL_SCENE_TETRIS_L7_B;
        case 16: return COL_SCENE_TETRIS_L8_A;  case 17: return COL_SCENE_TETRIS_L8_B;
        default: return which ? COL_SCENE_TETRIS_L9_B : COL_SCENE_TETRIS_L9_A;
    }
}

static void draw_block(int x, int y, int p, int size) {
    // A (size-1) square with a gap; the NES's three styles
    int s = size - 1;
    lv_color_t white = COL_SCENE_TETRIS_WHITE;
    switch (STYLE[p]) {
    case 0:   // white centre in a rim of colour 1
        pixfb_fill(fb, x, y, s, s, level_color(0));
        if (s > 2) pixfb_fill(fb, x + 1, y + 1, s - 2, s - 2, white);
        break;
    default: {   // solid colour 1 or 2, with a white glint in the corner
        pixfb_fill(fb, x, y, s, s, level_color(STYLE[p] == 1 ? 0 : 1));
        pixfb_px(fb, x, y, white);
        break;
    }
    }
}

static void draw_piece(int p, int r, int x, int y, int size) {   // x, y: where the pivot cell's top-left goes
    for (int i = 0; i < 4; i++) {
        const int8_t *c = cell_of(p, r, i);
        draw_block(x + c[0] * size, y + c[1] * size, p, size);
    }
}

static void draw() {
    pixfb_bg_restore(fb, 0, 0, W, H);
    lv_color_t text = COL_SCENE_TETRIS_TEXT, dim = COL_SCENE_TETRIS_DIM;
    if (flash_t > 0 && fmodf(flash_t, 0.1f) < 0.05f) pixfb_fill(fb, 0, 0, W, H, COL_SCENE_TETRIS_WHITE);   // a Tetris!

    // The well and its frame
    lv_color_t frame_c = beat_t > 0 ? pixfb_mix(COL_SCENE_TETRIS_FRAME, text, beat_t / 0.15f) : COL_SCENE_TETRIS_FRAME;
    int ww = COLS * CELL, wh = ROWS * CELL;
    pixfb_fill(fb, well_x - 2, well_y - 2, ww + 3, 1, frame_c);
    pixfb_fill(fb, well_x - 2, well_y + wh, ww + 3, 1, frame_c);
    pixfb_fill(fb, well_x - 2, well_y - 2, 1, wh + 3, frame_c);
    pixfb_fill(fb, well_x + ww, well_y - 2, 1, wh + 3, frame_c);
    int wipe = phase == PH_CLEAR ? phase_frames / 4 + 1 : 0;   // columns wiped each side of the middle
    for (int y = 0; y < ROWS; y++) {
        bool clearing = false;
        for (int k = 0; k < n_clear && phase == PH_CLEAR; k++) clearing |= clear_rows[k] == y;
        for (int x = 0; x < COLS; x++) {
            if (!well[y][x]) continue;
            if (clearing && (x >= COLS / 2 - wipe && x < COLS / 2 + wipe)) continue;
            draw_block(well_x + x * CELL, well_y + y * CELL, well[y][x] - 1, CELL);
        }
    }
    if (phase == PH_FALL) draw_piece(cur, rot, well_x + px * CELL, well_y + py * CELL, CELL);
    if (phase == PH_CURTAIN || phase == PH_OVER)   // the curtain: rows of blocks coming down
        for (int y = 0; y < curtain_row && y < ROWS; y++)
            for (int x = 0; x < COLS; x++) draw_block(well_x + x * CELL, well_y + y * CELL, (y + x) % 3 == 0 ? P_T : P_J, CELL);

    // Statistics on the left: each piece and how many have come
    for (int p = 0; p < PIECES; p++) {
        int y = 3 + p * 12;
        draw_piece(p, 0, stats_x + 6, y + (p == P_I ? 3 : 1), 3);
        pixfb_number(fb, stats_x + 18, y, stats[p], p == cur && phase == PH_FALL ? text : COL_SCENE_TETRIS_STAT, 3);
    }

    // The right-hand panel
    int x = panel_x;
    pixfb_text(fb, x, 2, "TOP", dim);    pixfb_number(fb, x + 38, 2, top_score, dim, 6);
    pixfb_text(fb, x, 12, "SCORE", text); pixfb_number(fb, x + 38, 12, score, text, 6);
    pixfb_text(fb, x, 24, "LINES", text); pixfb_number(fb, x + 38, 24, lines, text, 3);
    pixfb_text(fb, x, 34, "LEVEL", text); pixfb_number(fb, x + 38, 34, level, text, 2);
    pixfb_text(fb, x, 48, "NEXT", text);
    int nx = x + 38, ny = 46;
    pixfb_fill(fb, nx, ny, 26, 1, frame_c); pixfb_fill(fb, nx, ny + 16, 26, 1, frame_c);
    pixfb_fill(fb, nx, ny, 1, 17, frame_c); pixfb_fill(fb, nx + 25, ny, 1, 17, frame_c);
    if (phase != PH_CURTAIN && phase != PH_OVER)
        draw_piece(next_piece, 0, nx + 13 - (next_piece == P_O ? 0 : 2) + (next_piece == P_I ? 2 : 0), ny + 4 + (next_piece == P_I ? 2 : 0), CELL);
    pixfb_touch(fb, 0, 0, W, H);
}

// ---------------------------------------------------------------------------

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_TETRIS_BG);
    done = oom;
    if (oom) return;
    if (!pixfb_bg_save(fb)) { done = oom = true; return; }
    well_x = (w - COLS * CELL) / 2 - 7;   // the statistics, well and panel, centred as a group
    stats_x = well_x - 74;
    well_y = (h - ROWS * CELL) / 2 + 1;
    panel_x = well_x + COLS * CELL + 12;
    new_game();
    frame_acc = flash_t = beat_t = 0;
    stopping = false;
    since_touch = AUTO_RESUME_MS;
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
    if (stopping || phase != PH_FALL) { if (type == SCENE_TOUCH_RELEASE && !soft_latched) soft = false; return; }
    since_touch = 0;
    if (type == SCENE_TOUCH_PRESS && y >= H * 2 / 3) { soft = soft_latched = true; return; }   // the bottom: drop it
    if (type == SCENE_TOUCH_PRESS) {
        touch_y0 = y;
        // Left of the piece, right of it, or on it (rotate)
        int minx = COLS, maxx = -1;
        for (int i = 0; i < 4; i++) { int cx = px + cell_of(cur, rot, i)[0]; if (cx < minx) minx = cx; if (cx > maxx) maxx = cx; }
        if (x < well_x + minx * CELL) { if (fits(cur, rot, px - 1, py)) px--; }
        else if (x >= well_x + (maxx + 1) * CELL) { if (fits(cur, rot, px + 1, py)) px++; }
        else { int nr = (rot + 1) % STATES[cur]; if (fits(cur, nr, px, py)) rot = nr; }
    } else if (type == SCENE_TOUCH_DRAG) { if (!soft_latched) soft = y - touch_y0 > 8; }
    else if (!soft_latched) soft = false;
}

static void event(const SceneEvent &e) {
    if (e.type == SCENE_EV_BEAT) beat_t = 0.15f;
}

static void finish() { pixfb_free(fb); }

const Scene scene_tetris = { "tetris", start, tick, request_stop, is_done, touch, finish, event, true };
SCENE_REGISTER(scene_tetris, "Tetris");
