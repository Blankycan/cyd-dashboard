#include "track_card.h"
#include "menu.h"
#include "../config.h"
#include "../theme.h"
#include "../settings.h"
#include "../ui_helpers.h"
#include "../fonts/fonts.h"
#include "../scenes/scene_player.h"
#include <mbedtls/base64.h>
#include <string.h>

static const int ART_PAD   = 5;
static const int SLIDE_MS  = 320;

static lv_obj_t *clip  = nullptr;   // the slot's rect: clips the card while it slides
static lv_obj_t *card  = nullptr;
static lv_obj_t *img   = nullptr;
static lv_obj_t *ph    = nullptr;   // placeholder square when there's no cover
static lv_obj_t *lbl_head, *lbl_title, *lbl_artist, *lbl_album;
static int W, H, TEXT_X, TEXT_W;

// Only allocated while a card is up: a static buffer would permanently take
// 12.8 KB out of the main memory region the scenes' 43 KB canvases come from
static uint8_t     *art_buf = nullptr;
static lv_img_dsc_t art_dsc;
static uint32_t     art_need = 0, art_got = 0;

enum Phase { IDLE, WAITING, SHOWN, LEAVING };
static Phase    phase    = IDLE;
static uint32_t phase_ms = 0;
static int      cur_id   = -1;

static void go_idle() {
    phase = IDLE;
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);   // never drawn from a freed buffer
    free(art_buf);
    art_buf  = nullptr;
    art_need = art_got = 0;
}

static char title[64], artist[48], album[48];

// ---------------------------------------------------------------------------

static bool art_ready() { return art_need > 0 && art_got >= art_need; }

static void show_art() {
    if (art_ready()) {
        lv_img_set_src(img, &art_dsc);
        lv_obj_invalidate(img);
        lv_obj_clear_flag(img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ph, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ph, LV_OBJ_FLAG_HIDDEN);
    }
}

static int line_h(const lv_font_t *f) { return lv_font_get_line_height(f); }

// Stack heading, title (up to two lines), artist and album, centred vertically
static void layout_text() {
    lv_point_t sz;
    lv_txt_get_size(&sz, title, &font_ui_14, 0, 0, TEXT_W, LV_TEXT_FLAG_NONE);
    int title_h = sz.y > line_h(&font_ui_14) ? 2 * line_h(&font_ui_14) : line_h(&font_ui_14);
    lv_obj_set_height(lbl_title, title_h);
    lv_label_set_text(lbl_title, title);

    set_ellipsis_text(lbl_artist, artist);
    set_ellipsis_text(lbl_album, album);
    bool has_album = album[0] != '\0';
    if (has_album) lv_obj_clear_flag(lbl_album, LV_OBJ_FLAG_HIDDEN);
    else           lv_obj_add_flag(lbl_album, LV_OBJ_FLAG_HIDDEN);

    int head_h = line_h(&lv_font_montserrat_10);
    int total  = head_h + 3 + title_h + 2 + line_h(&font_ui_12) + (has_album ? line_h(&font_ui_12) : 0);
    int y = (H - total) / 2;
    lv_obj_set_y(lbl_head, y);              y += head_h + 3;
    lv_obj_set_y(lbl_title, y);             y += title_h + 2;
    lv_obj_set_y(lbl_artist, y);            y += line_h(&font_ui_12);
    lv_obj_set_y(lbl_album, y);
}

static void x_anim_cb(void *o, int32_t v) { lv_obj_set_x((lv_obj_t *)o, v); }

static void slide(int from, int to, lv_anim_path_cb_t path, lv_anim_ready_cb_t ready) {
    lv_anim_del(card, x_anim_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, card);
    lv_anim_set_exec_cb(&a, x_anim_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, SLIDE_MS);
    lv_anim_set_path_cb(&a, path);
    if (ready) lv_anim_set_ready_cb(&a, ready);
    lv_anim_start(&a);
}

static void left_cb(lv_anim_t *) {
    lv_obj_add_flag(clip, LV_OBJ_FLAG_HIDDEN);
    go_idle();
}

// Not while it would get in the way
static bool may_show() {
    return settings.track_card && !menu_is_open() && !scene_player_in_use();
}

static void show() {
    if (!may_show()) {
        Serial.println("{\"log\":\"song card skipped (off, menu open or scene in use)\"}");
        go_idle();
        return;
    }
    Serial.printf("{\"log\":\"song card, cover %u/%u bytes\"}\n", (unsigned)art_got, (unsigned)art_need);
    layout_text();
    show_art();
    lv_obj_set_x(card, W);
    lv_obj_clear_flag(clip, LV_OBJ_FLAG_HIDDEN);
    slide(W, 0, lv_anim_path_ease_out, nullptr);
    phase    = SHOWN;
    phase_ms = millis();
}

static void leave() {
    phase = LEAVING;
    slide(lv_obj_get_x(card), W, lv_anim_path_ease_in, left_cb);
}

static void tick_cb(lv_timer_t *) {
    uint32_t t = millis() - phase_ms;
    if (phase == WAITING && (art_ready() || art_need == 0 || t >= TRACK_ART_WAIT_MS)) show();
    else if (phase == SHOWN && (t >= TRACK_CARD_MS || !settings.track_card || menu_is_open())) leave();
}

// ---------------------------------------------------------------------------

void build_track_card(lv_obj_t *parent, int x, int y, int w, int h) {
    W = w; H = h;
    int art = h - 2 * ART_PAD;
    TEXT_X = ART_PAD + art + 10;
    TEXT_W = w - TEXT_X - 8;

    clip = lv_obj_create(parent);
    lv_obj_remove_style_all(clip);
    lv_obj_set_pos(clip, x, y);
    lv_obj_set_size(clip, w, h);
    lv_obj_clear_flag(clip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(clip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(clip, LV_OBJ_FLAG_HIDDEN);

    card = make_panel(clip, 0, 0, w, h, COL_TRACK_BG);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);

    ph = make_panel(card, ART_PAD, ART_PAD, art, art, COL_TRACK_ART_BG);
    lv_obj_set_style_radius(ph, 3, 0);
    lv_obj_t *note = lv_label_create(ph);
    lv_label_set_text(note, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(note, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(note, COL_TRACK_ART_ICON, 0);
    lv_obj_center(note);

    art_dsc.header.always_zero = 0;
    art_dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    img = lv_img_create(card);
    lv_obj_set_pos(img, ART_PAD, ART_PAD);
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);

    lbl_head = lv_label_create(card);
    lv_label_set_text(lbl_head, "NOW PLAYING");
    lv_obj_set_style_text_font(lbl_head, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_head, COL_TRACK_HEAD, 0);
    lv_obj_set_style_text_letter_space(lbl_head, 1, 0);
    lv_obj_set_x(lbl_head, TEXT_X);

    lbl_title = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_title, &font_ui_14, 0);
    lv_obj_set_style_text_color(lbl_title, COL_TRACK_TITLE, 0);
    lv_label_set_long_mode(lbl_title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_title, TEXT_W);
    lv_obj_set_x(lbl_title, TEXT_X);

    lbl_artist = make_ellipsis_label(card, TEXT_X, 0, TEXT_W, &font_ui_12);
    lv_obj_set_style_text_color(lbl_artist, COL_TRACK_ARTIST, 0);
    lbl_album = make_ellipsis_label(card, TEXT_X, 0, TEXT_W, &font_ui_12);
    lv_obj_set_style_text_color(lbl_album, COL_TRACK_ALBUM, 0);

    lv_timer_create(tick_cb, 100, nullptr);
}

void track_card_begin(int id, const char *t, const char *a, const char *al, int art_w, int art_h) {
    // A card still up for the last song goes at once: its cover is about to be overwritten
    if (phase != IDLE) track_card_hide();
    cur_id = id;
    strlcpy(title,  t,  sizeof(title));
    strlcpy(artist, a,  sizeof(artist));
    strlcpy(album,  al, sizeof(album));

    bool fits = art_w > 0 && art_h > 0 && art_w <= TRACK_ART_MAX && art_h <= TRACK_ART_MAX;
    art_need = fits ? (uint32_t)art_w * art_h * 2 : 0;
    art_got  = 0;
    if (fits && !(art_buf = (uint8_t *)malloc(art_need))) {
        Serial.println("{\"log\":\"song card: no memory for the cover\"}");
        art_need = 0;   // card without it
    }
    if (art_need) {
        art_dsc.data = art_buf;
        art_dsc.header.w  = art_w;
        art_dsc.header.h  = art_h;
        art_dsc.data_size = art_need;
        // Centre a smaller cover in the square
        int box = H - 2 * ART_PAD;
        lv_obj_set_pos(img, ART_PAD + (box - art_w) / 2, ART_PAD + (box - art_h) / 2);
    }
    phase    = WAITING;
    phase_ms = millis();
}

void track_card_art(int id, int offset, const char *b64) {
    if (id != cur_id || phase == IDLE || phase == LEAVING || art_need == 0) return;
    if (offset < 0 || (uint32_t)offset != art_got) return;   // a lost chunk: the cover stays incomplete
    size_t n = 0;
    if (mbedtls_base64_decode(art_buf + offset, art_need - offset, &n,
                              (const unsigned char *)b64, strlen(b64)) != 0) return;
    art_got += n;
    if (art_ready() && phase == SHOWN) show_art();   // arrived after the card gave up waiting
}

bool track_card_take_press(int x, int y) {
    if (phase != SHOWN) return false;
    lv_area_t a;
    lv_obj_get_coords(clip, &a);
    if (x < a.x1 || x > a.x2 || y < a.y1 || y > a.y2) return false;
    leave();
    return true;
}

void track_card_hide() {
    if (phase == IDLE) return;
    lv_anim_del(card, x_anim_cb);
    lv_obj_add_flag(clip, LV_OBJ_FLAG_HIDDEN);
    go_idle();
}
