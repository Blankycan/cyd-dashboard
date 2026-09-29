#include "scene_player.h"
#include "registry.h"
#include "../config.h"
#include "../theme.h"
#include "../ui_helpers.h"
#include "../widgets/calendar.h"

// Leading nullptr lets CYD_SCENES be left empty; real entries start at [1].
static const Scene *const playlist[] = { nullptr, CYD_SCENES };
static const int PLAYLIST_N = sizeof(playlist) / sizeof(playlist[0]) - 1;

enum Mode { MODE_CALENDAR, MODE_SCENE };

static Mode         mode        = MODE_CALENDAR;
static lv_obj_t    *cal_panel   = nullptr;
static lv_obj_t    *scene_panel = nullptr;   // background, shown while a scene runs
static lv_obj_t    *area        = nullptr;   // per-scene container, deleted when it ends
static const Scene *cur         = nullptr;
static int          next_idx    = 0;
static bool         stopping    = false;     // request_stop() sent, waiting for is_done()
static bool         manual      = false;     // scene was started by a tap: ignore meeting focus
static bool         paused      = false;
static uint32_t     mode_ms     = 0;         // time in the current mode, excluding pauses
static uint32_t     stop_ms     = 0;         // time since request_stop()
static uint32_t     last_ms     = 0;
static uint32_t     last_flip_ms = 0;
static uint32_t     frames = 0, frame_ms_sum = 0;   // actual frame pacing of the current scene

// Touch queue: filled from the indev read callback, drained in the frame
// timer, so scenes never create/delete objects mid-input-processing.
struct TouchEv { SceneTouch type; int16_t x, y; };
static TouchEv tq[8];
static int     tq_len = 0;
static bool    scene_owns_gesture = false;   // current press started inside the scene

static const uint32_t FLIP_DEBOUNCE_MS = 300;

// One {"log":"..."} line per transition; the companion prints these, which
// makes the rotation checkable from `journalctl` without looking at the board.
static void log_event(const char *what) {
    Serial.printf("{\"log\":\"scene %s %s\"}\n", cur ? cur->name : "-", what);
}

// ---------------------------------------------------------------------------

static void start_scene(bool by_tap) {
    if (PLAYLIST_N == 0) return;
    cur      = playlist[1 + next_idx];
    next_idx = (next_idx + 1) % PLAYLIST_N;

    int w = lv_obj_get_width(scene_panel), h = lv_obj_get_height(scene_panel);
    area = lv_obj_create(scene_panel);
    lv_obj_remove_style_all(area);
    lv_obj_set_size(area, w, h);
    lv_obj_clear_flag(area, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_flag(cal_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(scene_panel, LV_OBJ_FLAG_HIDDEN);

    mode     = MODE_SCENE;
    mode_ms  = 0;
    frames   = frame_ms_sum = 0;
    stopping = false;
    manual   = by_tap;
    cur->start(area, w, h);
    log_event(by_tap ? "start (tap)" : "start");
}

static void end_scene(const char *why) {
    // Report how well the scene kept up: well above SCENE_FRAME_MS means the
    // board is struggling to draw it
    char buf[64];
    snprintf(buf, sizeof(buf), "%s, avg frame %lu ms", why,
             (unsigned long)(frames ? frame_ms_sum / frames : 0));
    log_event(buf);
    lv_obj_del(area);           // takes every object the scene made with it
    area = nullptr;
    if (cur->finish) cur->finish();
    cur = nullptr;
    scene_owns_gesture = false;

    lv_obj_add_flag(scene_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cal_panel, LV_OBJ_FLAG_HIDDEN);
    mode    = MODE_CALENDAR;
    mode_ms = 0;
}

static void handle_touch(const TouchEv &ev) {
    if (ev.type == SCENE_TOUCH_PRESS) {
        if (mode == MODE_SCENE) {
            lv_area_t a;
            lv_obj_get_coords(area, &a);
            if (ev.x >= a.x1 && ev.x <= a.x2 && ev.y >= a.y1 && ev.y <= a.y2) {
                scene_owns_gesture = true;
                if (cur->touch) cur->touch(SCENE_TOUCH_PRESS, ev.x - a.x1, ev.y - a.y1);
                return;
            }
        }
        // Flip. Debounced so a bouncy touch reading can't flip straight back.
        uint32_t now = millis();
        if (now - last_flip_ms < FLIP_DEBOUNCE_MS) return;
        last_flip_ms = now;
        if (mode == MODE_SCENE) end_scene("ended (tap)");
        else                    start_scene(true);
        return;
    }

    if (!scene_owns_gesture || mode != MODE_SCENE) return;
    if (cur->touch) {
        lv_area_t a;
        lv_obj_get_coords(area, &a);
        cur->touch(ev.type, ev.x - a.x1, ev.y - a.y1);
    }
    if (ev.type == SCENE_TOUCH_RELEASE) scene_owns_gesture = false;
}

static void frame_cb(lv_timer_t *) {
    uint32_t now = millis();
    uint32_t dt  = now - last_ms;
    last_ms = now;
    if (mode == MODE_SCENE && !paused) { frames++; frame_ms_sum += dt; }
    if (dt > 200) dt = 200;   // don't jump after a long stall

    for (int i = 0; i < tq_len; i++) handle_touch(tq[i]);
    tq_len = 0;

    if (paused) return;
    mode_ms += dt;

    if (mode == MODE_CALENDAR) {
        if (PLAYLIST_N > 0 && mode_ms >= CAL_PEEK_MS && !calendar_wants_focus())
            start_scene(false);
        return;
    }

    cur->tick(dt);

    if (!stopping) {
        if (cur->is_done()) { end_scene("finished on its own"); return; }
        if (mode_ms >= SCENE_SHOW_MS || (!manual && calendar_wants_focus())) {
            cur->request_stop();
            stopping = true;
            stop_ms  = 0;
            log_event(mode_ms >= SCENE_SHOW_MS ? "stop requested (time)" : "stop requested (meeting soon)");
        }
        return;
    }

    stop_ms += dt;
    if (cur->is_done())                   end_scene("done");
    else if (stop_ms >= SCENE_STOP_GRACE_MS) end_scene("cut off after grace period");
}

// ---------------------------------------------------------------------------

void scene_player_init(lv_obj_t *calendar_panel) {
    cal_panel = calendar_panel;
    lv_obj_update_layout(cal_panel);

    scene_panel = make_panel(lv_obj_get_parent(cal_panel),
                             lv_obj_get_x(cal_panel), lv_obj_get_y(cal_panel),
                             lv_obj_get_width(cal_panel), lv_obj_get_height(cal_panel),
                             COL_SCENE_BG);
    lv_obj_add_flag(scene_panel, LV_OBJ_FLAG_HIDDEN);

    last_ms = millis();
    lv_timer_create(frame_cb, SCENE_FRAME_MS, nullptr);
}

void scene_player_touch(SceneTouch type, int x, int y) {
    // Collapse consecutive drags into the latest position
    if (type == SCENE_TOUCH_DRAG && tq_len > 0 && tq[tq_len - 1].type == SCENE_TOUCH_DRAG) {
        tq[tq_len - 1].x = x;
        tq[tq_len - 1].y = y;
        return;
    }
    if (tq_len >= (int)(sizeof(tq) / sizeof(tq[0]))) return;
    tq[tq_len++] = { type, (int16_t)x, (int16_t)y };
}

void scene_player_set_paused(bool p) {
    paused = p;
}
