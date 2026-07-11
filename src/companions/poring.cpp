#include "poring.h"
#include "../theme.h"
#include <lvgl.h>

// ---------------------------------------------------------------------------
// Poring — Ragnarok Online-style pink slime companion
// Expressive eyes, stem+leaf, sparkles, reacts to WPM and touch.
// ---------------------------------------------------------------------------

// Colors (kept local — not in theme since these are character-specific)
#define COL_P_BODY    lv_color_hex(0xF08AC0)   // warm pink
#define COL_P_DARK    lv_color_hex(0x1F0A14)   // very dark plum for eyes/mouth
#define COL_P_LEAF    lv_color_hex(0x68AA5E)   // moss green stem & leaf
#define COL_P_BLUSH   lv_color_hex(0xFFB8D4)   // lighter pink blush
#define COL_P_WHITE   lv_color_hex(0xFEFAE0)   // eye highlight (reuse cream)

#define N_SPARKS 5

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
enum PoringState { PS_IDLE, PS_SLOW, PS_FAST, PS_MUSIC };
static PoringState p_state = PS_IDLE;

static lv_obj_t *p_container = nullptr;
static lv_obj_t *p_body      = nullptr;
static lv_obj_t *p_eye_l     = nullptr;
static lv_obj_t *p_eye_r     = nullptr;
static lv_obj_t *p_sparks[N_SPARKS];
static lv_timer_t *p_spark_timer = nullptr;

static int32_t p_base_x = 0;
static int32_t p_base_y = 0;
static int32_t p_body_y = 0;   // body y within container (for squish compensation)

static bool  p_blinking   = false;
static int32_t p_eye_open_h = 8;   // normal eye height, used to restore after blink

// ---------------------------------------------------------------------------
// Animation callbacks
// ---------------------------------------------------------------------------
static void p_bob_cb(void *obj, int32_t val) {
    lv_obj_set_y((lv_obj_t*)obj, val);
}

static void p_sway_cb(void *obj, int32_t val) {
    lv_obj_set_x((lv_obj_t*)obj, val);
}

static void p_squish_cb(void *obj, int32_t val) {
    int diff = 62 - val;
    lv_obj_set_height((lv_obj_t*)obj, val);
    lv_obj_set_y((lv_obj_t*)obj, p_body_y + diff);  // keep bottom edge fixed
}

static void p_opa_cb(void *obj, int32_t val) {
    lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)val, 0);
}

// ---------------------------------------------------------------------------
// Blink
// ---------------------------------------------------------------------------
static void p_blink_restore_cb(lv_timer_t *t) {
    lv_timer_del(t);
    lv_obj_set_height(p_eye_l, p_eye_open_h);
    lv_obj_set_height(p_eye_r, p_eye_open_h);
    lv_obj_set_style_radius(p_eye_l, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(p_eye_r, LV_RADIUS_CIRCLE, 0);
    p_blinking = false;
}

static void p_blink_cb(lv_timer_t *) {
    if (p_blinking) return;
    p_blinking = true;
    lv_obj_set_height(p_eye_l, 2);
    lv_obj_set_height(p_eye_r, 2);
    lv_obj_set_style_radius(p_eye_l, 1, 0);
    lv_obj_set_style_radius(p_eye_r, 1, 0);
    lv_timer_create(p_blink_restore_cb, 150, nullptr);
}

// ---------------------------------------------------------------------------
// Sparkles
// ---------------------------------------------------------------------------
static uint8_t p_spark_idx = 0;

static void p_spark_tick_cb(lv_timer_t *) {
    lv_obj_t *sp = p_sparks[p_spark_idx % N_SPARKS];
    p_spark_idx++;
    lv_obj_set_style_opa(sp, LV_OPA_COVER, 0);
    lv_anim_del(sp, p_opa_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, sp);
    lv_anim_set_exec_cb(&a, p_opa_cb);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 500);
    lv_anim_start(&a);
}

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------
static void p_start_bob(uint32_t half_ms, int32_t range) {
    lv_anim_del(p_container, p_bob_cb);
    lv_anim_del(p_container, p_sway_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, p_container);
    lv_anim_set_exec_cb(&a, p_bob_cb);
    lv_anim_set_values(&a, p_base_y - range, p_base_y + range);
    lv_anim_set_time(&a, half_ms);
    lv_anim_set_playback_time(&a, half_ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void p_start_squish(uint32_t period_ms, int32_t min_h) {
    lv_anim_del(p_body, p_squish_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, p_body);
    lv_anim_set_exec_cb(&a, p_squish_cb);
    lv_anim_set_values(&a, 62, min_h);
    lv_anim_set_time(&a, period_ms / 2);
    lv_anim_set_playback_time(&a, period_ms / 2);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void p_stop_squish() {
    lv_anim_del(p_body, p_squish_cb);
    lv_obj_set_height(p_body, 62);
    lv_obj_set_y(p_body, p_body_y);
}

static void p_apply_state() {
    if (p_spark_timer) { lv_timer_del(p_spark_timer); p_spark_timer = nullptr; }
    for (int i = 0; i < N_SPARKS; i++)
        lv_obj_set_style_opa(p_sparks[i], LV_OPA_TRANSP, 0);

    switch (p_state) {
        case PS_FAST:
            p_start_bob(300, 5);
            p_start_squish(600, 52);
            p_spark_timer = lv_timer_create(p_spark_tick_cb, 320, nullptr);
            break;
        case PS_SLOW:
            p_start_bob(600, 4);
            p_start_squish(1200, 57);
            p_spark_timer = lv_timer_create(p_spark_tick_cb, 650, nullptr);
            break;
        case PS_MUSIC: {
            p_stop_squish();
            lv_anim_del(p_container, p_bob_cb);
            lv_anim_del(p_container, p_sway_cb);
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, p_container);
            lv_anim_set_exec_cb(&a, p_sway_cb);
            lv_anim_set_values(&a, p_base_x - 3, p_base_x + 3);
            lv_anim_set_time(&a, 500);
            lv_anim_set_playback_time(&a, 500);
            lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
            lv_anim_start(&a);
            break;
        }
        default:  // PS_IDLE
            p_start_bob(1000, 3);
            p_stop_squish();
            break;
    }
}

// ---------------------------------------------------------------------------
// Companion interface implementations
// ---------------------------------------------------------------------------
static void poring_update(int wpm, bool active, bool music_playing, bool /*connected*/) {
    PoringState ns;
    if      (active && wpm >= 50) ns = PS_FAST;
    else if (active && wpm >= 10) ns = PS_SLOW;
    else if (music_playing)       ns = PS_MUSIC;
    else                          ns = PS_IDLE;
    if (ns == p_state) return;
    p_state = ns;
    p_apply_state();
}

static void poring_on_touch() {
    // Squish flat then spring back
    lv_anim_del(p_body, p_squish_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, p_body);
    lv_anim_set_exec_cb(&a, p_squish_cb);
    lv_anim_set_values(&a, 62, 42);
    lv_anim_set_time(&a, 80);
    lv_anim_set_playback_time(&a, 220);
    lv_anim_set_playback_delay(&a, 30);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_start(&a);
}

static void poring_on_sleep() {
    // Close eyes
    lv_obj_set_height(p_eye_l, 2);
    lv_obj_set_height(p_eye_r, 2);
    lv_obj_set_style_radius(p_eye_l, 1, 0);
    lv_obj_set_style_radius(p_eye_r, 1, 0);
    // Slow idle bob
    p_state = PS_IDLE;
    p_apply_state();
}

static void poring_on_wake() {
    // Reopen eyes
    lv_obj_set_height(p_eye_l, p_eye_open_h);
    lv_obj_set_height(p_eye_r, p_eye_open_h);
    lv_obj_set_style_radius(p_eye_l, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_radius(p_eye_r, LV_RADIUS_CIRCLE, 0);
}

// Helper — plain filled rectangle/oval with no decoration
static lv_obj_t *p_rect(lv_obj_t *par, int x, int y, int w, int h,
                         lv_color_t col, int r = 0, lv_opa_t opa = LV_OPA_COVER) {
    lv_obj_t *o = lv_obj_create(par);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_bg_color(o, col, 0);
    lv_obj_set_style_bg_opa(o, opa, 0);
    lv_obj_set_style_radius(o, r, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void poring_build(lv_obj_t *parent, int w, int h) {
    // Container — 80×90, centred in sidebar
    const int CW = 80, CH = 90;
    p_base_x = (w - CW) / 2;
    p_base_y = (h - CH) / 2;

    p_container = lv_obj_create(parent);
    lv_obj_remove_style_all(p_container);
    lv_obj_set_size(p_container, CW, CH);
    lv_obj_set_pos(p_container, p_base_x, p_base_y);
    lv_obj_set_style_bg_opa(p_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p_container, 0, 0);
    lv_obj_clear_flag(p_container, LV_OBJ_FLAG_SCROLLABLE);

    // Stem (green, thin rect)
    p_rect(p_container, 38, 8, 4, 18, COL_P_LEAF, 2);

    // Leaf (green oval at top of stem)
    p_rect(p_container, 33, 0, 14, 12, COL_P_LEAF, 6);

    // Body (large pink oval — wider than tall)
    p_body_y = 24;
    p_body = p_rect(p_container, 5, p_body_y, 70, 62, COL_P_BODY, 31);

    // Blush marks (semi-transparent lighter pink ovals, on cheeks)
    p_rect(p_container,  7, 52, 15, 8, COL_P_BLUSH, 4, LV_OPA_60);
    p_rect(p_container, 58, 52, 15, 8, COL_P_BLUSH, 4, LV_OPA_60);

    // Eyes (dark circles)
    p_eye_l = p_rect(p_container, 19, 40, 8, 8, COL_P_DARK, LV_RADIUS_CIRCLE);
    p_eye_r = p_rect(p_container, 53, 40, 8, 8, COL_P_DARK, LV_RADIUS_CIRCLE);

    // Eye highlights (small white dots, upper-right of each eye)
    p_rect(p_container, 24, 41, 3, 3, COL_P_WHITE, LV_RADIUS_CIRCLE);
    p_rect(p_container, 58, 41, 3, 3, COL_P_WHITE, LV_RADIUS_CIRCLE);

    // Mouth (small dark oval, centered low on face)
    p_rect(p_container, 30, 60, 20, 6, COL_P_DARK, 3);

    // Sparkles — scattered around the body, hidden until fast typing
    static const int sx[N_SPARKS] = {  2, 73,  0, 70, 36 };
    static const int sy[N_SPARKS] = { 30, 28, 55, 56, 20 };
    for (int i = 0; i < N_SPARKS; i++) {
        p_sparks[i] = p_rect(p_container, sx[i], sy[i], 4, 4, COL_GLOW, LV_RADIUS_CIRCLE);
        lv_obj_set_style_opa(p_sparks[i], LV_OPA_TRANSP, 0);
    }

    // Blink every ~4.5s
    lv_timer_create(p_blink_cb, 4500, nullptr);

    p_apply_state();
}

// ---------------------------------------------------------------------------
// Public companion descriptor
// ---------------------------------------------------------------------------
const Companion poring_companion = {
    poring_build,
    poring_update,
    poring_on_touch,
    poring_on_sleep,
    poring_on_wake,
};
