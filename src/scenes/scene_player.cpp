#include "scene_player.h"
#include "registry.h"
#include "pixfb.h"
#include "../config.h"
#include "../theme.h"
#include "../ui_helpers.h"
#include "../state.h"
#include "../widgets/calendar.h"

// Leading nullptr lets CYD_SCENES be left empty; real entries start at [1].
static const Scene *const playlist[] = { nullptr, CYD_SCENES };
static const int PLAYLIST_N = sizeof(playlist) / sizeof(playlist[0]) - 1;

// Play order: playlist indices, reshuffled at the start of every cycle when
// SCENE_SHUFFLE is on
static int order[PLAYLIST_N > 0 ? PLAYLIST_N : 1];

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

// Host events for the running scene, queued like touches and delivered at the
// start of the next frame. Audio frames collapse into one entry.
static SceneEvent   evq[16];
static int          evq_len = 0;
static SceneContext ctx = { SCENE_MUSIC_NONE, 0, 0, 0, 0, false, 0, -1, -1 };
static uint32_t     last_key_ms = 0;

static const uint32_t FLIP_DEBOUNCE_MS = 300;
static const uint32_t TYPING_MS        = 1500;

// One {"log":"..."} line per transition; the companion prints these, which
// makes the rotation checkable from `journalctl` without looking at the board.
static void log_event(const char *what) {
    Serial.printf("{\"log\":\"scene %s %s\"}\n", cur ? cur->name : "-", what);
}

// ---------------------------------------------------------------------------

static void new_cycle() {
    int last = order[PLAYLIST_N - 1];
    for (int i = 0; i < PLAYLIST_N; i++) order[i] = i;
    if (!SCENE_SHUFFLE || PLAYLIST_N < 2) return;
    for (int i = PLAYLIST_N - 1; i > 0; i--) {   // Fisher-Yates
        int j = random(0, i + 1);
        int t = order[i]; order[i] = order[j]; order[j] = t;
    }
    // Don't play the same scene twice in a row across the cycle boundary
    if (order[0] == last) { int t = order[0]; order[0] = order[1]; order[1] = t; }
}

static void start_scene(bool by_tap) {
    if (PLAYLIST_N == 0) return;
    if (next_idx == 0) new_cycle();
    cur      = playlist[1 + order[next_idx]];
    next_idx = (next_idx + 1) % PLAYLIST_N;
    evq_len  = 0;   // events from before the scene existed are stale

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
    char buf[48];
    snprintf(buf, sizeof(buf), "%s, heap free %u KB", by_tap ? "start (tap)" : "start",
             (unsigned)(ESP.getFreeHeap() / 1024));
    log_event(buf);
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

    if (paused) { evq_len = 0; return; }
    mode_ms += dt;

    if (mode == MODE_SCENE && cur->event) {
        for (int i = 0; i < evq_len; i++) {
            if (SCENE_EVENT_LOG && evq[i].type != SCENE_EV_AUDIO) {
                char buf[40];
                snprintf(buf, sizeof(buf), "event %d (key %d, music %d, s %d, v %d)", evq[i].type,
                         evq[i].key, evq[i].music, evq[i].strength, evq[i].value);
                log_event(buf);
            }
            cur->event(evq[i]);
        }
    }
    evq_len = 0;

    if (mode == MODE_CALENDAR) {
        if (PLAYLIST_N > 0 && mode_ms >= CAL_PEEK_MS && !calendar_wants_focus())
            start_scene(false);
        return;
    }

    cur->tick(dt);
    pixfb_flush_active();   // in case a pixel-canvas scene didn't flush its own changes

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

    for (int i = 0; i < PLAYLIST_N; i++) order[i] = i;
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

// ---------------------------------------------------------------------------
// Host events

static void queue_event(const SceneEvent &e) {
    if (mode != MODE_SCENE || paused) return;   // nobody to tell
    if (e.type == SCENE_EV_AUDIO && evq_len > 0 && evq[evq_len - 1].type == SCENE_EV_AUDIO) return;
    if (evq_len < (int)(sizeof(evq) / sizeof(evq[0]))) evq[evq_len++] = e;
}

void scene_player_event(const SceneEvent &e) {
    switch (e.type) {
        case SCENE_EV_KEY:    last_key_ms = millis(); break;
        case SCENE_EV_MUSIC:
            ctx.music = e.music;
            if (e.music != SCENE_MUSIC_PLAYING) ctx.intensity = ctx.bass = ctx.bpm = 0;
            break;
        case SCENE_EV_BEAT:   ctx.last_beat_ms = millis(); break;
        case SCENE_EV_CLAUDE: ctx.claude_working = e.value; break;
        default: break;
    }
    queue_event(e);
}

void scene_player_audio(int intensity, int bass, int bpm) {
    ctx.intensity = (uint8_t)constrain(intensity, 0, 100);
    ctx.bass      = (uint8_t)constrain(bass, 0, 100);
    ctx.bpm       = (uint16_t)constrain(bpm, 0, 400);
    SceneEvent e = {};
    e.type = SCENE_EV_AUDIO;
    queue_event(e);
}

const SceneContext &scene_ctx() {
    ctx.typing = last_key_ms && millis() - last_key_ms < TYPING_MS;
    int now = dash_now_min();
    ctx.hour   = now < 0 ? -1 : now / 60;
    ctx.minute = now < 0 ? -1 : now % 60;
    return ctx;
}
