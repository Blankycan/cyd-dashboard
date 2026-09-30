#include "pixfb.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static PixFb *active = nullptr;   // the running scene's buffer, if it has one

void pixfb_flush_active() {
    if (active) pixfb_flush(*active);
}

bool pixfb_create(PixFb &fb, lv_obj_t *parent, int w, int h, lv_color_t bg) {
    active = &fb;
    fb.w = w;
    fb.h = h;
    fb.buf = (lv_color_t *)malloc(sizeof(lv_color_t) * w * h);
    fb.canvas = nullptr;
    fb.bg = nullptr;
    fb.n_dirty = 0;
    if (!fb.buf) return false;
    fb.canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(fb.canvas, fb.buf, w, h, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(fb.canvas, bg, LV_OPA_COVER);
    return true;
}

void pixfb_free(PixFb &fb) {
    if (active == &fb) active = nullptr;
    free(fb.buf);
    free(fb.bg);
    fb.buf = fb.bg = nullptr;
    fb.canvas = nullptr;   // deleted by the scene player along with the area
}

static int area_of(const lv_area_t &a) { return (a.x2 - a.x1 + 1) * (a.y2 - a.y1 + 1); }

static lv_area_t union_of(const lv_area_t &a, const lv_area_t &b) {
    return { LV_MIN(a.x1, b.x1), LV_MIN(a.y1, b.y1), LV_MAX(a.x2, b.x2), LV_MAX(a.y2, b.y2) };
}

void pixfb_touch(PixFb &fb, int x, int y, int w, int h) {
    if (!fb.canvas || w <= 0 || h <= 0) return;
    // Clip to the canvas
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > fb.w ? fb.w - 1 : x + w - 1, y1 = y + h > fb.h ? fb.h - 1 : y + h - 1;
    if (x0 > x1 || y0 > y1) return;
    lv_area_t r = { (lv_coord_t)x0, (lv_coord_t)y0, (lv_coord_t)x1, (lv_coord_t)y1 };

    // Fold into a rect it touches or nearly touches (a few px of slack keeps
    // neighbouring sprites together), else into whichever grows least when full
    const int SLACK = 4;
    for (int i = 0; i < fb.n_dirty; i++) {
        lv_area_t &d = fb.dirty[i];
        if (r.x1 <= d.x2 + SLACK && r.x2 >= d.x1 - SLACK && r.y1 <= d.y2 + SLACK && r.y2 >= d.y1 - SLACK) {
            d = union_of(d, r);
            return;
        }
    }
    if (fb.n_dirty < PIXFB_MAX_DIRTY) { fb.dirty[fb.n_dirty++] = r; return; }
    int best = 0, best_growth = INT32_MAX;
    for (int i = 0; i < fb.n_dirty; i++) {
        int growth = area_of(union_of(fb.dirty[i], r)) - area_of(fb.dirty[i]);
        if (growth < best_growth) { best_growth = growth; best = i; }
    }
    fb.dirty[best] = union_of(fb.dirty[best], r);
}

void pixfb_flush(PixFb &fb) {
    if (!fb.canvas) return;
    lv_area_t a;
    lv_obj_get_coords(fb.canvas, &a);   // invalidation wants screen coordinates
    for (int i = 0; i < fb.n_dirty; i++) {
        const lv_area_t &d = fb.dirty[i];
        lv_area_t r = { (lv_coord_t)(a.x1 + d.x1), (lv_coord_t)(a.y1 + d.y1),
                        (lv_coord_t)(a.x1 + d.x2), (lv_coord_t)(a.y1 + d.y2) };
        lv_obj_invalidate_area(fb.canvas, &r);
    }
    fb.n_dirty = 0;
}

void pixfb_px(PixFb &fb, int x, int y, lv_color_t c) {
    if (x < 0 || y < 0 || x >= fb.w || y >= fb.h) return;
    fb.buf[y * fb.w + x] = c;
}

void pixfb_fill(PixFb &fb, int x, int y, int w, int h, lv_color_t c) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > fb.w ? fb.w : x + w, y1 = y + h > fb.h ? fb.h : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++) {
        lv_color_t *row = fb.buf + yy * fb.w;
        for (int xx = x0; xx < x1; xx++) row[xx] = c;
    }
    pixfb_touch(fb, x0, y0, x1 - x0, y1 - y0);
}

void pixfb_line(PixFb &fb, int x0, int y0, int x1, int y1, lv_color_t c) {
    int minx = x0 < x1 ? x0 : x1, miny = y0 < y1 ? y0 : y1;
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int w = dx + 1, h = -dy + 1;
    int err = dx + dy;
    for (;;) {   // Bresenham
        pixfb_px(fb, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    pixfb_touch(fb, minx, miny, w, h);
}

void pixfb_sprite(PixFb &fb, int x, int y, const uint16_t *rows, int w, int h, lv_color_t c) {
    for (int r = 0; r < h; r++)
        for (int col = 0; col < w; col++)
            if (rows[r] & (1u << (w - 1 - col))) pixfb_px(fb, x + col, y + r, c);
    pixfb_touch(fb, x, y, w, h);
}

static void hline(PixFb &fb, int x0, int x1, int y, lv_color_t c) {
    if (y < 0 || y >= fb.h) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= fb.w) x1 = fb.w - 1;
    lv_color_t *row = fb.buf + y * fb.w;
    for (int x = x0; x <= x1; x++) row[x] = c;
}

void pixfb_fill_circle(PixFb &fb, int cx, int cy, int r, lv_color_t c) {
    if (r <= 0) { pixfb_px(fb, cx, cy, c); pixfb_touch(fb, cx, cy, 1, 1); return; }
    for (int dy = -r; dy <= r; dy++) {
        int dx = (int)sqrtf((float)(r * r - dy * dy) + 0.5f);
        hline(fb, cx - dx, cx + dx, cy + dy, c);
    }
    pixfb_touch(fb, cx - r, cy - r, 2 * r + 1, 2 * r + 1);
}

void pixfb_fill_tri(PixFb &fb, int x0, int y0, int x1, int y1, int x2, int y2, lv_color_t c) {
    // Sort by y, then scan between the long edge (0-2) and the two short ones
    if (y0 > y1) { int t; t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (y1 > y2) { int t; t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    if (y0 > y1) { int t; t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (y2 == y0) { hline(fb, LV_MIN(x0, LV_MIN(x1, x2)), LV_MAX(x0, LV_MAX(x1, x2)), y0, c); }
    else {
        int ys = y0 < 0 ? 0 : y0, ye = y2 >= fb.h ? fb.h - 1 : y2;
        for (int y = ys; y <= ye; y++) {
            float a = x0 + (float)(x2 - x0) * (y - y0) / (y2 - y0);
            float b = y < y1 ? (y1 == y0 ? x1 : x0 + (float)(x1 - x0) * (y - y0) / (y1 - y0))
                             : (y2 == y1 ? x1 : x1 + (float)(x2 - x1) * (y - y1) / (y2 - y1));
            hline(fb, (int)a, (int)b, y, c);
        }
    }
    int minx = LV_MIN(x0, LV_MIN(x1, x2)), maxx = LV_MAX(x0, LV_MAX(x1, x2));
    pixfb_touch(fb, minx, y0, maxx - minx + 1, y2 - y0 + 1);
}

bool pixfb_bg_save(PixFb &fb) {
    if (!fb.bg) fb.bg = (lv_color_t *)malloc(sizeof(lv_color_t) * fb.w * fb.h);
    if (!fb.bg) return false;
    memcpy(fb.bg, fb.buf, sizeof(lv_color_t) * fb.w * fb.h);
    return true;
}

void pixfb_bg_restore(PixFb &fb, int x, int y, int w, int h) {
    if (!fb.bg) return;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > fb.w ? fb.w : x + w, y1 = y + h > fb.h ? fb.h : y + h;
    if (x0 >= x1 || y0 >= y1) return;
    for (int yy = y0; yy < y1; yy++)
        memcpy(fb.buf + yy * fb.w + x0, fb.bg + yy * fb.w + x0, sizeof(lv_color_t) * (x1 - x0));
    pixfb_touch(fb, x0, y0, x1 - x0, y1 - y0);
}

static void blend_px(PixFb &fb, int x, int y, lv_color_t c, float t) {
    if (x < 0 || y < 0 || x >= fb.w || y >= fb.h) return;
    lv_color_t &p = fb.buf[y * fb.w + x];
    p = pixfb_mix(p, c, t);
}

void pixfb_blend_tri(PixFb &fb, int x0, int y0, int x1, int y1, int x2, int y2, lv_color_t c, float t) {
    if (y0 > y1) { int q; q = x0; x0 = x1; x1 = q; q = y0; y0 = y1; y1 = q; }
    if (y1 > y2) { int q; q = x1; x1 = x2; x2 = q; q = y1; y1 = y2; y2 = q; }
    if (y0 > y1) { int q; q = x0; x0 = x1; x1 = q; q = y0; y0 = y1; y1 = q; }
    if (y2 == y0) return;
    int ys = y0 < 0 ? 0 : y0, ye = y2 >= fb.h ? fb.h - 1 : y2;
    for (int y = ys; y <= ye; y++) {
        float a = x0 + (float)(x2 - x0) * (y - y0) / (y2 - y0);
        float b = y < y1 ? (y1 == y0 ? x1 : x0 + (float)(x1 - x0) * (y - y0) / (y1 - y0))
                         : (y2 == y1 ? x1 : x1 + (float)(x2 - x1) * (y - y1) / (y2 - y1));
        int l = (int)LV_MIN(a, b), r = (int)LV_MAX(a, b);
        for (int x = l; x <= r; x++) blend_px(fb, x, y, c, t);
    }
    int minx = LV_MIN(x0, LV_MIN(x1, x2)), maxx = LV_MAX(x0, LV_MAX(x1, x2));
    pixfb_touch(fb, minx, y0, maxx - minx + 1, y2 - y0 + 1);
}

void pixfb_glow(PixFb &fb, int cx, int cy, int r, lv_color_t c, float t) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++) {
            float d = sqrtf((float)(dx * dx + dy * dy)) / r;
            if (d < 1) blend_px(fb, cx + dx, cy + dy, c, t * (1 - d) * (1 - d));
        }
    pixfb_touch(fb, cx - r, cy - r, 2 * r + 1, 2 * r + 1);
}
