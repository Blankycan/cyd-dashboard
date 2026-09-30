#include "registry.h"
#include "quotes.h"
#include "../theme.h"
#include "../state.h"
#include "../fonts/fonts.h"
#include <stdio.h>
#include <string.h>

// Quote of the day — types a quote out like a typewriter (with a blinking
// cursor and short pauses after punctuation), fades the author in, holds it
// for a while, then fades out and reports done by itself. The quote is picked
// from the date, so it's the same one all day. request_stop() fades out early.
// Touch: tap for a different quote.

static const float    CHARS_PER_S   = 28.0f;
static const uint32_t PUNCT_PAUSE_MS = 220;
static const uint32_t HOLD_MS       = 45 * 1000;
static const uint32_t FADE_MS       = 800;
static const uint32_t CURSOR_MS     = 450;

enum Phase { TYPING, HOLDING, FADING, DONE };

static lv_obj_t   *box, *lbl_text, *lbl_author;
static int         area_w, area_h, quote_idx;
static Phase       phase;
static uint32_t    phase_ms, pause_ms, cursor_ms;
static float       type_acc;
static uint32_t    shown;         // bytes of the quote revealed so far
static bool        cursor_on;
static char        buf[160];

static int quote_of_the_day() {
    // Same quote all day: hash the date the host sent ("Tue 29 Sep")
    if (!state.date_str[0]) return random(0, QUOTES_N);
    uint32_t h = 2166136261u;
    for (const char *p = state.date_str; *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; }
    return h % QUOTES_N;
}

static void render_text() {
    const char *q = QUOTES[quote_idx].text;
    uint32_t n = shown < sizeof(buf) - 2 ? shown : sizeof(buf) - 2;
    memcpy(buf, q, n);
    // Blinking cursor while typing
    buf[n] = (phase == TYPING && cursor_on) ? '_' : '\0';
    buf[n + 1] = '\0';
    lv_label_set_text(lbl_text, buf);
}

// Lay out the full quote once so typing never shifts it: size the text for
// its final length, then centre text + author vertically in the area.
static void layout_quote() {
    const Quote &q = QUOTES[quote_idx];
    int text_w = area_w - 24;
    const lv_font_t *font = &font_ui_14;
    lv_point_t sz;
    lv_txt_get_size(&sz, q.text, font, 0, 0, text_w, LV_TEXT_FLAG_NONE);
    if (sz.y + 18 > area_h) {   // too tall for this area: drop to the small font
        font = &font_ui_12;
        lv_txt_get_size(&sz, q.text, font, 0, 0, text_w, LV_TEXT_FLAG_NONE);
    }
    int author_h = lv_font_get_line_height(&font_ui_12);
    int top = (area_h - (sz.y + 2 + author_h)) / 2;
    if (top < 0) top = 0;

    lv_obj_set_style_text_font(lbl_text, font, 0);
    lv_obj_set_pos(lbl_text, 12, top);
    lv_obj_set_width(lbl_text, text_w);

    char author[48];
    snprintf(author, sizeof(author), "- %s", q.author);
    lv_label_set_text(lbl_author, author);
    lv_obj_set_pos(lbl_author, 12, top + sz.y + 2);
    lv_obj_set_width(lbl_author, text_w);
    lv_obj_set_style_text_opa(lbl_author, LV_OPA_TRANSP, 0);
}

static void begin_quote(int idx) {
    quote_idx = idx;
    phase     = TYPING;
    phase_ms  = pause_ms = cursor_ms = 0;
    type_acc  = 0;
    shown     = 0;
    cursor_on = true;
    lv_obj_set_style_opa(box, LV_OPA_COVER, 0);
    layout_quote();
    render_text();
}

static void start(lv_obj_t *area, int w, int h) {
    area_w = w;
    area_h = h;

    // Everything in one box so the fade-out is a single opacity change
    box = lv_obj_create(area);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, w, h);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lbl_text = lv_label_create(box);
    lv_obj_set_style_text_color(lbl_text, COL_SCENE_QUOTE_TEXT, 0);
    lv_label_set_long_mode(lbl_text, LV_LABEL_LONG_WRAP);

    lbl_author = lv_label_create(box);
    lv_obj_set_style_text_font(lbl_author, &font_ui_12, 0);
    lv_obj_set_style_text_color(lbl_author, COL_SCENE_QUOTE_AUTHOR, 0);
    lv_obj_set_style_text_align(lbl_author, LV_TEXT_ALIGN_RIGHT, 0);

    begin_quote(quote_of_the_day());
}

static void tick(uint32_t dt_ms) {
    phase_ms += dt_ms;
    const char *q = QUOTES[quote_idx].text;
    uint32_t len = strlen(q);

    switch (phase) {
    case TYPING: {
        cursor_ms += dt_ms;
        if (cursor_ms >= CURSOR_MS) { cursor_ms = 0; cursor_on = !cursor_on; render_text(); }
        if (pause_ms > dt_ms) { pause_ms -= dt_ms; break; }
        pause_ms = 0;
        type_acc += CHARS_PER_S * dt_ms / 1000.0f;
        bool changed = false;
        while (type_acc >= 1.0f && shown < len) {
            type_acc -= 1.0f;
            _lv_txt_encoded_next(q, &shown);   // whole UTF-8 characters only
            changed = true;
            char c = q[shown - 1];
            if (c == '.' || c == ',' || c == ':' || c == ';') { pause_ms = PUNCT_PAUSE_MS; type_acc = 0; break; }
        }
        if (changed) render_text();
        if (shown >= len) { phase = HOLDING; phase_ms = 0; render_text(); }
        break;
    }
    case HOLDING: {
        // Author fades in over the first moment of the hold
        uint32_t a = phase_ms < FADE_MS ? phase_ms * 255 / FADE_MS : 255;
        lv_obj_set_style_text_opa(lbl_author, (lv_opa_t)a, 0);
        if (phase_ms >= HOLD_MS) { phase = FADING; phase_ms = 0; }
        break;
    }
    case FADING: {
        uint32_t a = phase_ms < FADE_MS ? 255 - phase_ms * 255 / FADE_MS : 0;
        lv_obj_set_style_opa(box, (lv_opa_t)a, 0);
        if (phase_ms >= FADE_MS) phase = DONE;
        break;
    }
    case DONE:
        break;
    }
}

static void request_stop() {
    if (phase != FADING && phase != DONE) { phase = FADING; phase_ms = 0; }
}

static bool is_done() { return phase == DONE; }

static void touch(SceneTouch type, int, int) {
    if (type != SCENE_TOUCH_PRESS || phase == FADING || phase == DONE) return;
    int next = quote_idx;
    if (QUOTES_N > 1) while (next == quote_idx) next = random(0, QUOTES_N);
    begin_quote(next);
}

const Scene scene_quote = { "quote", start, tick, request_stop, is_done, touch, nullptr };
SCENE_REGISTER(scene_quote, "Quote of the day");
