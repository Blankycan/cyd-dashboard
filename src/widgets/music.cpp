#include "music.h"
#include "../layout.h"
#include "../state.h"
#include "../theme.h"

// ---------------------------------------------------------------------------
// Music animation state machine
// Three states: MA_NOTES (playing), MA_PAUSE (paused/idle), MA_GRASS (disc.)
// lv_obj_clean() on the container removes children + their animations.
// ---------------------------------------------------------------------------

enum MusicAnim { MA_NONE, MA_NOTES, MA_PAUSE, MA_GRASS };
static MusicAnim ma_state = MA_NONE;

static lv_obj_t *dot_music        = nullptr;
static lv_obj_t *lbl_music_title  = nullptr;
static lv_obj_t *lbl_music_artist = nullptr;
static lv_obj_t *music_anim_ctr   = nullptr;

// LVGL anim exec callbacks for the right-side music icon
static void note_bob_cb(void *obj, int32_t val) { lv_obj_set_y((lv_obj_t*)obj, val); }
static void grass_h_cb(void *obj, int32_t val) {
    lv_obj_set_height((lv_obj_t*)obj, val);
    lv_obj_set_y((lv_obj_t*)obj, MUSIC_H - 6 - val);
}
static void grass_x_cb(void *obj, int32_t val) { lv_obj_set_x((lv_obj_t*)obj, val); }

// Simple styled rectangle used to draw note/pause/grass shapes
static lv_obj_t *ma_rect(lv_obj_t *p, int x, int y, int w, int h,
                          lv_color_t col, int radius = 0) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_bg_color(o, col, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

// Bouncing eighth-note icon — shown while music is playing
static void build_ma_notes(lv_obj_t *ctr) {
    lv_obj_t *grp = lv_obj_create(ctr);
    lv_obj_remove_style_all(grp);
    lv_obj_set_size(grp, MA_W, 40);
    lv_obj_set_pos(grp, 0, 12);
    lv_obj_set_style_bg_opa(grp, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grp, 0, 0);
    lv_obj_clear_flag(grp, LV_OBJ_FLAG_SCROLLABLE);

    ma_rect(grp, 10, 4, 20, 4, COL_GLOW, 0);   // beam
    ma_rect(grp, 10, 4,  2, 22, COL_GLOW, 0);  // stem 1
    ma_rect(grp,  5, 23, 6,  5, COL_GLOW, 3);  // head 1
    ma_rect(grp, 28, 4,  2, 16, COL_GLOW, 0);  // stem 2
    ma_rect(grp, 23, 17, 6,  5, COL_GLOW, 3);  // head 2

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, grp);
    lv_anim_set_exec_cb(&a, note_bob_cb);
    lv_anim_set_values(&a, 8, 16);
    lv_anim_set_time(&a, 1500);
    lv_anim_set_playback_time(&a, 1500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

// Two vertical bars — shown when a track is loaded but paused
static void build_ma_pause(lv_obj_t *ctr) {
    int bar_h = 22, bar_y = (MUSIC_H - bar_h) / 2;
    ma_rect(ctr, 10, bar_y, 5, bar_h, COL_TEXT_SEC, 2);
    ma_rect(ctr, 22, bar_y, 5, bar_h, COL_TEXT_SEC, 2);
}

// Swaying grass blades — default when disconnected or no music
static void build_ma_grass(lv_obj_t *ctr) {
    static const int bx[3]       = { 6, 18, 30};
    static const int bh[3]       = {18, 22, 16};
    static const uint32_t bd[3]  = { 0, 500, 1000};
    static const uint32_t sx_delay[3] = {200, 700, 400};
    const int y_bot = MUSIC_H - 6;

    for (int i = 0; i < 3; i++) {
        lv_obj_t *blade = ma_rect(ctr, bx[i], y_bot - bh[i], 3, bh[i], COL_OK, 2);

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, blade);
        lv_anim_set_exec_cb(&a, grass_h_cb);
        lv_anim_set_values(&a, bh[i] - 4, bh[i] + 4);
        lv_anim_set_time(&a, 1500);
        lv_anim_set_playback_time(&a, 1500);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_set_delay(&a, bd[i]);
        lv_anim_start(&a);

        lv_anim_t ax;
        lv_anim_init(&ax);
        lv_anim_set_var(&ax, blade);
        lv_anim_set_exec_cb(&ax, grass_x_cb);
        lv_anim_set_values(&ax, bx[i] - 2, bx[i] + 2);
        lv_anim_set_time(&ax, 1000);
        lv_anim_set_playback_time(&ax, 1000);
        lv_anim_set_repeat_count(&ax, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&ax, lv_anim_path_ease_in_out);
        lv_anim_set_delay(&ax, sx_delay[i]);
        lv_anim_start(&ax);
    }
}

// Tear down current animation and rebuild if the desired state changed
static void set_music_anim(MusicAnim next) {
    if (next == ma_state) return;
    ma_state = next;
    lv_obj_clean(music_anim_ctr);
    switch (next) {
        case MA_NOTES: build_ma_notes(music_anim_ctr); break;
        case MA_PAUSE: build_ma_pause(music_anim_ctr); break;
        case MA_GRASS: build_ma_grass(music_anim_ctr); break;
        default: break;
    }
}

void build_music_panel(lv_obj_t *parent) {
    dot_music = lv_obj_create(parent);
    lv_obj_remove_style_all(dot_music);
    lv_obj_set_size(dot_music, 6, 6);
    lv_obj_set_pos(dot_music, 8, 12);
    lv_obj_set_style_bg_color(dot_music, COL_TEXT_DIM, 0);
    lv_obj_set_style_bg_opa(dot_music, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot_music, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot_music, 0, 0);
    lv_obj_clear_flag(dot_music, LV_OBJ_FLAG_SCROLLABLE);

    lbl_music_title = lv_label_create(parent);
    lv_label_set_text(lbl_music_title, "nothing playing");
    lv_obj_set_style_text_color(lbl_music_title, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl_music_title, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_music_title, 22, 8);
    lv_obj_set_width(lbl_music_title, MUSIC_LABEL_W);
    lv_label_set_long_mode(lbl_music_title, LV_LABEL_LONG_DOT);

    lbl_music_artist = lv_label_create(parent);
    lv_label_set_text(lbl_music_artist, "");
    lv_obj_set_style_text_color(lbl_music_artist, COL_TEXT_SEC, 0);
    lv_obj_set_style_text_font(lbl_music_artist, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(lbl_music_artist, 22, 30);
    lv_obj_set_width(lbl_music_artist, MUSIC_LABEL_W);
    lv_label_set_long_mode(lbl_music_artist, LV_LABEL_LONG_DOT);

    music_anim_ctr = lv_obj_create(parent);
    lv_obj_remove_style_all(music_anim_ctr);
    lv_obj_set_size(music_anim_ctr, MA_W, MUSIC_H);
    lv_obj_set_pos(music_anim_ctr, MA_X, 0);
    lv_obj_set_style_bg_opa(music_anim_ctr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(music_anim_ctr, 0, 0);
    lv_obj_clear_flag(music_anim_ctr, LV_OBJ_FLAG_SCROLLABLE);

    build_ma_grass(music_anim_ctr);
    ma_state = MA_GRASS;
}

// Pick animation + update title/artist text from state
void update_music_ui() {
    MusicAnim anim;
    if (!state.connected || !state.music_active) {
        anim = MA_GRASS;
    } else if (state.music_playing) {
        anim = MA_NOTES;
    } else {
        anim = MA_PAUSE;
    }
    set_music_anim(anim);

    if (state.music_active) {
        lv_color_t dc = state.music_playing ? COL_OK : COL_TEXT_DIM;
        lv_obj_set_style_bg_color(dot_music, dc, 0);
        lv_label_set_text(lbl_music_title, state.music_title);
        lv_obj_set_style_text_color(lbl_music_title,
            state.music_playing ? COL_TEXT_PRI : COL_TEXT_SEC, 0);
        lv_label_set_text(lbl_music_artist, state.music_artist);
        lv_obj_set_style_text_color(lbl_music_artist, COL_TEXT_SEC, 0);
    } else {
        lv_obj_set_style_bg_color(dot_music, COL_TEXT_DIM, 0);
        lv_label_set_text(lbl_music_title, "nothing playing");
        lv_obj_set_style_text_color(lbl_music_title, COL_TEXT_DIM, 0);
        lv_label_set_text(lbl_music_artist, state.idle_msg);
        lv_obj_set_style_text_color(lbl_music_artist, COL_TEXT_DIM, 0);
    }
}
