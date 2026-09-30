#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>
#include <string.h>

// Snake — the Nokia Snake II classic (in the theme's colours): the snake wraps around
// the screen edges, grows by one for every bite, and the game ends only when
// it bites itself. Every fifth bite a bonus critter shows up for a while,
// worth more the sooner it's eaten (its countdown shows top right). Scored
// as on the phone at speed level 5: 5 points a bite. It plays itself, well
// but not perfectly; when it dies it blinks, and a new game starts. When
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
static int      bonus_steps, bonus_kind, bites, score;
static long     hi_score;                       // best since the board started
static float    step_acc, dead_t, tongue_t, shrink_acc;
static bool     dead, lapse, stopping, done, oom;
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
    score = bites = 0;
    bonus_steps = 0;
    dead = lapse = false;
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
// Breadth-first search to the food (or the bonus) round the snake, then check
// the square it would move into still leaves room for the whole snake; if
// not, take whichever move leaves the most room. Now and then (a "lapse") it
// skips that check, which is what eventually gets a long snake killed.

static uint8_t  seen[MAX_ROWS][MAX_COLS];
static Cell     queue[MAX_LEN];

static int room_from(Cell start) {   // squares reachable from `start`, the tail counted as free
    memset(seen, 0, sizeof(seen));
    Cell tail = at(len - 1);
    int qh = 0, qt = 0, n = 0;
    queue[qt++] = start; seen[start.y][start.x] = 1;
    while (qh < qt) {
        Cell c = queue[qh++];
        n++;
        for (int d = 0; d < 4; d++) {
            Cell nb = step_from(c, d);
            if (seen[nb.y][nb.x]) continue;
            if (occ[nb.y][nb.x] && !(nb.x == tail.x && nb.y == tail.y)) continue;
            seen[nb.y][nb.x] = 1;
            queue[qt++] = nb;
        }
    }
    return n;
}

static int first_move_to(Cell goal) {   // direction of the first step of a shortest path, or -1
    static int8_t first[MAX_ROWS][MAX_COLS];
    memset(seen, 0, sizeof(seen));
    Cell h = at(0);
    int qh = 0, qt = 0;
    seen[h.y][h.x] = 1;
    for (int d = 0; d < 4; d++) {
        if (d == (dir + 2) % 4) continue;
        Cell nb = step_from(h, d);
        if (occ[nb.y][nb.x] || seen[nb.y][nb.x]) continue;
        seen[nb.y][nb.x] = 1; first[nb.y][nb.x] = d;
        queue[qt++] = nb;
    }
    while (qh < qt) {
        Cell c = queue[qh++];
        if (c.x == goal.x && c.y == goal.y) return first[c.y][c.x];
        for (int d = 0; d < 4; d++) {
            Cell nb = step_from(c, d);
            if (occ[nb.y][nb.x] || seen[nb.y][nb.x]) continue;
            seen[nb.y][nb.x] = 1; first[nb.y][nb.x] = first[c.y][c.x];
            queue[qt++] = nb;
        }
    }
    return -1;
}

static void autopilot() {
    Cell goal = food;
    if (bonus_steps > 8) goal = bonus;   // go for the critter while there's time
    int d = first_move_to(goal);
    Cell h = at(0);
    if (d >= 0 && !lapse && room_from(step_from(h, d)) < len) d = -1;   // it would box itself in
    if (d < 0) {
        int best = -1, best_room = -1;
        for (int k = 0; k < 4; k++) {
            if (k == (dir + 2) % 4) continue;
            Cell nb = step_from(h, k);
            if (occ[nb.y][nb.x]) continue;
            int r = room_from(nb);
            if (r > best_room) { best_room = r; best = k; }
        }
        d = best >= 0 ? best : dir;
    }
    next_dir = d;
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
    if (eats) {
        score += LEVEL;
        if (++bites % BONUS_EVERY == 0 && bonus_steps == 0) place_bonus();
        lapse = random(0, 100) < 3;   // the next approach might be careless
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
