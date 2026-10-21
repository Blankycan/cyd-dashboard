#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>
#include <string.h>

// Snake — the Nokia Snake II classic (in the theme's colours): the snake wraps around
// the screen edges, grows by one for every bite, and the game ends only when
// it bites itself. Every fifth bite a bonus critter shows up for a while,
// worth more the sooner it's eaten (its countdown shows top right). Scored
// as on the phone at speed level 5: 5 points a bite. It plays itself, never
// taking a bite it couldn't get back out of, so it usually fills nearly the
// whole screen; when it dies it blinks, and a new game starts. When
// asked to stop, the snake shrinks away into its head.
// Touch: going sideways, tap above or below the snake to turn up or down;
// going up or down, tap left or right of it. The computer takes back
// over AUTO_RESUME_MS after your last touch.
// Events: with music playing the snake flicks its tongue on the beat; a
// Claude session finishing brings out a bonus critter.

static const uint32_t AUTO_RESUME_MS = 5000;
static const int      CELL = 6, HUD_H = 10, MAX_COLS = 40, MAX_ROWS = 14, MAX_LEN = MAX_COLS * MAX_ROWS;
static const int      LEVEL = 5, START_LEN = 7, BONUS_EVERY = 5, BONUS_STEPS = 40;
static const float    STEP_S = 0.12f;

// Bonus critters, two cells wide ('1' ink)
static const char *const CRITTERS[3][5] = {
    { "..1......1..", "...1....1...", "..11111111..", ".1.111111.1.", "1..1.11.1..1" },   // bug
    { ".1........1.", "..1.1111.1..", "...111111...", "..1.1111.1..", ".1........1." },   // spider
    { "...111111...", "..1.1111.1..", ".1111111111.", "..11111111..", ".1.1....1.1." },   // beetle
};

struct Cell { int8_t x, y; };

static PixFb    fb;
static int      W, H, cols, rows, ox, oy;       // grid size and its top-left on screen
static Cell     body[MAX_LEN];                  // ring buffer, head at [head]
static int      head, len, dir, next_dir;
static uint8_t  occ[MAX_ROWS][MAX_COLS];        // 1 where the snake is
static Cell     food, bonus;
static int      bonus_steps, bonus_kind, bites, score, since_bite;
static long     hi_score;                       // best since the board started
static float    step_acc, dead_t, tongue_t, shrink_acc;
static bool     dead, stopping, done, oom;
static int      claude_was;
static uint32_t since_touch;

static const int DX[4] = { 1, 0, -1, 0 }, DY[4] = { 0, 1, 0, -1 };   // right, down, left, up

static Cell at(int i) { return body[(head - i + MAX_LEN) % MAX_LEN]; }   // 0 = head
static Cell step_from(Cell c, int d) {
    return { (int8_t)((c.x + DX[d] + cols) % cols), (int8_t)((c.y + DY[d] + rows) % rows) };
}
static bool free_cell(int x, int y) { return !occ[y][x]; }

static void place_food() {
    for (;;) {
        int x = random(0, cols), y = random(0, rows);
        if (free_cell(x, y) && !(bonus_steps > 0 && y == bonus.y && (x == bonus.x || x == (bonus.x + 1) % cols))) {
            food = { (int8_t)x, (int8_t)y };
            return;
        }
    }
}

static void place_bonus() {
    for (int tries = 0; tries < 200; tries++) {
        int x = random(0, cols), y = random(0, rows), x2 = (x + 1) % cols;
        if (free_cell(x, y) && free_cell(x2, y) && !(food.y == y && (food.x == x || food.x == x2))) {
            bonus = { (int8_t)x, (int8_t)y };
            bonus_steps = BONUS_STEPS;
            bonus_kind = random(0, 3);
            return;
        }
    }
}

static void new_game() {
    memset(occ, 0, sizeof(occ));
    len = START_LEN;
    head = len - 1;
    int y = rows / 2, x0 = cols / 2 - len;
    for (int i = 0; i < len; i++) { body[i] = { (int8_t)(x0 + i), (int8_t)y }; occ[y][x0 + i] = 1; }
    dir = next_dir = 0;
    score = bites = since_bite = 0;
    bonus_steps = 0;
    dead = false;
    step_acc = 0;
    place_food();
}

static void paint_backdrop() {
    lv_color_t lcd = COL_SCENE_SNAKE_LCD, ink = COL_SCENE_SNAKE_FRAME;
    pixfb_fill(fb, 0, 0, W, H, lcd);
    // A frame around the field, the HUD line above it
    pixfb_fill(fb, ox - 2, oy - 2, cols * CELL + 3, 1, ink);
    pixfb_fill(fb, ox - 2, oy + rows * CELL, cols * CELL + 3, 1, ink);
    pixfb_fill(fb, ox - 2, oy - 2, 1, rows * CELL + 3, ink);
    pixfb_fill(fb, ox + cols * CELL, oy - 2, 1, rows * CELL + 3, ink);
    pixfb_touch(fb, 0, 0, W, H);
    pixfb_bg_save(fb);
}

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_SNAKE_LCD);
    done = oom;
    if (oom) return;
    cols = (w - 4) / CELL;
    rows = (h - HUD_H - 3) / CELL;
    if (cols > MAX_COLS) cols = MAX_COLS;
    if (rows > MAX_ROWS) rows = MAX_ROWS;
    ox = (w - cols * CELL) / 2;
    oy = HUD_H + 1;
    paint_backdrop();
    if (!fb.bg) { done = oom = true; return; }
    new_game();
    tongue_t = shrink_acc = 0;
    claude_was = scene_ctx().claude_working;
    stopping = false;
    since_touch = AUTO_RESUME_MS;
}

// --- Autopilot ---------------------------------------------------------------
// The classic safe way: find the shortest way to the food (or the critter,
// if it can get there before it goes), then play the whole trip out on a
// copy of the snake and only take it if, having eaten, the head can still
// find its way round to the tail. If not, it follows its own tail the long
// way round, so the body moves out of the way, until the food is safe to go
// for. Only when even that's impossible does it settle for the most room.

static uint8_t  vocc[MAX_ROWS][MAX_COLS];       // the snake as it would be
static int16_t  dist[MAX_ROWS][MAX_COLS];
static int8_t   came[MAX_ROWS][MAX_COLS];       // the step that reached each square
static Cell     queue[MAX_LEN], path[MAX_LEN], vbody[MAX_LEN + 1];

static bool same(Cell a, Cell b) { return a.x == b.x && a.y == b.y; }

// Distances from `from` over the squares free in `blk`; `pass` counts as free
// (the tail, which will have moved on). Returns how many squares it reached.
static int flood(const uint8_t blk[MAX_ROWS][MAX_COLS], Cell from, Cell pass) {
    for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) dist[y][x] = -1;
    int qh = 0, qt = 0;
    queue[qt++] = from; dist[from.y][from.x] = 0;
    while (qh < qt) {
        Cell c = queue[qh++];
        if (same(c, pass) && qh > 1) continue;   // the tail: can get to it, not past it
        for (int d = 0; d < 4; d++) {
            Cell nb = step_from(c, d);
            if (dist[nb.y][nb.x] >= 0 || (blk[nb.y][nb.x] && !same(nb, pass))) continue;
            dist[nb.y][nb.x] = dist[c.y][c.x] + 1; came[nb.y][nb.x] = d;
            queue[qt++] = nb;
        }
    }
    return qt;
}

// After following path[0..n-1] (and growing by one if it eats at the end),
// can the head still get to the tail?
static bool safe_trip(int n, bool grows) {
    int vlen = len + (grows ? 1 : 0), k = 0;
    for (int i = n - 1; i >= 0 && k < vlen; i--) vbody[k++] = path[i];
    for (int i = 0; k < vlen; i++) vbody[k++] = at(i);
    memset(vocc, 0, sizeof(vocc));
    for (int i = 0; i < vlen; i++) vocc[vbody[i].y][vbody[i].x] = 1;
    Cell vtail = vbody[vlen - 1];
    flood(vocc, vbody[0], vtail);
    return dist[vtail.y][vtail.x] > 0;
}

// The shortest way from the head to `goal` (or the square beside it, for the
// critter's second half) without going through the snake; its length, or -1.
// The way to go first is left in route_dir.
static int route_dir;
static int route_to(Cell goal, bool wide) {
    Cell h = at(0), tail = at(len - 1);
    flood(occ, h, tail);
    Cell end = goal;
    if (wide) {
        Cell other = { (int8_t)((goal.x + 1) % cols), goal.y };
        int a = dist[goal.y][goal.x], b = dist[other.y][other.x];
        if (b >= 0 && (a < 0 || b < a)) end = other;
    }
    int n = dist[end.y][end.x];
    if (n <= 0) return -1;
    Cell c = end;
    for (int i = n - 1; i >= 0; i--) { path[i] = c; c = step_from(c, (came[c.y][c.x] + 2) % 4); }
    route_dir = came[path[0].y][path[0].x];
    return route_dir == (dir + 2) % 4 ? -1 : n;   // no turning straight back
}

static void autopilot() {
    Cell h = at(0), tail = at(len - 1);
    // The critter first, if there's time to get there; then the food
    if (bonus_steps > 0) {
        int n = route_to(bonus, true);
        if (n > 0 && n < bonus_steps && safe_trip(n, false)) { next_dir = route_dir; return; }
    }
    int n = route_to(food, false);
    // Safe, or it's been going round so long the board's as good as full: go for it
    if (n > 0 && (safe_trip(n, true) || since_bite > 2 * cols * rows)) { next_dir = route_dir; return; }

    // Not safe yet: follow the tail, taking the move that keeps it farthest
    // away (the long way round buys the most time), closer to the food on a tie
    int best = -1, best_tail = -1, best_food = 1 << 30, best_room = -1, roomiest = -1;
    for (int k = 0; k < 4; k++) {
        if (k == (dir + 2) % 4) continue;
        Cell nb = step_from(h, k);
        if (same(nb, food)) continue;   // eating here was found unsafe above
        if (occ[nb.y][nb.x] && !same(nb, tail)) continue;
        path[0] = nb;
        bool ok = safe_trip(1, false);
        int to_tail = dist[vbody[len - 1].y][vbody[len - 1].x];
        int room = flood(vocc, nb, vbody[len - 1]);
        if (room > best_room) { best_room = room; roomiest = k; }
        if (!ok) continue;
        int dfx = abs(nb.x - food.x), dfy = abs(nb.y - food.y);
        int to_food = min(dfx, cols - dfx) + min(dfy, rows - dfy);
        if (to_tail > best_tail || (to_tail == best_tail && to_food < best_food)) { best_tail = to_tail; best_food = to_food; best = k; }
    }
    if (best < 0) best = roomiest;
    if (best < 0) {   // nothing but the food left that doesn't kill it at once
        for (int k = 0; k < 4 && best < 0; k++)
            if (k != (dir + 2) % 4 && !occ[step_from(h, k).y][step_from(h, k).x]) best = k;
    }
    next_dir = best >= 0 ? best : dir;
}

// --- Game --------------------------------------------------------------------

static void step() {
    dir = next_dir;
    Cell h = step_from(at(0), dir);
    Cell tail = at(len - 1);
    bool eats = h.x == food.x && h.y == food.y;
    bool eats_bonus = bonus_steps > 0 && h.y == bonus.y && (h.x == bonus.x || h.x == (bonus.x + 1) % cols);
    if (!eats) occ[tail.y][tail.x] = 0;   // the tail moves on, unless this bite makes it grow
    if (occ[h.y][h.x]) { dead = true; dead_t = 0; if (!eats) occ[tail.y][tail.x] = 1; return; }
    head = (head + 1) % MAX_LEN;
    body[head] = h;
    occ[h.y][h.x] = 1;
    if (eats) len++;
    since_bite = eats ? 0 : since_bite + 1;
    if (eats) {
        score += LEVEL;
        if (++bites % BONUS_EVERY == 0 && bonus_steps == 0) place_bonus();
        if (len + 1 >= cols * rows) { dead = true; dead_t = 0; return; }   // no room left: a win, really
        place_food();
    }
    if (eats_bonus) { score += LEVEL * 2 * bonus_steps / 4; bonus_steps = 0; }
    else if (bonus_steps > 0) bonus_steps--;
    if (score > hi_score) hi_score = score;
}

static void update(float dt) {
    since_touch += (uint32_t)(dt * 1000);
    if (tongue_t > 0) tongue_t -= dt;
    if (stopping) {
        // Wrapping up: the tail is drawn in, fast, until only the head is left, then it's gone
        shrink_acc += dt * (len / 1.2f + 4);
        while (shrink_acc >= 1 && len > 0) {
            shrink_acc -= 1;
            Cell t = at(len - 1);
            occ[t.y][t.x] = 0;
            len--;
        }
        if (len == 0) done = true;
        return;
    }
    if (dead) {
        if ((dead_t += dt) > 2.4f) new_game();
        return;
    }
    step_acc += dt;
    while (step_acc >= STEP_S && !dead) {
        step_acc -= STEP_S;
        if (since_touch >= AUTO_RESUME_MS) autopilot();
        step();
    }
}

// --- Drawing -----------------------------------------------------------------

static void cell_rect(Cell c, int &x, int &y) { x = ox + c.x * CELL; y = oy + c.y * CELL; }

static void draw_segment(int i, lv_color_t ink) {
    // A block with a hole in the middle (the phone's snake pattern), joined to
    // the segment ahead unless they're on opposite edges (a wrap)
    Cell c = at(i);
    int x, y;
    cell_rect(c, x, y);
    pixfb_fill(fb, x, y, CELL - 1, CELL - 1, ink);
    if (i > 0) pixfb_px(fb, x + 2, y + 2, COL_SCENE_SNAKE_LCD);
    if (i + 1 < len) {
        Cell n = at(i + 1);
        int dx = n.x - c.x, dy = n.y - c.y;
        if (dx == 1)  pixfb_fill(fb, x + CELL - 1, y, 1, CELL - 1, ink);
        if (dx == -1) pixfb_fill(fb, x - 1, y, 1, CELL - 1, ink);
        if (dy == 1)  pixfb_fill(fb, x, y + CELL - 1, CELL - 1, 1, ink);
        if (dy == -1) pixfb_fill(fb, x, y - 1, CELL - 1, 1, ink);
    }
}

static void draw() {
    pixfb_bg_restore(fb, 0, 0, W, H);
    lv_color_t ink = COL_SCENE_SNAKE_INK, lcd = COL_SCENE_SNAKE_LCD;

    // Score top left (the best, dimmer, beside it); the critter's countdown top right
    int sw = pixfb_number(fb, ox, 1, score, COL_SCENE_SNAKE_TEXT, 4);
    pixfb_number(fb, ox + sw + 10, 1, hi_score, COL_SCENE_SNAKE_DIM, 4);
    if (bonus_steps > 0 && !stopping) {
        lv_color_t pal[] = { COL_SCENE_SNAKE_BONUS };
        int nx = ox + cols * CELL - pixfb_number_width(bonus_steps);
        pixfb_number(fb, nx, 1, bonus_steps, COL_SCENE_SNAKE_TEXT);
        pixfb_art(fb, nx - 16, 2, CRITTERS[bonus_kind], 5, pal);
    }

    // Food: a small diamond; the critter, two cells wide
    if (!stopping) {
        int fx, fy;
        cell_rect(food, fx, fy);
        lv_color_t food_c = COL_SCENE_SNAKE_FOOD;
        pixfb_fill(fb, fx + 2, fy, 1, 5, food_c);
        pixfb_fill(fb, fx, fy + 2, 5, 1, food_c);
        pixfb_fill(fb, fx + 1, fy + 1, 3, 3, food_c);
        pixfb_px(fb, fx + 2, fy + 2, lcd);
        if (bonus_steps > 0) {
            int bx, by;
            cell_rect(bonus, bx, by);
            lv_color_t pal[] = { COL_SCENE_SNAKE_BONUS };
            pixfb_art(fb, bx, by, CRITTERS[bonus_kind], 5, pal);
        }
    }

    // The snake (blinking when it's dead)
    bool show = !dead || fmodf(dead_t, 0.4f) < 0.2f || dead_t > 1.6f;
    if (show && len > 0) {
        for (int i = len - 1; i >= 0; i--) draw_segment(i, ink);
        // Head: eyes, and a flick of the tongue now and then
        Cell c = at(0);
        int x, y;
        cell_rect(c, x, y);
        int ex = DX[dir] ? (DX[dir] > 0 ? x + 3 : x + 1) : x + 1, ey = DY[dir] ? (DY[dir] > 0 ? y + 3 : y + 1) : y + 1;
        pixfb_px(fb, ex, ey, lcd);
        pixfb_px(fb, ex + (DX[dir] ? 0 : 2), ey + (DY[dir] ? 0 : 2), lcd);
        if (tongue_t > 0 && !dead) {
            int tx = x + 2 + DX[dir] * 4, ty = y + 2 + DY[dir] * 4;
            pixfb_fill(fb, tx, ty, DX[dir] ? 2 : 1, DY[dir] ? 2 : 1, ink);
        }
    }
    pixfb_touch(fb, 0, 0, W, H);
}

static void tick(uint32_t dt_ms) {
    if (oom || done) return;
    update(dt_ms / 1000.0f);
    if (!stopping && !dead && random(0, 1000) < (int)dt_ms / 4) tongue_t = 0.15f;   // a flick every few seconds
    draw();
    pixfb_flush(fb);
}

static void request_stop() { stopping = true; }
static bool is_done()      { return done; }

static void touch(SceneTouch type, int x, int y) {
    if (type != SCENE_TOUCH_PRESS || stopping || dead) return;
    since_touch = 0;
    // Going sideways, a tap above or below the head turns up or down; going
    // up or down, a tap left or right of it turns that way
    int hx, hy;
    cell_rect(at(0), hx, hy);
    int dx = x - (hx + CELL / 2), dy = y - (hy + CELL / 2);
    if (DX[dir]) { if (dy) next_dir = dy > 0 ? 1 : 3; }
    else         { if (dx) next_dir = dx > 0 ? 0 : 2; }
}

static void event(const SceneEvent &e) {
    if (e.type == SCENE_EV_BEAT && !stopping) tongue_t = 0.15f;
    if (e.type == SCENE_EV_CLAUDE) {
        if (e.value < claude_was && !stopping && !dead && bonus_steps == 0) place_bonus();
        claude_was = e.value;
    }
}

static void finish() { pixfb_free(fb); }

const Scene scene_snake = { "snake", start, tick, request_stop, is_done, touch, finish, event, true };
SCENE_REGISTER(scene_snake, "Snake");
