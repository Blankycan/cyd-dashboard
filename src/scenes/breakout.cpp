#include "registry.h"
#include "pixfb.h"
#include "../theme.h"
#include <math.h>

// Breakout — a wall of coloured bricks, a paddle, and a ball that picks up
// speed as the wall comes down. Broken bricks shatter into falling chips.
// Cleared walls are replaced by a new pattern. It plays itself, and well:
// it works out where each ball will come down, takes it on the part of the
// paddle that sends it at the best brick (through a gap to get above the
// wall, if there's one), and with several balls in play saves the one that
// will land first. Scored as in the original: 1, 4 or 7 points a brick, higher
// rows worth more; three balls a game, then a fresh wall and a new game.
// When asked to stop, the ball vanishes and the remaining bricks crumble away.
// Touch: drag to move the paddle; tap to launch a ball waiting on it. The
// computer takes back over AUTO_RESUME_MS after your last touch.
// Events: a Claude session finishing splits the ball into three.

static const uint32_t AUTO_RESUME_MS = 5000;
static const int      ROWS = 6, MAX_COLS = 14, BW = 16, BH = 5, GAP = 2;
static const int      LIVES = 3;
static const int      PW = 28, PH = 4, BALL = 4, MAX_BALLS = 3, MAX_CHIPS = 90;
static const float    BASE_SPEED = 105, TOP_SPEED = 165, PADDLE_V = 170;

struct Ball { bool alive; float x, y, vx, vy; };    // x, y: top-left
struct Chip { bool alive; float x, y, vx, vy; uint8_t row; };

static PixFb    fb;
static int      W, H, cols, wall_x, wall_y, paddle_y, level, broken, score, lives;
static float    over_t;            // counting after the last ball is lost; < 0 while playing
static uint8_t  bricks[ROWS][MAX_COLS];            // 0 gone, 1 standing
static Ball     balls[MAX_BALLS];
static Chip     chips[MAX_CHIPS];
static float    paddle_x, paddle_target, speed, wait_t, crumble_acc;
static float    plan_x = -1, plan_t;   // where the ball it's saving comes down, and where on the paddle to take it
static int      plan_ball = -1;
static bool     waiting, stopping, done, oom;
static int      claude_was;   // working Claude sessions before the latest change
static uint32_t since_touch;

static lv_color_t row_color(int r) {
    switch (r) {
        case 0:  return COL_SCENE_BRK_ROW_1;
        case 1:  return COL_SCENE_BRK_ROW_2;
        case 2:  return COL_SCENE_BRK_ROW_3;
        case 3:  return COL_SCENE_BRK_ROW_4;
        case 4:  return COL_SCENE_BRK_ROW_5;
        default: return COL_SCENE_BRK_ROW_6;
    }
}

static int brick_x(int c) { return wall_x + c * (BW + GAP); }
static int brick_y(int r) { return wall_y + r * (BH + GAP); }

static int bricks_left() {
    int n = 0;
    for (int r = 0; r < ROWS; r++) for (int c = 0; c < cols; c++) n += bricks[r][c];
    return n;
}

static void new_wall() {
    // A few patterns in turn: the full wall, a pyramid, a checkerboard, gaps
    int pattern = level % 4;
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < cols; c++) {
            bool on = true;
            int mid = cols / 2, dc = c < mid ? mid - 1 - c : c - mid;
            if (pattern == 1) on = dc <= r + 1;
            if (pattern == 2) on = (r + c) % 2 == 0;
            if (pattern == 3) on = r % 2 == 0 || c % 3 != 1;
            bricks[r][c] = on;
        }
    level++;
    broken = 0;
}

static void serve() {   // the ball sits on the paddle for a moment, then goes
    for (int i = 0; i < MAX_BALLS; i++) balls[i].alive = false;
    balls[0] = { true, paddle_x - BALL / 2.0f, (float)paddle_y - BALL, 0, 0 };
    waiting = true;
    wait_t = 0;
    speed = BASE_SPEED;
    plan_ball = -1;
}

static float shot_value(float x, float a);

static void launch() {
    if (!waiting) return;
    waiting = false;
    float a = scene_randf(-0.5f, 0.5f);
    if (since_touch >= AUTO_RESUME_MS) {   // the computer serves at the best brick it can
        float best = -1;
        for (float t = -0.5f; t <= 0.5f; t += 0.05f) {
            float v = shot_value(paddle_x, t / 1.05f) - fabsf(t) * 0.5f;
            if (v > best) { best = v; a = t; }
        }
    }
    balls[0].vx = speed * sinf(a);
    balls[0].vy = -speed * cosf(a);
}

static void start(lv_obj_t *area, int w, int h) {
    W = w; H = h;
    oom = !pixfb_create(fb, area, w, h, COL_SCENE_ARCADE_BG);
    done = oom;
    if (oom) return;
    if (!pixfb_bg_save(fb)) { done = oom = true; return; }   // the plain background, wiped back each frame
    cols = (w - 8 + GAP) / (BW + GAP);
    if (cols > MAX_COLS) cols = MAX_COLS;
    wall_x = (w - (cols * (BW + GAP) - GAP)) / 2;
    wall_y = 12;   // room above for the score and balls
    paddle_y = h - PH - 2;
    paddle_x = paddle_target = w / 2.0f;
    level = score = 0;
    lives = LIVES;
    over_t = -1;
    new_wall();
    for (int i = 0; i < MAX_CHIPS; i++) chips[i].alive = false;
    serve();
    crumble_acc = 0;
    claude_was = scene_ctx().claude_working;
    stopping = false;
    since_touch = AUTO_RESUME_MS;
}

static void shatter(int r, int c) {
    bricks[r][c] = 0;
    broken++;
    int made = 0;
    for (int i = 0; i < MAX_CHIPS && made < 4; i++) {
        if (chips[i].alive) continue;
        chips[i] = { true, brick_x(c) + (made % 2) * 8.0f + 3, brick_y(r) + (made / 2) * 2.0f,
                     scene_randf(-30, 30), scene_randf(-40, 0), (uint8_t)r };
        made++;
    }
}

// Where and when will a ball on its way down reach the paddle's height?
// Walls fold the path back.
static float land_x(const Ball &b, float &t) {
    t = (paddle_y - BALL - b.y) / b.vy;
    float x = b.x + b.vx * t, span = W - BALL;
    x = fmodf(fabsf(x), 2 * span);
    if (x > span) x = 2 * span - x;
    return x + BALL / 2.0f;
}

static int brick_at(float cx, float cy, int &r, int &c) {   // as move_ball sees it
    c = (int)floorf((cx - wall_x) / (BW + GAP)); r = (int)floorf((cy - wall_y) / (BH + GAP));
    if (c < 0 || c >= cols || r < 0 || r >= ROWS || !bricks[r][c]) return 0;
    return cx <= brick_x(c) + BW && cy <= brick_y(r) + BH;
}

// What a shot off the paddle at x, taken at t (-1 left end .. 1 right end),
// is worth: follow it up through the walls to the first brick it breaks. A
// brick reached from above the wall (through a gap) is worth the most: the
// ball rattles about up there. A shot that comes back down untouched, nothing.
static float shot_value(float x, float t) {
    float a = t * 1.05f, dx = sinf(a) * 2, dy = -cosf(a) * 2;
    float cx = x, cy = paddle_y - BALL / 2.0f, lo = BALL / 2.0f, hi = W - BALL / 2.0f;
    bool above = false;
    for (int step = 0; step < 220; step++) {
        cx += dx; cy += dy;
        if (cx < lo) { cx = 2 * lo - cx; dx = -dx; }
        if (cx > hi) { cx = 2 * hi - cx; dx = -dx; }
        if (cy < lo) { cy = 2 * lo - cy; dy = -dy; }
        if (dy > 0 && cy > paddle_y) return 0;
        above |= cy < wall_y;
        int r, c;
        if (brick_at(cx, cy, r, c)) return 10 + (r < 2 ? 7 : r < 4 ? 4 : 1) + (above && dy > 0 ? 25 : 0);
    }
    return 0;
}

static void autopilot() {
    // Save the ball that lands first, unless the paddle can't get there in
    // time: then the next one that it can
    int pick = -1;
    float pick_t = 1e9f, pick_x = 0;
    for (int pass = 0; pass < 2 && pick < 0; pass++)
        for (int i = 0; i < MAX_BALLS; i++) {
            if (!balls[i].alive || balls[i].vy <= 0 || waiting) continue;
            float t, x = land_x(balls[i], t);
            bool reach = fabsf(x - paddle_x) <= PADDLE_V * t + PW / 2.0f;
            if ((reach || pass) && t < pick_t) { pick = i; pick_t = t; pick_x = x; }
        }
    if (pick >= 0) {
        // Plan the shot when a ball starts down, and again only if its
        // landing spot moves (a brick turned it)
        if (pick != plan_ball || fabsf(pick_x - plan_x) > 1.5f) {
            plan_ball = pick; plan_x = pick_x; plan_t = 0;
            float best = -1;
            for (float t = -0.85f; t <= 0.851f; t += 0.085f) {
                float px = fmaxf(PW / 2.0f, fminf(W - PW / 2.0f, pick_x - t * PW / 2.0f));
                float tt = (pick_x - px) / (PW / 2.0f);   // against a side wall the paddle can't centre on it
                if (fabsf(px - paddle_x) > PADDLE_V * pick_t - 2) continue;   // can't get there
                float v = shot_value(pick_x, tt) - fabsf(tt) * 2;              // all else equal, the safer middle
                if (v > best) { best = v; plan_t = tt; }
            }
        }
        paddle_target = plan_x - plan_t * PW / 2.0f;
    } else if (!waiting) {
        plan_ball = -1;
        // All on their way up: wait under the one that'll come down first
        float soon = 1e9f;
        for (int i = 0; i < MAX_BALLS; i++) {
            if (!balls[i].alive) continue;
            Ball b = balls[i];
            float up = b.vy < 0 ? -b.y / b.vy : 0;   // to the ceiling (roughly: bricks may turn it sooner)
            if (b.vy < 0) { b.x += b.vx * up; b.y = 0; b.vy = -b.vy; }
            float t, x = land_x(b, t);
            if (up + t < soon) { soon = up + t; paddle_target = x; }
        }
    }
    if (waiting && wait_t > 0.8f) launch();
}

static void bounce_off_paddle(Ball &b) {
    float t = (b.x + BALL / 2.0f - paddle_x) / (PW / 2.0f);   // -1 at the left end, 1 at the right
    t = fmaxf(-1, fminf(1, t));
    float a = t * 1.05f;                                      // up to 60 degrees off vertical
    b.vx = speed * sinf(a);
    b.vy = -speed * cosf(a);
    b.y = paddle_y - BALL;
    plan_ball = -1;
}

static void move_ball(Ball &b, float dt) {
    // Small steps so a fast ball can't skip through a brick
    float dist = sqrtf(b.vx * b.vx + b.vy * b.vy) * dt;
    int steps = (int)(dist / 1.5f) + 1;
    float sx = b.vx * dt / steps, sy = b.vy * dt / steps;
    for (int s = 0; s < steps && b.alive; s++) {
        float px = b.x, py = b.y;
        b.x += sx; b.y += sy;
        if (b.x < 0)            { b.x = 0; b.vx = fabsf(b.vx); sx = fabsf(sx); }
        if (b.x > W - BALL)     { b.x = W - BALL; b.vx = -fabsf(b.vx); sx = -fabsf(sx); }
        if (b.y < 0)            { b.y = 0; b.vy = fabsf(b.vy); sy = fabsf(sy); }
        if (b.y > H)            { b.alive = false; break; }
        if (b.vy > 0 && b.y + BALL >= paddle_y && b.y + BALL <= paddle_y + PH &&
            b.x + BALL > paddle_x - PW / 2.0f && b.x < paddle_x + PW / 2.0f) {
            bounce_off_paddle(b);
            sx = b.vx * dt / steps; sy = b.vy * dt / steps;
            continue;
        }
        // Brick under the ball's centre?
        float cx = b.x + BALL / 2.0f, cy = b.y + BALL / 2.0f;
        int c = (int)floorf((cx - wall_x) / (BW + GAP)), r = (int)floorf((cy - wall_y) / (BH + GAP));
        if (c < 0 || c >= cols || r < 0 || r >= ROWS || !bricks[r][c]) continue;
        if (cx > brick_x(c) + BW || cy > brick_y(r) + BH) continue;   // in the gap between bricks
        shatter(r, c);
        score += r < 2 ? 7 : r < 4 ? 4 : 1;
        float pcx = px + BALL / 2.0f;
        bool came_from_side = pcx < brick_x(c) || pcx > brick_x(c) + BW;
        if (came_from_side) { b.vx = -b.vx; sx = -sx; } else { b.vy = -b.vy; sy = -sy; }
        b.x = px; b.y = py;
        if (broken % 8 == 0 && speed < TOP_SPEED) {   // the pace picks up as the wall comes down
            speed += 8;
            float k = speed / sqrtf(b.vx * b.vx + b.vy * b.vy);
            b.vx *= k; b.vy *= k; sx *= k; sy *= k;
        }
    }
}

static void update(float dt) {
    since_touch += (uint32_t)(dt * 1000);
    if (waiting) {
        wait_t += dt;
        balls[0].x = paddle_x - BALL / 2.0f;
        balls[0].y = paddle_y - BALL;
    }
    if (!stopping) {
        if (since_touch >= AUTO_RESUME_MS) autopilot();
        else if (waiting && wait_t > 3) launch();
        float d = paddle_target - paddle_x, step = PADDLE_V * dt;
        paddle_x += d > step ? step : d < -step ? -step : d;
        paddle_x = fmaxf(PW / 2.0f, fminf(W - PW / 2.0f, paddle_x));
        bool any = false;
        for (int i = 0; i < MAX_BALLS; i++) if (balls[i].alive) { if (!waiting) move_ball(balls[i], dt); any |= balls[i].alive; }
        if (!any && over_t < 0) {
            if (--lives > 0) serve();
            else over_t = 0;   // game over: a pause on the final score, then a new game
        }
        if (over_t >= 0 && (over_t += dt) > 2.5f) {
            level = score = 0;
            lives = LIVES;
            over_t = -1;
            new_wall();
            serve();
        }
        if (bricks_left() == 0) { new_wall(); serve(); }
    } else {
        // Wrapping up: the bricks crumble, a few at a time
        crumble_acc += dt * 30;
        while (crumble_acc >= 1) {
            crumble_acc -= 1;
            int n = bricks_left();
            if (!n) break;
            int pick = random(0, n);
            for (int r = ROWS - 1; r >= 0 && pick >= 0; r--)
                for (int c = 0; c < cols; c++)
                    if (bricks[r][c] && pick-- == 0) { shatter(r, c); break; }
        }
    }

    bool chips_left = false;
    for (int i = 0; i < MAX_CHIPS; i++) {
        Chip &ch = chips[i];
        if (!ch.alive) continue;
        ch.vy += 220 * dt;
        ch.x += ch.vx * dt; ch.y += ch.vy * dt;
        if (ch.y > H) ch.alive = false;
        chips_left |= ch.alive;
    }
    if (stopping && !chips_left && bricks_left() == 0) done = true;
}

static void draw() {
    pixfb_bg_restore(fb, 0, 0, W, H);
    for (int r = 0; r < ROWS; r++) {
        lv_color_t col = row_color(r), hi = pixfb_mix(col, COL_SCENE_BRK_SHINE, 0.45f);
        for (int c = 0; c < cols; c++) {
            if (!bricks[r][c]) continue;
            int x = brick_x(c), y = brick_y(r);
            pixfb_fill(fb, x, y, BW, BH, col);
            pixfb_fill(fb, x, y, BW, 1, hi);   // a lit top edge
        }
    }
    for (int i = 0; i < MAX_CHIPS; i++)
        if (chips[i].alive) pixfb_fill(fb, (int)chips[i].x, (int)chips[i].y, 2, 2, row_color(chips[i].row));
    // Score top left, balls left top right (the score blinks once the game is over)
    if (over_t < 0 || fmodf(over_t, 0.5f) < 0.3f) pixfb_number(fb, 3, 2, score, COL_SCENE_BRK_PADDLE, 3);
    for (int i = 0; i < lives; i++) {
        int bx = W - 7 - i * 7;
        pixfb_fill(fb, bx, 4, BALL, BALL - 2, COL_SCENE_BRK_BALL);
        pixfb_fill(fb, bx + 1, 3, BALL - 2, BALL, COL_SCENE_BRK_BALL);
    }
    if (!stopping) {
        int px = (int)(paddle_x - PW / 2.0f);
        pixfb_fill(fb, px + 1, paddle_y, PW - 2, PH, COL_SCENE_BRK_PADDLE);
        pixfb_fill(fb, px, paddle_y + 1, PW, PH - 2, COL_SCENE_BRK_PADDLE);
        pixfb_fill(fb, px + 2, paddle_y, PW - 4, 1, COL_SCENE_BRK_SHINE);
        for (int i = 0; i < MAX_BALLS; i++) {
            if (!balls[i].alive) continue;
            int bx = (int)balls[i].x, by = (int)balls[i].y;
            pixfb_fill(fb, bx, by + 1, BALL, BALL - 2, COL_SCENE_BRK_BALL);   // rounded: no corners
            pixfb_fill(fb, bx + 1, by, BALL - 2, BALL, COL_SCENE_BRK_BALL);
        }
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

static void touch(SceneTouch type, int x, int) {
    if (type == SCENE_TOUCH_RELEASE || stopping) return;
    since_touch = 0;
    paddle_target = x;
    if (type == SCENE_TOUCH_PRESS) launch();
}

static void event(const SceneEvent &e) {
    if (e.type != SCENE_EV_CLAUDE) return;
    if (e.value < claude_was && !stopping && !waiting) {   // a session finished: every ball in play splits in three
        const Ball *src = nullptr;
        for (int i = 0; i < MAX_BALLS; i++) if (balls[i].alive) { src = &balls[i]; break; }
        if (src) {
            Ball b = *src;
            for (int i = 0, k = 0; i < MAX_BALLS; i++) {
                if (balls[i].alive && &balls[i] == src) continue;
                float a = (k++ ? 0.5f : -0.5f);
                balls[i] = { true, b.x, b.y, b.vx * cosf(a) - b.vy * sinf(a), b.vx * sinf(a) + b.vy * cosf(a) };
            }
        }
    }
    claude_was = e.value;
}

static void finish() { pixfb_free(fb); }

const Scene scene_breakout = { "breakout", start, tick, request_stop, is_done, touch, finish, event, true };
SCENE_REGISTER(scene_breakout, "Breakout");
