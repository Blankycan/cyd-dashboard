#pragma once
#include <lvgl.h>
#include <stdint.h>

// Pixel framebuffer for sprite-style scenes (the arcade games): one LVGL
// canvas the scene draws into directly, instead of one object per sprite.
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
// The buffer is malloc'd: call pixfb_free() from the scene's finish().

static const int PIXFB_MAX_DIRTY = 16;

struct PixFb {
    lv_obj_t   *canvas;
    lv_color_t *buf;
    int         w, h;
    lv_area_t   dirty[PIXFB_MAX_DIRTY];   // canvas-local, merged as they're added
    int         n_dirty;
};

bool pixfb_create(PixFb &fb, lv_obj_t *parent, int w, int h, lv_color_t bg);  // false if out of memory
void pixfb_free(PixFb &fb);

void pixfb_fill(PixFb &fb, int x, int y, int w, int h, lv_color_t c);         // clipped to the buffer
void pixfb_px(PixFb &fb, int x, int y, lv_color_t c);                          // no invalidation — call pixfb_touch()
void pixfb_touch(PixFb &fb, int x, int y, int w, int h);                       // mark a rect for redraw
void pixfb_flush(PixFb &fb);                                                   // send this frame's dirty rects to LVGL
void pixfb_line(PixFb &fb, int x0, int y0, int x1, int y1, lv_color_t c);

// 1-bit sprite: `rows[r]` holds row r, bit (w-1-c) = column c (so the
// literal reads left to right). Only set bits are drawn.
void pixfb_sprite(PixFb &fb, int x, int y, const uint16_t *rows, int w, int h, lv_color_t c);
