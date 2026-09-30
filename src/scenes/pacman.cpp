#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>
#include <string.h>

// Pac-Man — a mini maze generated to fit the area: horizontal corridors joined
// by mirrored connectors, dots everywhere, power pellets in the corners. Pac-Man
// plays itself (path-finds to the nearest dot while keeping clear of ghosts,
// hunts frightened ones), the ghosts chase with their classic personalities.
// Finishes on its own when the maze is cleared or the last life is lost; when
// asked to stop, the maze flashes and it's done.
// Touch: tap or drag on a side of Pac-Man to steer that way. The computer takes
// back over AUTO_RESUME_MS after your last touch.

static const uint32_t AUTO_RESUME_MS = 5000;
static const int      T = 6;                 // tile size, px
static const int      MAX_C = 40, MAX_R = 54;       // enough for a fullscreen 240×320 maze
static const int      N_GHOSTS = 4;
static const float    PAC_SPEED = 30.0f, GHOST_SPEED = 26.0f, FRIGHT_SPEED = 16.0f, EYES_SPEED = 60.0f;
static const float    FRIGHT_S = 6.0f;
static const int      LIVES = 3;

enum Dir { RIGHT, DOWN, LEFT, UP, NONE };
static const int DX[5] = { 1, 0, -1, 0, 0 };
static const int DY[5] = { 0, 1, 0, -1, 0 };

struct Mover { int c, r; Dir d; float p; };   // at tile (c,r), p px of the way toward d
struct Rect  { int x, y, w, h; };
enum GhostMode { G_WAIT, G_CHASE, G_FRIGHT, G_EYES };
struct Ghost { Mover m; GhostMode mode; float wait; lv_color_t col; Rect prev; };

static PixFb    fb;
static int      cols, rows, ox, oy, area_w, area_h;
static bool     open_[MAX_R][MAX_C], dot[MAX_R][MAX_C];
static int      dots_left, home_c, home_r;
static Mover    pac;
static Dir      want;
static Rect     pac_prev;
static Ghost    ghosts[N_GHOSTS];
static int      lives;
static float    fright_t, mouth_t, blink_t, dying_t, flash_t;
static bool     dying, flashing, stopping, done, oom;
static uint32_t since_touch;

// Mouth frames facing right (closed, half, open); other directions are rotations
static const uint8_t PAC_R[3][5] = {
    { 0b01110, 0b11111, 0b11111, 0b11111, 0b01110 },
    { 0b01110, 0b11110, 0b11100, 0b11110, 0b01110 },
    { 0b01110, 0b11100, 0b11000, 0b11100, 0b01110 },
};
static const uint8_t GHOST_BODY[5] = { 0b01110, 0b11111, 0b11111, 0b11111, 0b10101 };

// ---------------------------------------------------------------------------
// Maze

static bool is_open(int c, int r) { return c >= 0 && r >= 0 && c < cols && r < rows && open_[r][c]; }
static bool can_go(int c, int r, Dir d) { return d != NONE && is_open(c + DX[d], r + DY[d]); }
static int  tile_x(int c) { return ox + c * T; }
static int  tile_y(int r) { return oy + r * T; }

static void build_maze() {
    memset(open_, 0, sizeof(open_));
    int last = 1;
    for (int r = 1; r <= rows - 2; r += 3) {
        for (int c = 1; c <= cols - 2; c++) open_[r][c] = true;
        last = r;
    }
    // Connectors between neighbouring corridors: both edges, plus mirrored
    // pairs in between at random columns
    for (int r = 1; r + 3 <= last; r += 3) {
        int picks[2] = { (int)random(3, cols / 4), (int)random(cols / 4 + 2, cols / 2 - 1) };
        int xs[6] = { 1, cols - 2, picks[0], cols - 1 - picks[0], picks[1], cols - 1 - picks[1] };
        for (int i = 0; i < 6; i++) open_[r + 1][xs[i]] = open_[r + 2][xs[i]] = true;
    }
    home_c = cols / 2;
    home_r = 1 + ((last - 1) / 3 / 2) * 3;   // the middle corridor
    dots_left = 0;
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) {
        dot[r][c] = open_[r][c] && !(r == home_r && abs(c - home_c) <= 3);
        dots_left += dot[r][c];
    }
}

static bool is_pellet(int c, int r) {
    int last = 1 + ((rows - 3) / 3) * 3;
    return (c == 1 || c == cols - 2) && (r == 1 || r == last);
}

static void draw_walls(lv_color_t col) {
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) {
        if (open_[r][c]) continue;
        int x = tile_x(c), y = tile_y(r);
        if (is_open(c, r - 1)) pixfb_line(fb, x, y + 1, x + T - 1, y + 1, col);
        if (is_open(c, r + 1)) pixfb_line(fb, x, y + T - 2, x + T - 1, y + T - 2, col);
        if (is_open(c - 1, r)) pixfb_line(fb, x + 1, y, x + 1, y + T - 1, col);
        if (is_open(c + 1, r)) pixfb_line(fb, x + T - 2, y, x + T - 2, y + T - 1, col);
    }
}

// Redraw the dot / pellet in one tile after something moved over it
static void draw_tile_content(int c, int r) {
    if (!is_open(c, r)) return;
    int x = tile_x(c), y = tile_y(r);
    pixfb_fill(fb, x, y, T - 1, T - 1, COL_SCENE_ARCADE_BG);
    if (!dot[r][c]) return;
    if (is_pellet(c, r)) {
        if (blink_t < 0.25f) pixfb_fill(fb, x + 1, y + 1, 3, 3, COL_SCENE_PAC_DOT);
    } else {
        pixfb_fill(fb, x + 2, y + 2, 1, 1, COL_SCENE_PAC_DOT);
    }
}

// ---------------------------------------------------------------------------
// Movement

static void pos_px(const Mover &m, float &x, float &y) {
    x = tile_x(m.c) + m.p * DX[m.d];   // DX/DY[NONE] are 0
    y = tile_y(m.r) + m.p * DY[m.d];
}

// Advance along the grid; `choose` picks a direction at each tile centre
template <typename F>
static void advance(Mover &m, float dist, F choose) {
    while (dist > 0) {
        if (m.p == 0) {
            Dir d = choose(m);
            m.d = can_go(m.c, m.r, d) ? d : NONE;
            if (m.d == NONE) return;
        }
        float step = fminf(dist, T - m.p);
        m.p += step;
        dist -= step;
        if (m.p >= T) { m.c += DX[m.d]; m.r += DY[m.d]; m.p = 0; }
    }
}

static Dir reverse(Dir d) { return d == NONE ? NONE : (Dir)((d + 2) % 4); }

static bool ghost_near(int c, int r, int radius) {
    for (int i = 0; i < N_GHOSTS; i++) {
        const Ghost &g = ghosts[i];
        if (g.mode != G_CHASE && g.mode != G_WAIT) continue;
        if (abs(g.m.c - c) + abs(g.m.r - r) <= radius) return true;
    }
    return false;
}

// Breadth-first search from Pac-Man to the nearest wanted tile, avoiding
// tiles next to dangerous ghosts. Returns the first step, or NONE.
static Dir pac_brain(const Mover &m) {
    static uint8_t first[MAX_R][MAX_C];
    static uint8_t seen[MAX_R][MAX_C];
    static int16_t queue[MAX_R * MAX_C];
    memset(seen, 0, sizeof(seen));
    int head = 0, tail = 0;
    seen[m.r][m.c] = 1;
    for (int d = 0; d < 4; d++) {
        int c = m.c + DX[d], r = m.r + DY[d];
        if (!is_open(c, r) || seen[r][c] || ghost_near(c, r, 1)) continue;
        seen[r][c] = 1; first[r][c] = d; queue[tail++] = r * MAX_C + c;
    }
    while (head < tail) {
        int c = queue[head] % MAX_C, r = queue[head] / MAX_C;
        head++;
        bool target = dot[r][c];
        if (fright_t > 1.0f)
            for (int i = 0; i < N_GHOSTS; i++)
                if (ghosts[i].mode == G_FRIGHT && ghosts[i].m.c == c && ghosts[i].m.r == r) target = true;
        if (target) return (Dir)first[r][c];
        for (int d = 0; d < 4; d++) {
            int nc = c + DX[d], nr = r + DY[d];
            if (!is_open(nc, nr) || seen[nr][nc] || ghost_near(nc, nr, 1)) continue;
            seen[nr][nc] = 1; first[nr][nc] = first[r][c]; queue[tail++] = nr * MAX_C + nc;
        }
    }
    // Boxed in: any way that isn't straight into a ghost
    for (int d = 0; d < 4; d++) if (can_go(m.c, m.r, (Dir)d) && !ghost_near(m.c + DX[d], m.r + DY[d], 0)) return (Dir)d;
    return NONE;
}

static Dir ghost_brain(const Ghost &g, int idx) {
    const Mover &m = g.m;
    int tc = pac.c, tr = pac.r;
    if (g.mode == G_EYES) { tc = home_c; tr = home_r; }
    else if (idx == 1)    { tc = pac.c + 4 * DX[pac.d == NONE ? 0 : pac.d]; tr = pac.r + 4 * DY[pac.d == NONE ? 0 : pac.d]; }
    else if (idx >= 2 && random(0, 3) == 0) { tc = random(0, cols); tr = random(0, rows); }

    Dir best = NONE;
    float best_d = 1e9f;
    int options = 0;
    for (int d = 0; d < 4; d++) {
        if (!can_go(m.c, m.r, (Dir)d)) continue;
        if ((Dir)d == reverse(m.d)) continue;   // ghosts don't turn back
        options++;
        float dx = m.c + DX[d] - tc, dy = m.r + DY[d] - tr;
        float dist = g.mode == G_FRIGHT ? scene_randf(0, 1) : dx * dx + dy * dy;
        if (dist < best_d) { best_d = dist; best = (Dir)d; }
    }
    return options ? best : reverse(m.d);   // dead end
}

// ---------------------------------------------------------------------------

static void reset_positions() {
    pac = { home_c, 1 + ((rows - 3) / 3) * 3, LEFT, 0 };   // bottom corridor
    want = LEFT;
    for (int i = 0; i < N_GHOSTS; i++) {
        Ghost &g = ghosts[i];
        g.m = { home_c - 2 + i + (i >= 2), home_r, NONE, 0 };
        g.mode = G_WAIT;
        g.wait = 1.0f + i * 2.5f;
    }
    fright_t = 0;
}

static void start(lv_obj_t *area, int w, int h) {
    area_w = w;
    area_h = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_ARCADE_BG);
    done = oom;
    if (oom) return;
    cols = w / T; if (cols > MAX_C) cols = MAX_C;
    rows = h / T; if (rows > MAX_R) rows = MAX_R;
    ox = (w - cols * T) / 2;
    oy = (h - rows * T) / 2;
    build_maze();
    draw_walls(COL_SCENE_PAC_WALL);
    for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) draw_tile_content(c, r);

    lv_color_t cols4[N_GHOSTS] = { COL_SCENE_PAC_GHOST_1, COL_SCENE_PAC_GHOST_2, COL_SCENE_PAC_GHOST_3, COL_SCENE_PAC_GHOST_4 };
    for (int i = 0; i < N_GHOSTS; i++) { ghosts[i].col = cols4[i]; ghosts[i].prev = { 0, 0, 0, 0 }; }
    pac_prev = { 0, 0, 0, 0 };
    lives = LIVES;
    reset_positions();
    mouth_t = blink_t = 0;
    dying = flashing = stopping = false;
    since_touch = AUTO_RESUME_MS;
}

static void begin_flash() { flashing = true; flash_t = 0; }

static void update(float dt) {
    blink_t += dt; if (blink_t >= 0.5f) blink_t = 0;

    if (flashing) {
        flash_t += dt;
        bool on = ((int)(flash_t * 5)) & 1;
        draw_walls(on ? COL_SCENE_PAC_FLASH : COL_SCENE_PAC_WALL);
        if (flash_t > 1.6f) done = true;
        return;
    }
    if (dying) {
        dying_t += dt;
        if (dying_t > 1.2f) {
            dying = false;
            if (--lives <= 0) { begin_flash(); return; }
            reset_positions();
        }
        return;
    }

    mouth_t += dt * 10;
    if (fright_t > 0) fright_t -= dt;

    // Pac-Man
    advance(pac, PAC_SPEED * dt, [](const Mover &m) {
        if (since_touch >= AUTO_RESUME_MS) want = pac_brain(m);
        return can_go(m.c, m.r, want) ? want : m.d;
    });
    if (pac.p < T / 2.0f && dot[pac.r][pac.c]) {
        dot[pac.r][pac.c] = false;
        dots_left--;
        if (is_pellet(pac.c, pac.r)) {
            fright_t = FRIGHT_S;
            for (int i = 0; i < N_GHOSTS; i++)
                if (ghosts[i].mode == G_CHASE) { ghosts[i].mode = G_FRIGHT; ghosts[i].m.d = reverse(ghosts[i].m.d); }
        }
        if (dots_left == 0) { begin_flash(); return; }
    }

    // Ghosts
    float px, py;
    pos_px(pac, px, py);
    for (int i = 0; i < N_GHOSTS; i++) {
        Ghost &g = ghosts[i];
        if (g.mode == G_WAIT) { if ((g.wait -= dt) <= 0) g.mode = fright_t > 0 ? G_FRIGHT : G_CHASE; continue; }
        if (g.mode == G_FRIGHT && fright_t <= 0) g.mode = G_CHASE;
        float speed = g.mode == G_FRIGHT ? FRIGHT_SPEED : g.mode == G_EYES ? EYES_SPEED : GHOST_SPEED;
        advance(g.m, speed * dt, [&](const Mover &) { return ghost_brain(g, i); });
        if (g.mode == G_EYES && g.m.c == home_c && g.m.r == home_r) g.mode = G_CHASE;

        float gx, gy;
        pos_px(g.m, gx, gy);
        if (fabsf(gx - px) < 4 && fabsf(gy - py) < 4) {
            if (g.mode == G_FRIGHT)     g.mode = G_EYES;
            else if (g.mode == G_CHASE) { dying = true; dying_t = 0; }
        }
    }
}

static void rot(const uint8_t *in, uint8_t *out, Dir d) {
    for (int r = 0; r < 5; r++) out[r] = 0;
    for (int r = 0; r < 5; r++) for (int c = 0; c < 5; c++) {
        if (!(in[r] & (1 << (4 - c)))) continue;
        int nr = r, nc = c;
        if (d == LEFT) nc = 4 - c;
        if (d == DOWN) { nr = c; nc = 4 - r; }
        if (d == UP)   { nr = 4 - c; nc = r; }
        out[nr] |= 1 << (4 - nc);
    }
}

static void sprite5(int x, int y, const uint8_t *rows5, lv_color_t col) {
    uint16_t r16[5];
    for (int i = 0; i < 5; i++) r16[i] = rows5[i];
    pixfb_sprite(fb, x, y, r16, 5, 5, col);
}

static void erase_rect(Rect &r) {
    if (r.w <= 0) return;
    pixfb_fill(fb, r.x, r.y, r.w, r.h, COL_SCENE_ARCADE_BG);
    // Put back the dots under it
    for (int c = (r.x - ox) / T; c <= (r.x + r.w - 1 - ox) / T; c++)
        for (int rr = (r.y - oy) / T; rr <= (r.y + r.h - 1 - oy) / T; rr++) draw_tile_content(c, rr);
    r = { 0, 0, 0, 0 };
}

static void draw() {
    if (flashing) return;
    erase_rect(pac_prev);
    for (int i = 0; i < N_GHOSTS; i++) erase_rect(ghosts[i].prev);
    // Pellets blink on their own
    int last = 1 + ((rows - 3) / 3) * 3;
    int pc[4][2] = { { 1, 1 }, { cols - 2, 1 }, { 1, last }, { cols - 2, last } };
    for (int i = 0; i < 4; i++) draw_tile_content(pc[i][0], pc[i][1]);

    float x, y;
    for (int i = 0; i < N_GHOSTS; i++) {
        Ghost &g = ghosts[i];
        pos_px(g.m, x, y);
        int gx = (int)x, gy = (int)y;
        if (g.mode == G_EYES) {
            pixfb_fill(fb, gx + 1, gy + 1, 1, 1, COL_SCENE_PAC_EATEN);
            pixfb_fill(fb, gx + 3, gy + 1, 1, 1, COL_SCENE_PAC_EATEN);
        } else {
            bool ending = g.mode == G_FRIGHT && fright_t < 2.0f && ((int)(fright_t * 6) & 1);
            lv_color_t body = g.mode == G_FRIGHT ? (ending ? COL_SCENE_PAC_EATEN : COL_SCENE_PAC_FRIGHT) : g.col;
            sprite5(gx, gy, GHOST_BODY, body);
            pixfb_fill(fb, gx + 1, gy + 1, 1, 1, COL_SCENE_PAC_EYES);
            pixfb_fill(fb, gx + 3, gy + 1, 1, 1, COL_SCENE_PAC_EYES);
        }
        g.prev = { gx, gy, 5, 5 };
    }

    pos_px(pac, x, y);
    uint8_t frame[5];
    if (dying) {
        // Shrink away
        int keep = 5 - (int)(dying_t / 1.2f * 5);
        for (int r = 0; r < 5; r++) frame[r] = (r < (5 - keep) / 2 || r >= (5 + keep) / 2) ? 0 : PAC_R[0][r];
    } else {
        int f = ((int)mouth_t) % 4;
        rot(PAC_R[f == 3 ? 1 : f], frame, pac.d == NONE ? RIGHT : pac.d);
    }
    sprite5((int)x, (int)y, frame, COL_SCENE_PAC_PACMAN);
    pac_prev = { (int)x, (int)y, 5, 5 };
}

static void tick(uint32_t dt_ms) {
    if (oom || done) return;
    since_touch += dt_ms;
    update(dt_ms / 1000.0f);
    draw();
    pixfb_flush(fb);
}

static void request_stop() {
    if (!flashing) { stopping = true; begin_flash(); }
}

static bool is_done() { return done; }

static void touch(SceneTouch type, int x, int y) {
    if (type == SCENE_TOUCH_RELEASE) return;
    since_touch = 0;
    float px, py;
    pos_px(pac, px, py);
    float dx = x - (px + 2), dy = y - (py + 2);
    if (fabsf(dx) < 3 && fabsf(dy) < 3) return;
    want = fabsf(dx) > fabsf(dy) ? (dx > 0 ? RIGHT : LEFT) : (dy > 0 ? DOWN : UP);
}

static void finish() { pixfb_free(fb); }

const Scene scene_pacman = { "pacman", start, tick, request_stop, is_done, touch, finish, nullptr, true };
SCENE_REGISTER(scene_pacman, "Pac-Man");
