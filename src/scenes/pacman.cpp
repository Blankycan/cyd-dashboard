#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Pac-Man — Namco, 1980: the arcade maze, upright and tile for tile at 5 px
// a tile. It's taller than the area, so the view scrolls up and down after
// Pac-Man, only once he nears the top or bottom third of it. 1UP and the
// score are on the left with the lives under them; the high score (since the
// board started) and the fruit of the last few levels are on the right.
// Kept to the arcade rules, as Jamey Pittman's Pac-Man Dossier has them:
// every level's speeds (ghosts slower in the tunnel, Pac-Man pausing a frame
// for each dot he eats), the scatter / chase schedule and the reversal at
// every change, each ghost's target (Blinky straight at Pac-Man, Pinky four
// tiles ahead with the arcade's bug when he faces up, Inky off Blinky,
// Clyde backing off inside eight tiles), no turning up at the four spots
// above the house and above Pac-Man's start, the house's dot counters and
// its timer, Cruise Elroy, each level's fright time and flashes, and
// Pac-Man cutting corners. 10 a dot, 50 an energizer, 200 / 400 / 800 /
// 1600 a ghost, the level's fruit after 70 and 170 dots, an extra life at
// 10,000, three lives; then GAME OVER and a new game.
// It plays itself, the way the experts do it: the ghosts follow fixed rules,
// so it plays each way it could go a couple of seconds ahead, ghosts and
// all, and takes the one that eats the most without getting caught or
// boxed in, saving the energizers for when there are ghosts close enough to
// eat, then hunting the blue ones down in the order that catches the most. When asked to stop, the maze flashes and it's done.
// Touch: tap or drag on a side of Pac-Man to steer that way. The computer
// takes back over AUTO_RESUME_MS after your last touch.

static const uint32_t AUTO_RESUME_MS = 5000;
static const int      COLS = 28, ROWS = 31, T = 5;   // the arcade maze, 5 px a tile
static const int      MAZE_W = COLS * T, MAZE_H = ROWS * T, WALL_WORDS = (MAZE_W + 31) / 32;
static const int      N_GHOSTS = 4, LIVES = 3, EXTRA_LIFE = 10000;
static const float    FULL_SPEED = 75.75757625f / 8 / 60;   // the arcade's "100%", in tiles a frame (60 a second)
static const float    HOUSE_PCT = 50, EYES_PCT = 150;
static const int      TUNNEL_R = 14, DOOR_R = 11, FRUIT_R = 17;   // rows: the tunnel, over the door, the fruit and READY!
static const float    DOOR_X = 14.0f, HOUSE_Y = 14.5f, EXIT_Y = 11.5f, PAC_X = 14.0f, PAC_Y = 23.5f;
static const int      READY_F = 120, FLASH_F = 28;   // frames: READY!, one flash of a ghost about to turn back
static const float    PI_F = 3.14159265f;

enum Dir { UP, LEFT, DOWN, RIGHT, NONE };   // the order ghosts break ties in
static const int DX[5] = { 0, -1, 0, 1, 0 };
static const int DY[5] = { -1, 0, 1, 0, 0 };
static Dir reverse(Dir d) { return d == NONE ? NONE : (Dir)((d + 2) % 4); }

// '#' wall, '.' dot, 'o' energizer, ' ' open, '-' the house door, 'h' inside
// the house, 'x' outside the maze
static const char *const MAZE[ROWS] = {
    "############################",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o####.#####.##.#####.####o#",
    "#.####.#####.##.#####.####.#",
    "#..........................#",
    "#.####.##.########.##.####.#",
    "#.####.##.########.##.####.#",
    "#......##....##....##......#",
    "######.##### ## #####.######",
    "xxxxx#.##### ## #####.#xxxxx",
    "xxxxx#.##          ##.#xxxxx",
    "xxxxx#.## ###--### ##.#xxxxx",
    "######.## #hhhhhh# ##.######",
    "      .   #hhhhhh#   .      ",
    "######.## #hhhhhh# ##.######",
    "xxxxx#.## ######## ##.#xxxxx",
    "xxxxx#.##          ##.#xxxxx",
    "xxxxx#.## ######## ##.#xxxxx",
    "######.## ######## ##.######",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#.####.#####.##.#####.####.#",
    "#o..##.......  .......##..o#",
    "###.##.##.########.##.##.###",
    "###.##.##.########.##.##.###",
    "#......##....##....##......#",
    "#.##########.##.##########.#",
    "#.##########.##.##########.#",
    "#..........................#",
    "############################",
};

// Speeds in % of full, fright seconds and flashes, Elroy's dots-left
// thresholds and speeds, and the fruit; the last row is level 21 onwards
struct Level { uint8_t pac, pac_fr, ghost, tunnel, ghost_fr, fright_s, flashes, elroy1_dots, elroy2_dots, elroy1, elroy2, fruit; };
static const Level LEVELS[21] = {
    {  80,  90, 75, 40, 50, 6, 5,  20, 10,  80,  85, 0 },
    {  90,  95, 85, 45, 55, 5, 5,  30, 15,  90,  95, 1 },
    {  90,  95, 85, 45, 55, 4, 5,  40, 20,  90,  95, 2 },
    {  90,  95, 85, 45, 55, 3, 5,  40, 20,  90,  95, 2 },
    { 100, 100, 95, 50, 60, 2, 5,  40, 20, 100, 105, 3 },
    { 100, 100, 95, 50, 60, 5, 5,  50, 25, 100, 105, 3 },
    { 100, 100, 95, 50, 60, 2, 5,  50, 25, 100, 105, 4 },
    { 100, 100, 95, 50, 60, 2, 5,  50, 25, 100, 105, 4 },
    { 100, 100, 95, 50, 60, 1, 3,  60, 30, 100, 105, 5 },
    { 100, 100, 95, 50, 60, 5, 5,  60, 30, 100, 105, 5 },
    { 100, 100, 95, 50, 60, 2, 5,  60, 30, 100, 105, 6 },
    { 100, 100, 95, 50, 60, 1, 3,  80, 40, 100, 105, 6 },
    { 100, 100, 95, 50, 60, 1, 3,  80, 40, 100, 105, 7 },
    { 100, 100, 95, 50, 60, 3, 5,  80, 40, 100, 105, 7 },
    { 100, 100, 95, 50, 60, 1, 3, 100, 50, 100, 105, 7 },
    { 100, 100, 95, 50, 60, 1, 3, 100, 50, 100, 105, 7 },
    { 100, 100, 95, 50, 60, 0, 0, 100, 50, 100, 105, 7 },
    { 100, 100, 95, 50, 60, 1, 3, 100, 50, 100, 105, 7 },
    { 100, 100, 95, 50, 60, 0, 0, 120, 60, 100, 105, 7 },
    { 100, 100, 95, 50, 60, 0, 0, 120, 60, 100, 105, 7 },
    {  90,  90, 95, 50, 60, 0, 0, 120, 60, 100, 105, 7 },
};
static const int FRUIT_PTS[8] = { 100, 300, 500, 700, 1000, 2000, 3000, 5000 };
// Scatter, chase, scatter, ... in frames, for level 1, levels 2-4 and 5 on;
// after the seventh it's chase for good
static const uint16_t MODE_F[3][7] = {
    { 420, 1200, 420, 1200, 300, 1200,  300 },
    { 420, 1200, 420, 1200, 300, 61980,   1 },
    { 300, 1200, 300, 1200, 300, 62220,   1 },
};
static const int   CORNER[N_GHOSTS][2] = { { 25, -3 }, { 2, -3 }, { 27, 32 }, { 0, 32 } };   // scatter targets
static const float HOME_X[N_GHOSTS]    = { DOOR_X, DOOR_X, 12.0f, 16.0f };                  // places in the house

enum GState { G_HOUSE, G_LEAVING, G_ACTIVE, G_EYES, G_ENTERING };
struct Ghost { float x, y; Dir d; GState st; bool fright; int dots; };   // x, y in tiles: a tile's centre is at +0.5
struct Pac   { float x, y; Dir d; };
// Everything that moves on from frame to frame (bar the dots), so the
// computer can copy it and play ahead
struct World {
    Pac   pac;
    Ghost ghosts[N_GHOSTS];
    int   mode_idx, mode_f, fright_f, combo, pac_pause, dots_left, dots_eaten, since_dot, global_dots, fruit_f;
    bool  global_on, elroy_wait;
};
enum Phase { PH_READY, PH_PLAY, PH_EAT, PH_CAUGHT, PH_DYING, PH_OVER, PH_CLEARED, PH_FLASH, PH_STOP };
enum Event { EV_NONE, EV_CAUGHT, EV_GHOST, EV_CLEARED };
struct Outcome { Event ev; int dots, energizers, ghost, pts; bool fruit; };

// Scratch space, allocated while the scene runs
struct Work {
    uint32_t walls[MAZE_H][WALL_WORDS];   // the wall outlines, a bit a pixel
    bool     dots[ROWS][COLS];            // the computer's copy of the dots, eaten as it plays ahead
    uint16_t undo[512];                   // ... and the ones to put back
    int16_t  dot_dist[ROWS][COLS];        // tiles to the nearest dot
    uint8_t  blue_dist[N_GHOSTS][ROWS][COLS];   // tiles to each blue ghost (where it was when planning)
    bool     blue_ok[N_GHOSTS];                 // ... for the ghosts that were blue then
    uint16_t q[ROWS * COLS];
};

static PixFb      fb;
static Work      *wk;
static World      live;
static bool       dot[ROWS][COLS];
static int        W, H, left_x, top_y, view_h, view_y;
static Dir        want;
static Phase      phase;
static int        phase_f, frames, level, lives, eaten, fruit_pts_f, pts_val, plan_c, plan_r, undo_n, hungry_f;
static long       score, top_score, plan_sit;   // top: best since the board started
static float      frame_acc, pac_anim, pts_x, pts_y;
static bool       extra_given, stopping, done, oom;
static uint32_t   since_touch;
static long       hud_score, hud_top;
static int        hud_lives, hud_level, hud_up;
static lv_color_t ghost_col[N_GHOSTS], fruit_pal[7];

// ---------------------------------------------------------------------------
// The maze

static int  ifloor(float v) { int i = (int)v; return i - (v < i); }   // cheaper than floorf() on the board
static int  tile(float v) { return ifloor(v); }
static int  wrap_c(int c) { return c < 0 ? c + COLS : c >= COLS ? c - COLS : c; }
static char cell(int c, int r) {
    if (r < 0 || r >= ROWS) return 'x';
    if (c < 0 || c >= COLS) { if (r != TUNNEL_R) return 'x'; c = wrap_c(c); }   // the tunnel comes out the other side
    return MAZE[r][c];
}
static bool open_tile(int c, int r) { char k = cell(c, r); return k == '.' || k == 'o' || k == ' '; }
static bool energizer(int c, int r) { return cell(c, r) == 'o'; }
static int  exits(int c, int r) { int n = 0; for (int d = 0; d < 4; d++) n += open_tile(c + DX[d], r + DY[d]); return n; }
static const Level &spec() { return LEVELS[level < 21 ? level - 1 : 20]; }

// The walls are drawn as the arcade draws them: thin outlines around each
// block, a pixel in from the corridors, with the corners rounded off. The
// outer wall, with nothing but the edge behind it, gets a double line.
static bool walkable_px(int x, int y) {
    if (x < 0 || y < 0 || x >= MAZE_W || y >= MAZE_H) return false;
    char k = MAZE[y / T][x / T];
    return k != '#' && k != 'x';
}
static bool solid_px(int x, int y) {
    if (x < 0 || y < 0 || x >= MAZE_W || y >= MAZE_H || MAZE[y / T][x / T] != '#') return false;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            if (walkable_px(x + dx, y + dy)) return false;
    return true;
}
static void build_walls() {
    memset(wk->walls, 0, sizeof(wk->walls));
    for (int y = 0; y < MAZE_H; y++)
        for (int x = 0; x < MAZE_W; x++) {
            if (!solid_px(x, y)) continue;
            bool u = !solid_px(x, y - 1), d = !solid_px(x, y + 1), l = !solid_px(x - 1, y), r = !solid_px(x + 1, y);
            if (!(u || d || l || r)) continue;                        // inside a block
            if (u + d + l + r == 2 && u != d) continue;               // a corner: cut off, so it rounds
            wk->walls[y][x >> 5] |= 1u << (x & 31);
        }
}

// ---------------------------------------------------------------------------
// The rules: one frame of the game, on a World and a set of dots

static bool in_tunnel(const Ghost &g) {
    int c = wrap_c(tile(g.x));
    return tile(g.y) == TUNNEL_R && (c <= 5 || c >= 22);
}
static int elroy(const World &w) {   // Blinky's Cruise Elroy stage: 0, 1 or 2
    if (w.elroy_wait) return 0;
    const Level &L = spec();
    return w.dots_left <= L.elroy2_dots ? 2 : w.dots_left <= L.elroy1_dots ? 1 : 0;
}
static float ghost_pct(const World &w, int i) {
    const Ghost &g = w.ghosts[i];
    const Level &L = spec();
    if (g.st == G_EYES || g.st == G_ENTERING) return EYES_PCT;
    if (g.st == G_HOUSE || g.st == G_LEAVING) return HOUSE_PCT;
    if (in_tunnel(g)) return L.tunnel;
    if (g.fright) return L.ghost_fr;
    int e = i == 0 ? elroy(w) : 0;
    return e == 2 ? L.elroy2 : e == 1 ? L.elroy1 : L.ghost;
}
static float pac_pct(const World &w) { return w.fright_f > 0 ? spec().pac_fr : spec().pac; }
static int   mode_len(int idx) { return MODE_F[level == 1 ? 0 : level < 5 ? 1 : 2][idx]; }

// The ghost house: the first ghost still in it (Pinky, Inky, then Clyde)
// counts the dots Pac-Man eats and comes out at its limit. After a life is
// lost one shared counter lets them out at 7, 17 and 32 dots instead. And if
// Pac-Man goes too long without eating, the next one comes out anyway.
static int preferred(const World &w) {
    for (int i = 1; i < N_GHOSTS; i++) if (w.ghosts[i].st == G_HOUSE) return i;
    return -1;
}
static int dot_limit(int i) {
    if (i == 1 || level >= 3) return 0;
    if (i == 2) return level == 1 ? 30 : 0;
    return level == 1 ? 60 : 50;
}
static void release(World &w, int i) { if (i >= 0 && w.ghosts[i].st == G_HOUSE) w.ghosts[i].st = G_LEAVING; }

static void count_dot(World &w) {
    w.since_dot = 0;
    if (w.global_on) {
        w.global_dots++;
        if (w.global_dots == 7)       release(w, 1);
        else if (w.global_dots == 17) release(w, 2);
        else if (w.global_dots == 32) { release(w, 3); w.global_on = false; }
    } else {
        int p = preferred(w);
        if (p >= 0) w.ghosts[p].dots++;
    }
}

static void check_house(World &w, int k) {
    if (!w.global_on) {
        int p = preferred(w);
        if (p >= 0 && w.ghosts[p].dots >= dot_limit(p)) release(w, p);
    }
    if ((w.since_dot += k) >= (level < 5 ? 240 : 180)) { w.since_dot = 0; release(w, preferred(w)); }
}

static void target(const World &w, int i, int &tc, int &tr) {
    const Ghost &g = w.ghosts[i];
    if (g.st == G_EYES) { tc = 13; tr = DOOR_R; return; }
    int pc = wrap_c(tile(w.pac.x)), pr = tile(w.pac.y);
    Dir pd = w.pac.d == NONE ? LEFT : w.pac.d;
    if (!(w.mode_idx & 1) && !(i == 0 && elroy(w))) { tc = CORNER[i][0]; tr = CORNER[i][1]; return; }   // scatter (Elroy keeps chasing)
    switch (i) {
    case 0:   // Blinky: Pac-Man himself
        tc = pc; tr = pr;
        break;
    case 1:   // Pinky: four ahead of him (facing up, four to the left as well: the arcade's overflow bug)
        tc = pc + 4 * DX[pd] - (pd == UP ? 4 : 0); tr = pr + 4 * DY[pd];
        break;
    case 2: { // Inky: from Blinky through two ahead of Pac-Man, and as far again
        int ac = pc + 2 * DX[pd] - (pd == UP ? 2 : 0), ar = pr + 2 * DY[pd];
        tc = 2 * ac - wrap_c(tile(w.ghosts[0].x)); tr = 2 * ar - tile(w.ghosts[0].y);
        break;
    }
    default: { // Clyde: Pac-Man, until he's within eight tiles; then home to his corner
        int dx = wrap_c(tile(g.x)) - pc, dy = tile(g.y) - pr;
        if (dx * dx + dy * dy >= 64) { tc = pc; tr = pr; } else { tc = CORNER[3][0]; tr = CORNER[3][1]; }
    }
    }
}

static bool no_up_zone(int c, int r) { return (c == 12 || c == 15) && (r == 11 || r == 23); }

// At a tile's centre: the way that leaves it closest to the target, never
// back the way it came (frightened: any way at random)
static Dir choose(const World &w, int i) {
    const Ghost &g = w.ghosts[i];
    int c = wrap_c(tile(g.x)), r = tile(g.y), tc, tr;
    target(w, i, tc, tr);
    Dir opts[4], best = NONE;
    int n = 0;
    long best_d = 0;
    for (int d = 0; d < 4; d++) {
        if ((Dir)d == reverse(g.d) || !open_tile(c + DX[d], r + DY[d])) continue;
        if (d == UP && g.st == G_ACTIVE && !g.fright && no_up_zone(c, r)) continue;
        opts[n++] = (Dir)d;
        long dx = c + DX[d] - tc, dy = r + DY[d] - tr, dd = dx * dx + dy * dy;
        if (best == NONE || dd < best_d) { best = (Dir)d; best_d = dd; }   // ties: up, left, down, right
    }
    if (!n) return reverse(g.d);
    if (g.fright && g.st == G_ACTIVE) return opts[random(0, n)];
    return best;
}

static void step_toward(float &v, float to, float &budget) {
    float d = fabsf(to - v);
    if (budget >= d) { v = to; budget -= d; }
    else { v += to > v ? budget : -budget; budget = 0; }
}

// Along the corridors from tile centre to tile centre, deciding at each
static void grid_move(World &w, int i, float v) {
    Ghost &g = w.ghosts[i];
    while (v > 0) {
        bool horiz = g.d == LEFT || g.d == RIGHT;
        float &a = horiz ? g.x : g.y;
        int s = g.d == RIGHT || g.d == DOWN ? 1 : -1;
        float n = s > 0 ? ifloor(a + 0.5f) + 0.5f : -ifloor(0.5f - a) - 0.5f;   // the next centre ahead
        float gap = fabsf(n - a);
        if (v < gap) { a += s * v; return; }
        a = n;
        v -= gap;
        if (g.x < 0) g.x += COLS; else if (g.x > COLS) g.x -= COLS;   // through the tunnel
        int c = wrap_c(tile(g.x)), r = tile(g.y);
        if (g.st == G_EYES && r == DOOR_R && (c == 13 || c == 14)) { g.st = G_ENTERING; return; }
        g.d = choose(w, i);
    }
}

static void ghost_frame(World &w, int i, int k) {
    Ghost &g = w.ghosts[i];
    float v = ghost_pct(w, i) / 100 * FULL_SPEED * k;
    switch (g.st) {
    case G_HOUSE:   // bobbing up and down in its place
        g.y += g.d == UP ? -v : v;
        if (g.y <= HOUSE_Y - 0.5f) { g.y = HOUSE_Y - 0.5f; g.d = DOWN; }
        if (g.y >= HOUSE_Y + 0.5f) { g.y = HOUSE_Y + 0.5f; g.d = UP; }
        break;
    case G_LEAVING:   // to the middle of the house, then up and out through the door
        if (g.x != DOOR_X) {
            if (g.y != HOUSE_Y) { g.d = g.y < HOUSE_Y ? DOWN : UP; step_toward(g.y, HOUSE_Y, v); }
            if (v > 0) { g.d = g.x < DOOR_X ? RIGHT : LEFT; step_toward(g.x, DOOR_X, v); }
        }
        if (g.x == DOOR_X && v > 0) {
            g.d = UP;
            step_toward(g.y, EXIT_Y, v);
            if (g.y == EXIT_Y) { g.st = G_ACTIVE; g.d = LEFT; }
        }
        break;
    case G_ENTERING:   // eyes: over the door, down into the house, across to its place, and back to life
        if (g.y < HOUSE_Y) {
            step_toward(g.x, DOOR_X, v);
            if (v > 0) { g.d = DOWN; step_toward(g.y, HOUSE_Y, v); }
        }
        if (g.y == HOUSE_Y && v > 0) {
            if (g.x != HOME_X[i]) g.d = g.x < HOME_X[i] ? RIGHT : LEFT;
            step_toward(g.x, HOME_X[i], v);
            if (g.x == HOME_X[i]) g.st = G_LEAVING;
        }
        break;
    default:
        grid_move(w, i, v);
    }
}

static void pac_frame(World &w, Dir want, int k) {
    Pac &p = w.pac;
    int paused = w.pac_pause < k ? w.pac_pause : k;   // a frame for every dot, three for an energizer
    w.pac_pause -= paused;
    if (paused == k) return;
    float v = pac_pct(w) / 100 * FULL_SPEED * (k - paused);
    int c = tile(p.x), r = tile(p.y);
    if (want != NONE && want != p.d) {
        if (want == reverse(p.d)) p.d = want;
        else if (open_tile(c + DX[want], r + DY[want])) p.d = want;   // he turns early or late, cutting the corner
    }
    bool horiz = p.d == LEFT || p.d == RIGHT;
    float &a = horiz ? p.x : p.y, &o = horiz ? p.y : p.x;
    int s = p.d == RIGHT || p.d == DOWN ? 1 : -1;
    if (open_tile(c + DX[p.d], r + DY[p.d])) a += s * v;
    else {   // a wall ahead: as far as the middle of this tile
        float to = ifloor(a) + 0.5f - a;
        if (to * s > 0) a += s * fminf(v, fabsf(to));
    }
    float oc = ifloor(o) + 0.5f;   // and back onto the middle of the corridor after a corner
    if (o != oc) {
        float m = fminf(v, fabsf(oc - o));
        o = m == fabsf(oc - o) ? oc : o + (o < oc ? m : -m);
    }
    if (p.x < -0.5f) p.x += COLS; else if (p.x >= COLS + 0.5f) p.x -= COLS;
}

static void frighten(World &w) {   // an energizer: the ghosts turn round, and blue if the level still allows it
    w.fright_f = spec().fright_s * 60;
    w.combo = 0;
    for (Ghost &g : w.ghosts) {
        if (g.st == G_ACTIVE) g.d = reverse(g.d);
        if (w.fright_f && g.st != G_EYES && g.st != G_ENTERING) g.fright = true;
    }
}

static bool collide(World &w, Outcome &o) {
    int pc = wrap_c(tile(w.pac.x)), pr = tile(w.pac.y);
    for (int i = 0; i < N_GHOSTS; i++) {
        Ghost &g = w.ghosts[i];
        if (g.st != G_ACTIVE || wrap_c(tile(g.x)) != pc || tile(g.y) != pr) continue;
        if (g.fright) {
            g.fright = false;
            g.st = G_EYES;
            o.ev = EV_GHOST;
            o.ghost = i;
            o.pts = 200 << w.combo;
            if (w.combo < 3) w.combo++;
        } else o.ev = EV_CAUGHT;
        return true;
    }
    return false;
}

// k frames (the game runs one at a time; the computer, playing ahead, a few;
// at these speeds nothing can slip past anything else in that time).
// `undo`: note the dots eaten in wk->undo, to put back later.
static void step(World &w, bool (*dots)[COLS], Dir want, int k, Outcome &o, bool undo) {
    o = { EV_NONE, 0, 0, -1, 0, false };
    if (w.fright_f > 0) { if ((w.fright_f -= k) <= 0) { w.fright_f = 0; for (Ghost &g : w.ghosts) g.fright = false; } }
    else if (w.mode_idx < 7 && (w.mode_f += k) >= mode_len(w.mode_idx)) {   // the mode timer stands still while they're blue
        w.mode_f = 0;
        w.mode_idx++;
        for (Ghost &g : w.ghosts) if (g.st == G_ACTIVE) g.d = reverse(g.d);
    }
    if (w.elroy_wait && w.ghosts[3].st != G_HOUSE) w.elroy_wait = false;
    check_house(w, k);
    if (w.fruit_f > 0) w.fruit_f = w.fruit_f > k ? w.fruit_f - k : 0;

    pac_frame(w, want, k);
    int c = wrap_c(tile(w.pac.x)), r = tile(w.pac.y);
    if (w.fruit_f > 0 && r == FRUIT_R && (c == 13 || c == 14)) { w.fruit_f = 0; o.fruit = true; }
    if (dots[r][c]) {
        dots[r][c] = false;
        if (undo && undo_n < 512) wk->undo[undo_n++] = (uint16_t)(r * COLS + c);
        w.dots_left--;
        w.dots_eaten++;
        count_dot(w);
        if (energizer(c, r)) { o.energizers++; w.pac_pause = 3; frighten(w); }
        else                 { o.dots++; w.pac_pause = 1; }
        if (w.dots_eaten == 70 || w.dots_eaten == 170) w.fruit_f = 540 + random(0, 60);   // 9 to 10 s
        if (w.dots_left == 0) { o.ev = EV_CLEARED; return; }
    }
    if (collide(w, o)) return;
    for (int i = 0; i < N_GHOSTS; i++) ghost_frame(w, i, k);
    collide(w, o);
}

// ---------------------------------------------------------------------------
// The computer
//
// The ghosts go by fixed rules, so Pac-Man can be played ahead exactly (bar
// where a blue ghost wanders): from here, each way out is followed to the
// next junction, and every way on from there, and so on for LOOK_F frames,
// running the real game on a copy (a few frames a step, and at most BUDGET
// steps a look, shared out evenly between the ways at each junction, so it
// fits in a frame on the board). A line that gets him caught is out; of
// the rest, the one that eats the most soonest wins, with an energizer only
// worth it when there are ghosts close enough to eat. Where a line ends it's
// scored by how far it is from the ghosts and from the dots still left (the
// dots pulling harder the longer he goes without one, so he never just
// hides), or, while there are blue ghosts to catch, by what they're worth
// from there. With blue ghosts out it goes straight for them instead (see
// hunt()), the look ahead only there to keep him from getting caught.

static const int   LOOK_F = 150, LOOK_STEP = 3, MAX_DEPTH = 6, BUDGET = 900;
// How much of the fright's time to count on: when weighing up an energizer
// (it only takes one with ghosts close enough to be worth ENERGIZE_MIN), and
// when planning the order to eat the blue ghosts in
static const float ENERGIZE_REACH = 0.4f, ENERGIZE_MIN = 2000, HUNT_MARGIN = 0.85f;
// Once he's turned back he keeps going for TURN_LOCK tiles before turning
// back again, unless a ghost that can catch him is within LOCK_SAFE tiles
static const int   TURN_LOCK = 4, LOCK_SAFE = 6;

static void plan_blue_dist() {   // a search out from each blue ghost
    for (int i = 0; i < N_GHOSTS; i++) {
        const Ghost &g = live.ghosts[i];
        if (!(wk->blue_ok[i] = g.st == G_ACTIVE && g.fright)) continue;
        uint8_t (*dist)[COLS] = wk->blue_dist[i];
        memset(dist, 255, sizeof(wk->blue_dist[i]));
        int head = 0, tail = 0, gc = wrap_c(tile(g.x)), gr = tile(g.y);
        dist[gr][gc] = 0;
        wk->q[tail++] = (uint16_t)(gr * COLS + gc);
        while (head < tail) {
            int c = wk->q[head] % COLS, r = wk->q[head] / COLS;
            head++;
            for (int d = 0; d < 4; d++) {
                if (!open_tile(c + DX[d], r + DY[d])) continue;
                int nc = wrap_c(c + DX[d]), nr = r + DY[d];
                if (dist[nr][nc] != 255) continue;
                dist[nr][nc] = dist[r][c] + 1;
                wk->q[tail++] = (uint16_t)(nr * COLS + nc);
            }
        }
    }
}

// Tiles from (c, r) to ghost i: along the maze if it was blue when planning,
// otherwise as the crow flies (round the tunnel)
static int ghost_dist(const World &w, int i, int c, int r) {
    if (wk->blue_ok[i]) return wk->blue_dist[i][r][c];
    const Ghost &g = w.ghosts[i];
    int dx = abs(wrap_c(tile(g.x)) - c);
    return (dx < COLS - dx ? dx : COLS - dx) + abs(tile(g.y) - r);
}

// What the blue ghosts are still worth from (c, r): each one he can get to
// before the fright runs out, nearest first, at the doubling ghost points,
// counting for less the further it is. Blue ghosts don't run, they wander,
// so he catches them at about his own speed.
static float blue_value(const World &w, int c, int r, float reach = 0.8f) {
    if (w.fright_f <= 0) return 0;
    float tiles_left = w.fright_f * pac_pct(w) / 100 * FULL_SPEED * reach;
    int   ds[N_GHOSTS], n = 0;
    for (int i = 0; i < N_GHOSTS; i++)
        if (w.ghosts[i].st == G_ACTIVE && w.ghosts[i].fright) {
            int d = ghost_dist(w, i, c, r);
            if (d < tiles_left) ds[n++] = d;
        }
    for (int i = 1; i < n; i++) for (int j = i; j > 0 && ds[j] < ds[j - 1]; j--) { int t = ds[j]; ds[j] = ds[j - 1]; ds[j - 1] = t; }
    float v = 0;
    for (int i = 0, combo = w.combo; i < n; i++) {
        v += (200 << combo) * (1.0f - ds[i] / tiles_left);
        if (combo < 3) combo++;
    }
    return v;
}

static void plan_dot_dist() {   // a search out from every dot at once
    int head = 0, tail = 0;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            wk->dot_dist[r][c] = dot[r][c] ? 0 : -1;
            if (dot[r][c]) wk->q[tail++] = (uint16_t)(r * COLS + c);
        }
    while (head < tail) {
        int c = wk->q[head] % COLS, r = wk->q[head] / COLS;
        head++;
        for (int d = 0; d < 4; d++) {
            if (!open_tile(c + DX[d], r + DY[d])) continue;
            int nc = wrap_c(c + DX[d]), nr = r + DY[d];
            if (wk->dot_dist[nr][nc] >= 0) continue;
            wk->dot_dist[nr][nc] = wk->dot_dist[r][c] + 1;
            wk->q[tail++] = (uint16_t)(nr * COLS + nc);
        }
    }
}

static float leaf(const World &w) {
    int c = wrap_c(tile(w.pac.x)), r = tile(w.pac.y), near = 8;
    for (const Ghost &g : w.ghosts) {
        if (g.st != G_ACTIVE || g.fright) continue;
        int dx = abs(wrap_c(tile(g.x)) - c), m = (dx < COLS - dx ? dx : COLS - dx) + abs(tile(g.y) - r);
        if (m < near) near = m;
    }
    float blue = blue_value(w, c, r);
    if (blue > 0) return blue + near * 15;   // on the hunt: the dots can wait
    int dd = wk->dot_dist[r][c];
    float pull = 4 + hungry_f / 120.0f;
    return -(dd < 0 ? 30 : dd) * (pull < 40 ? pull : 40) + near * 15;
}

static float walk(World &w, Dir d, int used, int depth, float gain, int budget) {
    int mark = undo_n, pc = wrap_c(tile(w.pac.x)), pr = tile(w.pac.y);
    Dir way = d;
    float result;
    for (;;) {
        Outcome o;
        bool was_blue = w.fright_f > 0;
        step(w, wk->dots, way, LOOK_STEP, o, true);
        used += LOOK_STEP;
        float soon = 1.0f - 0.5f * used / LOOK_F;   // sooner is better
        if (o.ev == EV_CAUGHT) { result = -100000.0f + used * 100; break; }
        if (o.ev == EV_CLEARED) { result = gain + 10000; break; }
        gain += o.dots * 10 * soon;
        if (o.ghost >= 0) gain += o.pts * soon;
        if (o.fruit) gain += FRUIT_PTS[spec().fruit] * soon;
        if (o.energizers) {
            // Worth what the ghosts it turns blue can be caught for; wasted
            // with none in reach, or while they're still blue from the last
            // (it starts the 200 / 400 / 800 / 1600 over)
            float blue = was_blue ? 0 : blue_value(w, pc, pr, ENERGIZE_REACH);
            gain += (blue >= ENERGIZE_MIN ? blue : w.dots_left <= 4 ? 50 : -400) * soon;
        }
        if (used >= LOOK_F || --budget <= 0) { result = gain + leaf(w); break; }
        int c = wrap_c(tile(w.pac.x)), r = tile(w.pac.y);
        if (c == pc && r == pr) continue;
        pc = c; pr = r;
        if (exits(c, r) >= 3) {   // a junction: try every way on
            if (depth >= MAX_DEPTH) { result = gain + leaf(w); break; }
            Dir ways[4];
            int n = 0;
            for (int nd = 0; nd < 4; nd++)
                if (open_tile(c + DX[nd], r + DY[nd]) && !(depth >= 1 && (Dir)nd == reverse(w.pac.d))) ways[n++] = (Dir)nd;
            result = -1e9f;
            for (int i = 0; i < n; i++) {
                World w2 = w;
                float v = walk(w2, ways[i], used, depth + 1, gain, budget / n);
                if (v > result) result = v;
            }
            break;
        }
        if (!open_tile(c + DX[way], r + DY[way]))   // a corner: round it
            for (int nd = 0; nd < 4; nd++)
                if ((Dir)nd != reverse(way) && open_tile(c + DX[nd], r + DY[nd])) { way = (Dir)nd; break; }
    }
    while (undo_n > mark) { int u = wk->undo[--undo_n]; wk->dots[u / COLS][u % COLS] = true; }
    return result;
}

// On the hunt: the blue ghost to go for next, or -1. Tries every order of
// the blue ghosts (four at most: 24 orders) by how far apart they are, and
// takes the one that catches the most before the fright runs out, the
// shortest way. Blue ghosts only wander, at about half his speed, so going
// straight for where one is now catches it. It sticks with the ghost it's
// after unless another order catches more, so it doesn't swap back and forth.
static int hunt_target = -1;
static int hunt(int c, int r) {
    if (live.fright_f <= 0) return hunt_target = -1;
    float tiles_left = live.fright_f * pac_pct(live) / 100 * FULL_SPEED * HUNT_MARGIN;
    int   ids[N_GHOSTS], gc[N_GHOSTS], gr[N_GHOSTS], n = 0;
    for (int i = 0; i < N_GHOSTS; i++)
        if (wk->blue_ok[i]) { ids[n] = i; gc[n] = wrap_c(tile(live.ghosts[i].x)); gr[n] = tile(live.ghosts[i].y); n++; }
    int perm[N_GHOSTS] = { 0, 1, 2, 3 }, best_n = 0, best_first = -1, keep_n = -1;
    float best_d = 1e9f;
    auto try_order = [&]() {
        int caught = 0, pc = c, pr = r;
        float d = 0;
        for (int k = 0; k < n; k++) {
            int i = perm[k];
            d += wk->blue_dist[ids[i]][pr][pc];
            if (d > tiles_left) break;
            caught++;
            pc = gc[i]; pr = gr[i];
        }
        if (caught && ids[perm[0]] == hunt_target && caught > keep_n) keep_n = caught;
        if (caught > best_n || (caught == best_n && caught && d < best_d)) { best_n = caught; best_d = d; best_first = ids[perm[0]]; }
    };
    // every order of n things, by Heap's algorithm
    int cnt[N_GHOSTS] = { 0 };
    try_order();
    for (int i = 1; i < n;) {
        if (cnt[i] < i) {
            int j = i % 2 ? cnt[i] : 0, t = perm[j];
            perm[j] = perm[i]; perm[i] = t;
            try_order();
            cnt[i]++;
            i = 1;
        } else cnt[i++] = 0;
    }
    if (keep_n >= best_n && keep_n > 0) return hunt_target;
    return hunt_target = best_first;
}

// Tiles gone since he last turned back (see plan())
static int since_turn, turn_c, turn_r;

static void plan() {
    memcpy(wk->dots, dot, sizeof(dot));
    undo_n = 0;
    plan_dot_dist();
    plan_blue_dist();
    int c = wrap_c(tile(live.pac.x)), r = tile(live.pac.y), n = exits(c, r);
    int prey = hunt(c, r);
    if (c != turn_c || r != turn_r) { since_turn++; turn_c = c; turn_r = r; }
    int near = 99;   // tiles to the nearest ghost that can catch him
    for (const Ghost &g : live.ghosts) {
        if (g.st != G_ACTIVE || g.fright) continue;
        int dx = abs(wrap_c(tile(g.x)) - c), m = (dx < COLS - dx ? dx : COLS - dx) + abs(tile(g.y) - r);
        if (m < near) near = m;
    }
    float ahead = 0;   // what going on is worth
    if (live.pac.d != NONE && open_tile(c + DX[live.pac.d], r + DY[live.pac.d])) {
        World w = live;
        ahead = walk(w, live.pac.d, 0, 0, 0, BUDGET / n);
    }
    Dir best = NONE;
    float best_v = 0;
    for (int d = 0; d < 4; d++) {
        if (!open_tile(c + DX[d], r + DY[d])) continue;
        float v = ahead;
        if ((Dir)d != live.pac.d) { World w = live; v = walk(w, (Dir)d, 0, 0, 0, BUDGET / n); }
        if (prey >= 0 && v > -50000) {   // hunting: the way that closes in on it, if it doesn't get him caught
            v = 100000.0f - wk->blue_dist[prey][r + DY[d]][wrap_c(c + DX[d])] * 1000.0f + v * 0.01f;
            if (best == NONE || v > best_v) { best = (Dir)d; best_v = v; }
            continue;
        }
        // Loyalty, so he doesn't dither: going on wins ties, and turning back
        // has to be clearly better, and not again within a few tiles of the
        // last time, unless going on gets him caught. Otherwise he'd rock
        // back and forth across a tile edge, between two close calls that
        // each look better from the other side.
        if ((Dir)d == live.pac.d) v += 5;
        else if ((Dir)d == reverse(live.pac.d)) {
            if (since_turn < TURN_LOCK && near > LOCK_SAFE) continue;
            v -= 150;
        }
        if (best == NONE || v > best_v) { best = (Dir)d; best_v = v; }
    }
    if (best != NONE && best == reverse(live.pac.d)) since_turn = 0;
    if (best != NONE) want = best;
}

// ---------------------------------------------------------------------------
// The game

static void set_phase(Phase p) { phase = p; phase_f = 0; }

static void add_score(int pts) {
    score += pts;
    if (!extra_given && score >= EXTRA_LIFE) { extra_given = true; lives++; }
    if (score > top_score) top_score = score;
}

static void reset_actors() {
    World &w = live;
    w.pac = { PAC_X, PAC_Y, LEFT };
    want = LEFT;
    w.pac_pause = 0;
    pac_anim = 0;
    for (int i = 0; i < N_GHOSTS; i++) {
        Ghost &g = w.ghosts[i];
        g.x = HOME_X[i];
        g.y = i ? HOUSE_Y : EXIT_Y;
        g.st = i ? G_HOUSE : G_ACTIVE;
        g.d = i == 0 ? LEFT : i == 1 ? DOWN : UP;
        g.fright = false;
    }
    w.fright_f = w.mode_idx = w.mode_f = w.since_dot = w.combo = w.fruit_f = 0;
    fruit_pts_f = hungry_f = 0;
    plan_c = plan_r = -99;
}

static void start_level() {
    World &w = live;
    w.dots_left = 0;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            dot[r][c] = MAZE[r][c] == '.' || MAZE[r][c] == 'o';
            w.dots_left += dot[r][c];
        }
    w.dots_eaten = 0;
    for (Ghost &g : w.ghosts) g.dots = 0;
    w.global_on = w.elroy_wait = false;
    reset_actors();
    set_phase(PH_READY);
}

static void new_game() {
    score = 0;
    lives = LIVES;
    level = 1;
    extra_given = false;
    start_level();
}

static void lost_life() {
    reset_actors();
    live.global_on = true;    // the shared counter lets the ghosts out from here on
    live.global_dots = 0;
    live.elroy_wait = true;   // and Blinky isn't Elroy again until Clyde's out
}

// Something the last look ahead couldn't have seen coming: a mode change,
// fright starting or ending, a ghost coming out or being eaten
static long situation() {
    long s = live.mode_idx * 2 + (live.fright_f > 0);
    for (const Ghost &g : live.ghosts) s = s * 10 + g.st * 2 + g.fright;
    return s;
}

static void play_frame() {
    if (fruit_pts_f > 0) fruit_pts_f--;
    // Think again in every new tile, and whenever things change
    int c = wrap_c(tile(live.pac.x)), r = tile(live.pac.y);
    long now = situation();
    if (since_touch >= AUTO_RESUME_MS && (c != plan_c || r != plan_r || now != plan_sit)) {
        plan();
        plan_c = c; plan_r = r; plan_sit = now;
    }
    float x0 = live.pac.x, y0 = live.pac.y;
    Outcome o;
    step(live, dot, want, 1, o, false);
    pac_anim += fabsf(live.pac.x - x0) < 1 ? fabsf(live.pac.x - x0) + fabsf(live.pac.y - y0) : 0;
    add_score(o.dots * 10 + o.energizers * 50);
    hungry_f = o.dots || o.energizers ? 0 : hungry_f + 1;
    if (o.fruit) { add_score(FRUIT_PTS[spec().fruit]); fruit_pts_f = 120; }
    if (o.ev == EV_GHOST) {
        add_score(o.pts);
        pts_val = o.pts;
        pts_x = live.ghosts[o.ghost].x; pts_y = live.ghosts[o.ghost].y;
        eaten = o.ghost;
        set_phase(PH_EAT);
    }
    if (o.ev == EV_CAUGHT) set_phase(PH_CAUGHT);
    if (o.ev == EV_CLEARED) set_phase(PH_CLEARED);
}

static void game_frame() {
    frames++;
    phase_f++;
    switch (phase) {
    case PH_READY:   if (phase_f >= READY_F) set_phase(PH_PLAY); break;
    case PH_PLAY:    play_frame(); break;
    case PH_EAT:     if (phase_f >= 60) set_phase(PH_PLAY); break;
    case PH_CAUGHT:  if (phase_f >= 60) set_phase(PH_DYING); break;
    case PH_DYING:
        if (phase_f >= 90) {
            if (--lives > 0) { lost_life(); set_phase(PH_READY); }
            else set_phase(PH_OVER);
        }
        break;
    case PH_OVER:    if (phase_f >= 180) new_game(); break;
    case PH_CLEARED: if (phase_f >= 60) set_phase(PH_FLASH); break;
    case PH_FLASH:   if (phase_f >= 96) { level++; start_level(); } break;
    case PH_STOP:    if (phase_f >= 96) done = true; break;
    }
}

// ---------------------------------------------------------------------------
// Drawing

// Ghosts, 9x9, with two skirts to waddle on; fruit as text art (see pixfb_art)
static const uint16_t GHOST[2][9] = {
    { 0x07C, 0x0FE, 0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x1BB },
    { 0x07C, 0x0FE, 0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x1FF, 0x16D },
};
// '1' red, '2' orange, '3' green, '4' yellow, '5' blue, '6' stem, '7' white
static const char *const CHERRY[]     = { "......66.", ".....6.6.", "....6..6.", ".116..6..", "1171.111.", "1111.1711", ".11..1111", "......11." };
static const char *const STRAWBERRY[] = { "...333...", "..33333..", ".1171711.", ".1711171.", ".1171711.", "..17171..", "..11711..", "...111...", "....1...." };
static const char *const ORANGE[]     = { "....3....", "...333...", "..22222..", ".2222222.", ".2222222.", ".2222222.", ".2222222.", "..22222..", "...222..." };
static const char *const APPLE[]      = { "....6....", "..11611..", ".1111111.", ".1711111.", ".7111111.", ".1111111.", ".1111111.", "..11111..", "...1.1..." };
static const char *const MELON[]      = { "....6....", "...333...", "..37373..", ".3733373.", ".3373733.", ".3733373.", ".3373733.", "..37373..", "...333..." };
static const char *const GALAXIAN[]   = { "....4....", "...444...", "..14441..", ".1144411.", "55.444.55", "5..444..5", "5...4...5", "....4...." };
static const char *const BELL[]       = { "....4....", "...444...", "..44474..", "..44474..", ".4444474.", ".4444444.", "444444444", "..55555..", "....7...." };
static const char *const KEY[]        = { "...555...", "..5...5..", "..55555..", "....7....", "....77...", "....7....", "....77...", "....7....", "....7...." };
struct Art { const char *const *rows; int h; };
static const Art FRUIT[8] = { { CHERRY, 8 }, { STRAWBERRY, 9 }, { ORANGE, 9 }, { APPLE, 9 },
                              { MELON, 9 }, { GALAXIAN, 8 }, { BELL, 9 }, { KEY, 9 } };
static int fruit_of(int lv) { return LEVELS[lv < 21 ? lv - 1 : 20].fruit; }

// A pixel of the maze, in maze coordinates, if it's in view
static inline void mpx(int x, int y, lv_color_t c) {
    int vy = y - view_y;
    if ((unsigned)x >= (unsigned)MAZE_W || (unsigned)vy >= (unsigned)view_h) return;
    fb.buf[(top_y + vy) * fb.w + left_x + x] = c;
}

static void maze_art(int x, int y, const Art &a) {
    for (int r = 0; r < a.h; r++)
        for (int c = 0; a.rows[r][c]; c++) {
            char ch = a.rows[r][c];
            if (ch >= '1' && ch <= '7') mpx(x + c, y + r, fruit_pal[ch - '1']);
        }
}

// Pac-Man: a 9 px disc with the mouth `half_gap` radians either side of facing d
static void draw_pac(int cx, int cy, Dir d, float half_gap, bool in_maze) {
    if (half_gap >= PI_F) return;
    float fx = DX[d], fy = DY[d], cg = cosf(half_gap);
    lv_color_t col = COL_SCENE_PAC_PACMAN;
    for (int dy = -4; dy <= 4; dy++)
        for (int dx = -4; dx <= 4; dx++) {
            if (dx * dx + dy * dy > 18) continue;
            if (half_gap > 0 && (dx || dy) && (dx * fx + dy * fy) / sqrtf(dx * dx + dy * dy) > cg) continue;
            if (in_maze) mpx(cx + dx, cy + dy, col); else pixfb_px(fb, cx + dx, cy + dy, col);
        }
}

static void draw_eyes(int x, int y, Dir d) {
    int ex = DX[d == NONE ? LEFT : d], ey = DY[d == NONE ? LEFT : d];
    lv_color_t white = COL_SCENE_PAC_EATEN, pupil = COL_SCENE_PAC_EYES;
    for (int e = 0; e < 2; e++) {
        int wx = x + (e ? 6 : 1) + ex, wy = y + 2 + ey;
        for (int j = 0; j < 3; j++) for (int i = 0; i < 2; i++) mpx(wx + i, wy + j, white);
        if (ex) { mpx(wx + (ex > 0), wy + 1, pupil); mpx(wx + (ex > 0), wy + 2, pupil); }
        else    { int py = wy + (ey > 0 ? 2 : 0); mpx(wx, py, pupil); mpx(wx + 1, py, pupil); }
    }
}

static void draw_ghost(int i) {
    const Ghost &g = live.ghosts[i];
    int x = (int)floorf(g.x * T) - 4, y = (int)floorf(g.y * T) - 4;
    if (g.st == G_EYES || g.st == G_ENTERING) { draw_eyes(x, y, g.d); return; }
    // Blue, and near the end flashing white
    bool flash = g.fright && live.fright_f <= spec().flashes * FLASH_F && (live.fright_f / (FLASH_F / 2)) % 2 == 0;
    lv_color_t body = !g.fright ? ghost_col[i] : flash ? COL_SCENE_PAC_EATEN : COL_SCENE_PAC_FRIGHT;
    const uint16_t *rows = GHOST[(frames / 8) & 1];
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            if (rows[r] & (1u << (8 - c))) mpx(x + c, y + r, body);
    if (!g.fright) { draw_eyes(x, y, g.d); return; }
    lv_color_t face = flash ? COL_SCENE_PAC_FLASH_FACE : COL_SCENE_PAC_FRIGHT_FACE;
    for (int j = 3; j <= 4; j++) { mpx(x + 2, y + j, face); mpx(x + 6, y + j, face); }
    for (int c = 1; c <= 7; c++) mpx(x + c, y + (c & 1 ? 6 : 5), face);   // the wavy mouth
}

static void maze_text(const char *s, int row, lv_color_t c) {   // centred on the maze, on a row
    int w = pixfb_text_width(s), x = left_x + MAZE_W / 2 - w / 2, y = top_y + row * T + T / 2 - 3 - view_y;
    pixfb_fill(fb, x - 1, y - 1, w + 2, PIXFB_DIGIT_H + 2, COL_SCENE_ARCADE_BG);
    pixfb_text(fb, x, y, s, c);
}

static void maze_number(long v, float tx, float ty, lv_color_t c) {   // centred on a point in the maze
    int w = pixfb_number_width(v);
    pixfb_number(fb, left_x + (int)(tx * T) - w / 2, top_y + (int)(ty * T) - 3 - view_y, v, c);
}

static void draw_hud() {
    int up = phase != PH_PLAY || (frames / 16) & 1;   // 1UP blinks while you play
    if (score == hud_score && top_score == hud_top && lives == hud_lives && level == hud_level && up == hud_up) return;
    hud_score = score; hud_top = top_score; hud_lives = lives; hud_level = level; hud_up = up;
    lv_color_t text = COL_SCENE_PAC_TEXT, bg = COL_SCENE_ARCADE_BG;
    int lw = left_x, rx = left_x + MAZE_W, rw = W - rx;
    pixfb_fill(fb, 0, 0, lw, H, bg);
    pixfb_fill(fb, rx, 0, rw, H, bg);
    if (up) pixfb_text(fb, (lw - pixfb_text_width("1UP")) / 2, 3, "1UP", text);
    pixfb_number(fb, lw - 5 - pixfb_number_width(score, 2), 13, score, text, 2);
    for (int k = 0; k < lives - 1 && k < 4; k++) draw_pac(7 + k * 11, H - 8, LEFT, 0.5f, false);   // the ones in reserve
    pixfb_text(fb, rx + (rw - pixfb_text_width("HIGH")) / 2, 3, "HIGH", text);
    if (top_score) pixfb_number(fb, W - 5 - pixfb_number_width(top_score, 2), 13, top_score, text, 2);
    for (int k = 0; k < 4 && level - k >= 1; k++) {   // this level's fruit and the last few, newest on the right
        const Art &a = FRUIT[fruit_of(level - k)];
        pixfb_art(fb, W - 13 - k * 11, H - 12 - (a.h < 9), a.rows, a.h, fruit_pal);
    }
}

static void draw() {
    // The walls (flashing white at the end of a level) and the door
    bool white = (phase == PH_FLASH || phase == PH_STOP) && (phase_f / 12) & 1;
    lv_color_t wall = white ? COL_SCENE_PAC_FLASH : COL_SCENE_PAC_WALL, bg = COL_SCENE_ARCADE_BG;
    for (int y = 0; y < view_h; y++) {
        lv_color_t *row = fb.buf + (top_y + y) * fb.w + left_x;
        const uint32_t *bits = wk->walls[view_y + y];
        for (int x = 0; x < MAZE_W; x++) row[x] = (bits[x >> 5] >> (x & 31)) & 1 ? wall : bg;
    }
    bool flashing = phase == PH_FLASH || phase == PH_STOP;
    if (!flashing) for (int x = 13 * T; x < 15 * T; x++) mpx(x, (DOOR_R + 1) * T + 2, COL_SCENE_PAC_DOOR);

    // Dots, and the energizers blinking
    bool blink = phase == PH_PLAY && (frames / 10) & 1;
    for (int r = view_y / T; r <= (view_y + view_h - 1) / T && r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            if (!dot[r][c]) continue;
            int x = c * T + 2, y = r * T + 2;
            if (!energizer(c, r)) { mpx(x, y, COL_SCENE_PAC_DOT); continue; }
            if (blink) continue;
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2; dx <= 2; dx++) if (dx * dx + dy * dy <= 5) mpx(x + dx, y + dy, COL_SCENE_PAC_DOT);
        }

    bool play = phase == PH_PLAY || phase == PH_EAT || phase == PH_CAUGHT;
    if (live.fruit_f > 0 && play) maze_art((int)(DOOR_X * T) - 4, FRUIT_R * T + 2 - 4, FRUIT[spec().fruit]);

    if (play || phase == PH_READY || phase == PH_CLEARED)
        for (int i = N_GHOSTS - 1; i >= 0; i--)
            if (!(phase == PH_EAT && i == eaten)) draw_ghost(i);

    int px = (int)floorf(live.pac.x * T), py = (int)floorf(live.pac.y * T);
    if (phase == PH_DYING) {
        // He folds up from the top into nothing, and pops
        float t = (phase_f - 15) / 60.0f;
        if (t < 1) draw_pac(px, py, UP, (t < 0 ? 0.3f : 0.3f + t * (PI_F - 0.3f)), true);
        else if (phase_f < 88)
            for (int d = 0; d < 8; d++) {
                float a = d * PI_F / 4;
                for (int k = 2; k <= 3; k++) mpx(px + (int)roundf(cosf(a) * k), py + (int)roundf(sinf(a) * k), COL_SCENE_PAC_PACMAN);
            }
    } else if (phase != PH_EAT && phase != PH_OVER) {
        static const float GAPS[4] = { 0.95f, 0.5f, 0, 0.5f };   // open, half, shut, half
        float gap = phase == PH_READY ? 0 : GAPS[(int)(pac_anim * 6) & 3];
        draw_pac(px, py, live.pac.d, gap, true);
    }
    pixfb_touch(fb, left_x, top_y, MAZE_W, view_h);

    if (phase == PH_EAT) maze_number(pts_val, pts_x, pts_y, COL_SCENE_PAC_GHOST_POINTS);
    if (fruit_pts_f > 0) maze_number(FRUIT_PTS[spec().fruit], DOOR_X, FRUIT_R + 0.5f, COL_SCENE_PAC_FRUIT_POINTS);
    if (phase == PH_READY) maze_text("READY!", FRUIT_R, COL_SCENE_PAC_READY);
    if (phase == PH_OVER) maze_text("GAME OVER", FRUIT_R, COL_SCENE_PAC_GAME_OVER);
    draw_hud();
}

static void follow() {   // the view keeps Pac-Man in its middle third
    if (view_h >= MAZE_H) { view_y = 0; return; }
    int py = (int)(live.pac.y * T), lo = view_h / 3, hi = view_h - view_h / 3;
    if (py - view_y < lo) view_y = py - lo;
    if (py - view_y > hi) view_y = py - hi;
    if (view_y < 0) view_y = 0;
    if (view_y > MAZE_H - view_h) view_y = MAZE_H - view_h;
}

// ---------------------------------------------------------------------------

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_ARCADE_BG);
    if (!oom && !(wk = (Work *)malloc(sizeof(Work)))) oom = true;
    done = oom || w < MAZE_W;
    if (done) return;
    left_x = (w - MAZE_W) / 2;
    view_h = h < MAZE_H ? h : MAZE_H;
    top_y = (h - view_h) / 2;
    build_walls();
    ghost_col[0] = COL_SCENE_PAC_GHOST_1; ghost_col[1] = COL_SCENE_PAC_GHOST_2;
    ghost_col[2] = COL_SCENE_PAC_GHOST_3; ghost_col[3] = COL_SCENE_PAC_GHOST_4;
    lv_color_t pal[7] = { COL_SCENE_PAC_FRUIT_RED, COL_SCENE_PAC_FRUIT_ORANGE, COL_SCENE_PAC_FRUIT_GREEN, COL_SCENE_PAC_FRUIT_YELLOW,
                          COL_SCENE_PAC_FRUIT_BLUE, COL_SCENE_PAC_FRUIT_STEM, COL_SCENE_PAC_EATEN };
    memcpy(fruit_pal, pal, sizeof(pal));
    new_game();
    frames = 0;
    frame_acc = 0;
    stopping = false;
    since_touch = AUTO_RESUME_MS;
    hud_score = hud_top = -1;
    view_y = MAZE_H;   // follow() snaps it onto Pac-Man
    follow();
}

static void tick(uint32_t dt_ms) {
    if (oom || done) return;
    since_touch += dt_ms;
    frame_acc += dt_ms * 0.06f;
    while (frame_acc >= 1 && !done) { frame_acc -= 1; game_frame(); }
    if (done) return;
    follow();
    draw();
    pixfb_flush(fb);
}

static void request_stop() {
    if (stopping) return;
    stopping = true;
    set_phase(PH_STOP);
}

static bool is_done() { return done; }

static void touch(SceneTouch type, int x, int y) {
    if (type == SCENE_TOUCH_RELEASE || stopping || oom) return;
    since_touch = 0;
    float dx = x - (left_x + live.pac.x * T), dy = y - (top_y + live.pac.y * T - view_y);
    if (fabsf(dx) < 4 && fabsf(dy) < 4) return;
    want = fabsf(dx) > fabsf(dy) ? (dx > 0 ? RIGHT : LEFT) : (dy > 0 ? DOWN : UP);
}

static void finish() {
    pixfb_free(fb);
    free(wk);
    wk = nullptr;
}

const Scene scene_pacman = { "pacman", start, tick, request_stop, is_done, touch, finish, nullptr, true };
SCENE_REGISTER(scene_pacman, "Pac-Man");
