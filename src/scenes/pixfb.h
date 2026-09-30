#pragma once
#include <lvgl.h>
#include <stdint.h>

// Pixel framebuffer for scenes with many moving things (the arcade games,
// leaves, snow, stars...): one LVGL canvas the scene draws into directly,
// instead of one object per sprite — see scene.h for why that matters.
//
// Draw calls record the rectangle they touched; pixfb_flush() hands those to
// LVGL once per frame, merged into at most PIXFB_MAX_DIRTY rects. Invalidating
// every sprite and line separately would overflow LVGL's small list of
// pending areas, and LVGL then falls back to redrawing the whole screen.
//
// Typical frame: erase last frame's sprite rects with pixfb_fill(bg), draw
// everything at its new position, then pixfb_flush(). Erase everything before
// drawing anything, so an erase can't cut into a sprite drawn earlier.
//
// Painted scenes (a static backdrop with things moving over it) can draw the
// backdrop once, pixfb_bg_save() it, and then erase a moving sprite with
// pixfb_bg_restore() — copying back what was behind it — instead of
// redrawing the scene.
//
// Buffers are malloc'd: call pixfb_free() from the scene's finish().

static const int PIXFB_MAX_DIRTY = 16;

struct PixFb {
    lv_obj_t   *canvas;
    lv_color_t *buf;
    int         w, h;
    lv_color_t *bg;                       // saved backdrop, or nullptr
    lv_area_t   dirty[PIXFB_MAX_DIRTY];   // canvas-local, merged as they're added
    int         n_dirty;
};

bool pixfb_create(PixFb &fb, lv_obj_t *parent, int w, int h, lv_color_t bg);  // false if out of memory
void pixfb_free(PixFb &fb);

void pixfb_fill(PixFb &fb, int x, int y, int w, int h, lv_color_t c);         // clipped to the buffer
void pixfb_px(PixFb &fb, int x, int y, lv_color_t c);                          // no invalidation — call pixfb_touch()
void pixfb_touch(PixFb &fb, int x, int y, int w, int h);                       // mark a rect for redraw
void pixfb_flush(PixFb &fb);                                                   // send this frame's dirty rects to LVGL
void pixfb_flush_active();   // flush the running scene's buffer; the scene player calls this after every frame
void pixfb_line(PixFb &fb, int x0, int y0, int x1, int y1, lv_color_t c);

void pixfb_fill_circle(PixFb &fb, int cx, int cy, int r, lv_color_t c);
void pixfb_fill_tri(PixFb &fb, int x0, int y0, int x1, int y1, int x2, int y2, lv_color_t c);

// Translucent versions: tint what's already there toward `c` by `t` (0-1),
// e.g. a beam of light over a window. The glow fades out toward its edge.
void pixfb_blend_tri(PixFb &fb, int x0, int y0, int x1, int y1, int x2, int y2, lv_color_t c, float t);
void pixfb_glow(PixFb &fb, int cx, int cy, int r, lv_color_t c, float t);

bool pixfb_bg_save(PixFb &fb);                                    // snapshot the current image; false if out of memory
void pixfb_bg_restore(PixFb &fb, int x, int y, int w, int h);     // copy the snapshot back over a rect

// c1 → c2 as t goes 0 → 1 (for gradients and fading particles)
inline lv_color_t pixfb_mix(lv_color_t c1, lv_color_t c2, float t) {
    if (t <= 0) return c1;
    if (t >= 1) return c2;
    return lv_color_mix(c2, c1, (uint8_t)(t * 255));
}

// 1-bit sprite: `rows[r]` holds row r, bit (w-1-c) = column c (so the
// literal reads left to right). Only set bits are drawn.
void pixfb_sprite(PixFb &fb, int x, int y, const uint16_t *rows, int w, int h, lv_color_t c);

// Multi-colour sprite drawn from text art, one string per row: '1'..'9' pick
// pal[0..8], anything else ('.', ' ') is transparent. Easy to author and read:
//     static const char *const MUG[] = { ".11.", "1221", "1221", ".11." };
//     pixfb_art(fb, x, y, MUG, 4, pal);
// `flip` mirrors it left to right.
void pixfb_art(PixFb &fb, int x, int y, const char *const *rows, int h, const lv_color_t *pal, bool flip = false);

// Numbers in a 5x7 pixel font (digits 7 px apart), for scores and counters.
// `pad` zero-pads to at least that many digits; `outline`, if given, draws a
// 1 px border in that colour around every stroke (readable over busy art).
// Returns the width in pixels (pixfb_number_width() gives it without drawing).
static const int PIXFB_DIGIT_W = 5, PIXFB_DIGIT_H = 7, PIXFB_DIGIT_PITCH = 7;
int pixfb_number(PixFb &fb, int x, int y, long value, lv_color_t c, int pad = 0, const lv_color_t *outline = nullptr);
int pixfb_number_width(long value, int pad = 0);
