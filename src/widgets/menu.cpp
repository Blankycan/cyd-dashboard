#include "menu.h"
#include "topbar.h"
#include "../layout.h"
#include "../theme.h"
#include "../settings.h"
#include "../scenes/registry.h"
#include "../scenes/scene_player.h"

// Everything below the scene slot and its divider
#define MENU_Y   (CONTENT_Y + CALENDAR_H + DIV_W)
#define MENU_H   (SCREEN_H - MENU_Y)
#define ROW_H    32
#define PAD_X    10
#define TICK_W   40   // touch target of a scene row's checkbox, at its left end

static lv_obj_t *scr_ref  = nullptr;
static lv_obj_t *menu     = nullptr;   // nullptr while closed
static lv_timer_t *refresh_timer = nullptr;

static lv_obj_t *sw_scenes, *sw_cal, *sw_shuffle;
static lv_obj_t *slider_len, *lbl_len;
static lv_obj_t *lbl_hint;
static lv_obj_t *dim_rows[3];          // the rows that only matter while scenes are on
static lv_obj_t *scene_row[SCENE_REGISTRY_MAX];
static lv_obj_t *scene_box[SCENE_REGISTRY_MAX];
static lv_obj_t *scene_tick[SCENE_REGISTRY_MAX];
static lv_obj_t *scene_play[SCENE_REGISTRY_MAX];
static const Scene *shown_current = nullptr;   // what the highlight last showed

// ---------------------------------------------------------------------------
// Building blocks

static lv_obj_t *make_row(lv_obj_t *parent, int h) {
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, SCREEN_W, h);
    lv_obj_set_style_bg_color(r, COL_MENU_ROW_SEL, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);   // drags scroll the menu
    return r;
}

static lv_obj_t *make_text(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t col) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, col, 0);
    return l;
}

static void style_track(lv_obj_t *o) {
    lv_obj_set_style_bg_color(o, COL_MENU_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(o, COL_MENU_KNOB, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_KNOB);
}

static void switch_changed(lv_event_t *e);

static lv_obj_t *make_switch_row(lv_obj_t *parent, const char *text, bool on) {
    lv_obj_t *r = make_row(parent, ROW_H);
    lv_obj_align(make_text(r, text, &lv_font_montserrat_14, COL_MENU_TEXT), LV_ALIGN_LEFT_MID, PAD_X, 0);

    lv_obj_t *sw = lv_switch_create(r);
    lv_obj_set_size(sw, 38, 20);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -PAD_X, 0);
    style_track(sw);
    lv_obj_set_style_bg_color(sw, COL_MENU_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_pad_all(sw, -3, LV_PART_KNOB);   // knob sits inside the track
    lv_obj_set_ext_click_area(sw, 8);
    if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, switch_changed, LV_EVENT_VALUE_CHANGED, nullptr);
    return sw;
}

static void make_divider(lv_obj_t *parent) {
    lv_obj_t *d = lv_obj_create(parent);
    lv_obj_remove_style_all(d);
    lv_obj_set_size(d, SCREEN_W, DIV_W);
    lv_obj_set_style_bg_color(d, COL_MENU_DIVIDER, 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
}

// ---------------------------------------------------------------------------
// Keeping it in step with the settings and the player

static void set_box(int i) {
    bool on = settings_scene_on(i);
    lv_obj_set_style_bg_opa(scene_box[i], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(scene_box[i], on ? COL_MENU_ACCENT : COL_MENU_HINT, 0);
    if (on) lv_obj_clear_flag(scene_tick[i], LV_OBJ_FLAG_HIDDEN);
    else    lv_obj_add_flag(scene_tick[i], LV_OBJ_FLAG_HIDDEN);
}

static void refresh_hint() {
    int ticked = 0;
    for (int i = 0; i < scene_count(); i++) ticked += settings_scene_on(i);
    const char *hint = !settings.scenes_on ? "Scenes off: calendar only"
                     : ticked == 0          ? "None ticked: calendar only"
                     : ticked == 1          ? "One ticked: only it plays"
                                            : "Tap a name to preview it above";
    lv_label_set_text(lbl_hint, hint);
    for (lv_obj_t *r : dim_rows) lv_obj_set_style_opa(r, settings.scenes_on ? LV_OPA_COVER : LV_OPA_40, 0);
}

static void refresh_current() {
    const Scene *now = scene_player_current();
    if (now == shown_current) return;
    shown_current = now;
    for (int i = 0; i < scene_count(); i++) {
        bool is = scene_entry(i).scene == now;
        lv_obj_set_style_bg_opa(scene_row[i], is ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        if (is) lv_obj_clear_flag(scene_play[i], LV_OBJ_FLAG_HIDDEN);
        else    lv_obj_add_flag(scene_play[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void changed() {
    settings_touch();
    scene_player_settings_changed();
    refresh_hint();
}

// ---------------------------------------------------------------------------
// Events

static void switch_changed(lv_event_t *e) {
    lv_obj_t *sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (sw == sw_scenes)  settings.scenes_on   = on;
    if (sw == sw_cal)     settings.cal_between = on;
    if (sw == sw_shuffle) settings.shuffle     = on;
    changed();
}

static void set_len_label() {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d min", settings.scene_min);
    lv_label_set_text(lbl_len, buf);
}

// The scene length slider doesn't take touches itself (it would grab every
// drag that lands on it, so scrolling the menu past it adjusted it instead).
// Its row does: a drag that goes sideways first moves the slider, one that
// goes up or down scrolls the menu as usual, and a tap jumps to that value.
static const int SLIDE_GRAB_PX = 6;
static lv_point_t slide_start;
static bool       slide_grabbed;

static void slide_to(int x) {
    lv_area_t a;
    lv_obj_get_coords(slider_len, &a);
    int span = LV_MAX(1, a.x2 - a.x1);
    int v = 1 + ((x - a.x1) * (SCENE_MIN_MAX - 1) + span / 2) / span;
    v = LV_CLAMP(1, v, SCENE_MIN_MAX);
    if (v == settings.scene_min) return;
    settings.scene_min = (uint8_t)v;
    lv_slider_set_value(slider_len, v, LV_ANIM_OFF);
    set_len_label();
    settings_touch();   // read live by the player: nothing to rebuild
}

static void slide_row_event(lv_event_t *e) {
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    switch (lv_event_get_code(e)) {
        case LV_EVENT_PRESSED:
            slide_start   = p;
            slide_grabbed = false;
            break;
        case LV_EVENT_PRESSING:
            if (!slide_grabbed) {
                if (lv_indev_get_scroll_obj(indev)) break;   // the menu is scrolling
                int dx = LV_ABS(p.x - slide_start.x), dy = LV_ABS(p.y - slide_start.y);
                if (dx < SLIDE_GRAB_PX || dx <= dy) break;
                slide_grabbed = true;
                lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);   // no scrolling off a wobbly finger
            }
            slide_to(p.x);
            break;
        case LV_EVENT_CLICKED:   // a tap, not a scroll
            slide_to(p.x);
            break;
        case LV_EVENT_RELEASED:
        case LV_EVENT_PRESS_LOST:
            if (slide_grabbed) lv_obj_add_flag(menu, LV_OBJ_FLAG_SCROLLABLE);
            slide_grabbed = false;
            break;
        default: break;
    }
}

static void tick_clicked(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    settings_set_scene_on(i, !settings_scene_on(i));
    set_box(i);
    changed();
}

static void row_clicked(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    scene_player_play(scene_entry(i).scene);
}

static void all_none_clicked(lv_event_t *e) {
    bool on = (bool)(intptr_t)lv_event_get_user_data(e);
    for (int i = 0; i < scene_count(); i++) { settings_set_scene_on(i, on); set_box(i); }
    changed();
}

static void refresh_cb(lv_timer_t *) {
    if (!menu) return;
    refresh_current();
    if (lv_disp_get_inactive_time(nullptr) > MENU_IDLE_CLOSE_MS) menu_close();
}

// ---------------------------------------------------------------------------
// Open / close

static lv_obj_t *make_text_button(lv_obj_t *parent, const char *text, bool on) {
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 44, ROW_H);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(make_text(b, text, &lv_font_montserrat_12, COL_MENU_ACCENT));
    lv_obj_add_event_cb(b, all_none_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)on);
    return b;
}

static void open_menu() {
    menu = lv_obj_create(scr_ref);
    lv_obj_remove_style_all(menu);
    lv_obj_set_pos(menu, 0, MENU_Y);
    lv_obj_set_size(menu, SCREEN_W, MENU_H);
    lv_obj_set_style_bg_color(menu, COL_MENU_BG, 0);
    lv_obj_set_style_bg_opa(menu, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(menu, LV_DIR_VER);
    lv_obj_set_style_bg_color(menu, COL_MENU_HINT, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(menu, LV_OPA_50, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(menu, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(menu, 1, LV_PART_SCROLLBAR);

    sw_scenes = make_switch_row(menu, "Scenes", settings.scenes_on);
    sw_cal    = make_switch_row(menu, "Calendar between", settings.cal_between);
    sw_shuffle = make_switch_row(menu, "Shuffle", settings.shuffle);
    dim_rows[0] = lv_obj_get_parent(sw_cal);
    dim_rows[1] = lv_obj_get_parent(sw_shuffle);

    // Scene length: 1 .. SCENE_MIN_MAX minutes, whole minutes only
    lv_obj_t *r = make_row(menu, 46);
    dim_rows[2] = r;
    lv_obj_align(make_text(r, "Scene length", &lv_font_montserrat_14, COL_MENU_TEXT), LV_ALIGN_TOP_LEFT, PAD_X, 7);
    lbl_len = make_text(r, "", &lv_font_montserrat_14, COL_MENU_ACCENT);
    lv_obj_align(lbl_len, LV_ALIGN_TOP_RIGHT, -PAD_X, 7);
    slider_len = lv_slider_create(r);
    lv_obj_set_size(slider_len, SCREEN_W - 2 * PAD_X - 12, 6);
    lv_obj_align(slider_len, LV_ALIGN_BOTTOM_MID, 0, -9);
    lv_slider_set_range(slider_len, 1, SCENE_MIN_MAX);
    lv_slider_set_value(slider_len, settings.scene_min, LV_ANIM_OFF);
    style_track(slider_len);
    lv_obj_set_style_bg_color(slider_len, COL_MENU_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider_len, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(slider_len, 5, LV_PART_KNOB);
    lv_obj_clear_flag(slider_len, LV_OBJ_FLAG_CLICKABLE);   // the row handles it, see slide_row_event()
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(r, slide_row_event, LV_EVENT_ALL, nullptr);
    set_len_label();

    make_divider(menu);

    // Scenes: heading with All / None, a hint, then one row per scene
    r = make_row(menu, ROW_H);
    lv_obj_align(make_text(r, "SCENES", &lv_font_montserrat_12, COL_MENU_HEADING), LV_ALIGN_LEFT_MID, PAD_X, 0);
    lv_obj_align(make_text_button(r, "None", false), LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_align(make_text_button(r, "All", true), LV_ALIGN_RIGHT_MID, -48, 0);

    r = make_row(menu, 18);
    lbl_hint = make_text(r, "", &lv_font_montserrat_12, COL_MENU_HINT);
    lv_obj_align(lbl_hint, LV_ALIGN_TOP_LEFT, PAD_X, 0);

    for (int i = 0; i < scene_count(); i++) {
        r = make_row(menu, ROW_H);
        scene_row[i] = r;
        lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(r, row_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        // The checkbox gets its own touch target so ticking never previews
        lv_obj_t *zone = lv_obj_create(r);
        lv_obj_remove_style_all(zone);
        lv_obj_set_size(zone, TICK_W, ROW_H);
        lv_obj_add_flag(zone, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(zone, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(zone, tick_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *box = lv_obj_create(zone);
        lv_obj_remove_style_all(box);
        lv_obj_set_size(box, 16, 16);
        lv_obj_align(box, LV_ALIGN_LEFT_MID, PAD_X, 0);
        lv_obj_set_style_radius(box, 3, 0);
        lv_obj_set_style_border_width(box, 2, 0);
        lv_obj_set_style_bg_color(box, COL_MENU_ACCENT, 0);
        lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);
        scene_box[i] = box;
        scene_tick[i] = make_text(box, LV_SYMBOL_OK, &lv_font_montserrat_10, COL_MENU_BG);
        lv_obj_center(scene_tick[i]);

        lv_obj_align(make_text(r, scene_entry(i).title, &lv_font_montserrat_14, COL_MENU_TEXT),
                     LV_ALIGN_LEFT_MID, TICK_W, 0);
        scene_play[i] = make_text(r, LV_SYMBOL_PLAY, &lv_font_montserrat_12, COL_MENU_ACCENT);
        lv_obj_align(scene_play[i], LV_ALIGN_RIGHT_MID, -PAD_X, 0);
        set_box(i);
    }

    shown_current = (const Scene *)-1;   // force the first highlight
    refresh_current();
    refresh_hint();
    topbar_set_menu_open(true);
    refresh_timer = lv_timer_create(refresh_cb, 300, nullptr);
}

void menu_close() {
    if (!menu) return;
    lv_timer_del(refresh_timer);
    refresh_timer = nullptr;
    lv_obj_del_async(menu);   // may be closing from one of its own event callbacks
    menu = nullptr;
    topbar_set_menu_open(false);
    settings_save();
}

static void menu_btn_clicked(lv_event_t *) {
    if (menu) menu_close();
    else      open_menu();
}

// ---------------------------------------------------------------------------

void build_menu(lv_obj_t *scr) {
    scr_ref = scr;
    lv_obj_add_event_cb(topbar_menu_button(), menu_btn_clicked, LV_EVENT_CLICKED, nullptr);
}

bool menu_is_open() {
    return menu != nullptr;
}

bool menu_owns_point(int x, int y) {
    if (y < TOPBAR_H && x >= SCREEN_W - TOPBAR_MENU_BTN_W) return true;
    return menu && y >= MENU_Y;
}
