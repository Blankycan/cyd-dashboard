#pragma once
#include <lvgl.h>

// Montserrat-Medium at 12/14px, regenerated with Basic Latin + Latin-1
// Supplement (0x20-0xFF) instead of LVGL's stock ASCII-only build, so
// music titles/artists with accented characters (é, ñ, Å, Ä, Ö, ü, ...)
// render instead of showing blank glyphs. See src/fonts/README.md to
// regenerate.
extern const lv_font_t font_ui_12;
extern const lv_font_t font_ui_14;
