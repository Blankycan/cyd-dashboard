#include "registry.h"
#include "../theme.h"
#include <stdlib.h>
#include <string.h>

// Conway's Game of Life on a wrapping grid, drawn into one canvas (one object
// per cell would be far too many). Starts from a random soup. When the colony
// dies out or settles into a short repeating loop it dissolves and reports
// done by itself; request_stop() dissolves it early.
// Touch: press or drag to bring cells to life under your finger.

static const int      CELL      = 4;     // px per cell (3px dot + 1px gap)
static const uint32_t STEP_MS   = 180;
static const int      HISTORY   = 8;     // generations checked for repeats
static const int      FILL_PCT  = 30;
static const int      DISSOLVE_PCT = 10; // chance per step a cell dies while dissolving (~3-4 s)

static lv_obj_t   *canvas;
static lv_color_t *buf;          // canvas pixels, area_w × area_h
static uint8_t    *grid, *next;  // cols × rows, 1 = alive
static int         area_w, area_h, cols, rows, ox, oy;
static uint32_t    step_acc;
static bool        dissolving, done;
static bool        stop_requested;   // player asked; unlike self-dissolving, touch can't undo it
static uint32_t    history[HISTORY];
static int         history_n;

static void draw_cell(int c, int r, bool alive) {
    lv_color_t col = alive ? COL_SCENE_LIFE_CELL : COL_SCENE_LIFE_BG;
    int x0 = ox + c * CELL, y0 = oy + r * CELL;
    for (int y = y0; y < y0 + CELL - 1; y++)
        for (int x = x0; x < x0 + CELL - 1; x++)
            buf[y * area_w + x] = col;
}

static void set_cell(int c, int r, bool alive) {
    c = (c % cols + cols) % cols;
    r = (r % rows + rows) % rows;
    if (grid[r * cols + c] == alive) return;
    grid[r * cols + c] = alive;
    draw_cell(c, r, alive);
}

static void start(lv_obj_t *area, int w, int h) {
    area_w = w;
    area_h = h;
    cols = w / CELL;
    rows = h / CELL;
    ox = (w - cols * CELL) / 2;
    oy = (h - rows * CELL) / 2;

    buf  = (lv_color_t *)malloc(sizeof(lv_color_t) * w * h);
    grid = (uint8_t *)calloc(cols * rows, 1);
    next = (uint8_t *)calloc(cols * rows, 1);
    done = !buf || !grid || !next;   // out of memory: end straight away
    if (done) return;

    canvas = lv_canvas_create(area);
    lv_canvas_set_buffer(canvas, buf, w, h, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(canvas, COL_SCENE_LIFE_BG, LV_OPA_COVER);

    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            if (random(0, 100) < FILL_PCT) set_cell(c, r, true);

    step_acc   = 0;
    dissolving = stop_requested = false;
    history_n  = 0;
    lv_obj_invalidate(canvas);
}

static uint32_t grid_hash() {
    uint32_t h = 2166136261u;   // FNV-1a
    for (int i = 0; i < cols * rows; i++) { h ^= grid[i]; h *= 16777619u; }
    return h;
}

static void step() {
    int alive = 0;
    for (int r = 0; r < rows; r++) {
        int ru = (r + rows - 1) % rows, rd = (r + 1) % rows;
        for (int c = 0; c < cols; c++) {
            int cl = (c + cols - 1) % cols, cr = (c + 1) % cols;
            int n = grid[ru * cols + cl] + grid[ru * cols + c] + grid[ru * cols + cr]
                  + grid[r  * cols + cl]                       + grid[r  * cols + cr]
                  + grid[rd * cols + cl] + grid[rd * cols + c] + grid[rd * cols + cr];
            bool was = grid[r * cols + c];
            bool now = was ? (n == 2 || n == 3) : (n == 3 && !dissolving);
            if (now && dissolving && random(0, 100) < DISSOLVE_PCT) now = false;
            next[r * cols + c] = now;
            alive += now;
        }
    }
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            if (next[r * cols + c] != grid[r * cols + c]) draw_cell(c, r, next[r * cols + c]);
    uint8_t *t = grid; grid = next; next = t;
    lv_obj_invalidate(canvas);

    if (alive == 0) { done = true; return; }
    if (dissolving) return;

    // Stuck in a still life or short oscillator: wind down on our own
    uint32_t h = grid_hash();
    for (int i = 0; i < history_n; i++) if (history[i] == h) { dissolving = true; return; }
    memmove(history + 1, history, sizeof(history[0]) * (HISTORY - 1));
    history[0] = h;
    if (history_n < HISTORY) history_n++;
}

static void tick(uint32_t dt_ms) {
    if (done) return;
    step_acc += dt_ms;
    if (step_acc >= STEP_MS) { step_acc = 0; step(); }
}

static void request_stop() { dissolving = stop_requested = true; }
static bool is_done()      { return done; }

static void touch(SceneTouch type, int x, int y) {
    if (done || type == SCENE_TOUCH_RELEASE) return;
    int c = (x - ox) / CELL, r = (y - oy) / CELL;
    // A small plus-shaped seed under the finger; drags draw a trail of them
    set_cell(c, r, true);
    set_cell(c - 1, r, true);
    set_cell(c + 1, r, true);
    set_cell(c, r - 1, true);
    set_cell(c, r + 1, true);
    history_n = 0;   // the pattern changed, so it isn't stuck any more
    if (!stop_requested) dissolving = false;
    lv_obj_invalidate(canvas);
}

static void finish() {
    free(buf);  buf  = nullptr;
    free(grid); grid = nullptr;
    free(next); next = nullptr;
    canvas = nullptr;   // already deleted along with the scene area
}

const Scene scene_life = { "life", start, tick, request_stop, is_done, touch, finish };
