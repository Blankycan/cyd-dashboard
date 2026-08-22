# Custom fonts

`font_ui_12.c` / `font_ui_14.c` are Montserrat-Medium regenerated with
`lv_font_conv` to cover Basic Latin + Latin-1 Supplement (`0x20-0xFF`)
instead of LVGL's stock ASCII-only builds, so accented characters in
music titles/artists (é, ñ, Å, Ä, Ö, ü, ...) render correctly instead of
being dropped as missing glyphs.

To regenerate (needs Node/npx; the source TTF ships with the LVGL lib dep):

```bash
cd .pio/libdeps/esp32dev/lvgl/scripts/built_in_font
npx --yes lv_font_conv --no-compress --no-prefilter --bpp 4 --size 14 \
  --font Montserrat-Medium.ttf -r 0x20-0xFF \
  --format lvgl -o ../../../../../src/fonts/font_ui_14.c \
  --force-fast-kern-format

npx --yes lv_font_conv --no-compress --no-prefilter --bpp 4 --size 12 \
  --font Montserrat-Medium.ttf -r 0x20-0xFF \
  --format lvgl -o ../../../../../src/fonts/font_ui_12.c \
  --force-fast-kern-format
```

The generated files `#include "lvgl.h"` / `#include "lvgl/lvgl.h"` behind an
`LV_LVGL_H_INCLUDE_SIMPLE` guard by default — replace that with a plain
`#include <lvgl.h>` after regenerating, to match how the rest of this repo
includes LVGL.

To widen the covered alphabet (e.g. add Latin Extended-A for Polish/Czech/
Turkish), extend the `-r` range and regenerate — flash usage scales with
range size.
