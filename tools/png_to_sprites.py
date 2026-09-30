#!/usr/bin/env python3
"""
Convert the lofi scene's PNG layers into the firmware art header.

  python tools/png_to_sprites.py            # assets/lofi/*.png -> src/scenes/lofi_art.h

Every layer is a full 240x90 PNG, transparent except for its part, so
positions come straight from where things are drawn:

  girl_body/head/fist/hand   the girl (head nods, hand writes)
  prop_lamp, prop_mug        the lamp and mug, painted into the backdrop
  prop_lamp_lit              only drawn while the lamp is on (bulb, glow, shine)
  prop_book                  the notebook she writes in
  book_ink, book_ink2        her handwriting on the first and second page, as it
                             looks when full; revealed one pixel per typed character
                             (see ink_order())
  light                      greyscale: how strongly the lamp lights each pixel
                             (black = not at all, white = brightest)

Colour layers must use assets/lofi/lofi_girl.gpl (load it as the palette in
Aseprite); anything else is reported with its pixel position rather than
guessed. The light layer can use any grey. Saved as RGB, RGBA, or indexed
PNG — all work. Pure Python, no dependencies.
"""
import struct
import sys
import zlib
from pathlib import Path

ROOT    = Path(__file__).resolve().parent.parent
ASSETS  = ROOT / "assets" / "lofi"
OUT     = ROOT / "src" / "scenes" / "lofi_art.h"
LAYERS  = ["girl_body", "girl_head", "girl_fist", "girl_hand", "prop_lamp", "prop_lamp_lit", "prop_mug",
           "prop_book", "book_ink", "book_ink2"]
INK     = ["book_ink", "book_ink2"]   # pages, in the order she fills them
# Every line of handwriting takes this many typed characters. Its pixels fill
# the first slots (after the page's lead-in); the rest are her writing on out
# of view, so pages last longer than their few visible pixels.
INK_SLOTS_PER_LINE = 5
INK_LEAD = {"book_ink": 0, "book_ink2": 1}   # empty slots before each line's pixels
EMPTY = 255                                  # an out-of-view slot in INK_ORDER
MASKS   = ["light"]
SCENE_W, SCENE_H = 240, 90


def read_palette(path: Path) -> dict:
    """GIMP .gpl palette: `R G B  <char> name` per line -> {(r, g, b): char}."""
    pal = {}
    for line in path.read_text().splitlines():
        parts = line.split()
        if len(parts) >= 4 and all(p.isdigit() for p in parts[:3]):
            pal[tuple(int(p) for p in parts[:3])] = parts[3]
    return pal


def read_png(path: Path):
    """Decode an 8-bit, non-interlaced PNG to rows of (r, g, b, a)."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit(f"{path}: not a PNG")
    pos, idat, plte, trns = 8, b"", None, b""
    while pos < len(data):
        n = struct.unpack(">I", data[pos:pos + 4])[0]
        kind, body = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + n]
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
        elif kind == b"PLTE":
            plte = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif kind == b"tRNS":
            trns = body
        elif kind == b"IDAT":
            idat += body
        pos += 12 + n
    if depth != 8 or interlace:
        sys.exit(f"{path}: save as 8-bit, non-interlaced PNG")
    bpp = {2: 3, 3: 1, 6: 4}.get(ctype)
    if bpp is None:
        sys.exit(f"{path}: unsupported PNG colour type {ctype} (use RGB, RGBA or indexed)")
    raw, stride = zlib.decompress(idat), w * bpp
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):   # undo the PNG row filter
            a = line[i - bpp] if i >= bpp else 0
            b, c = prev[i], prev[i - bpp] if i >= bpp else 0
            if f == 1:   line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        prev = line
        if ctype == 2:   rows.append([(*line[i:i + 3], 255) for i in range(0, stride, 3)])
        elif ctype == 6: rows.append([tuple(line[i:i + 4]) for i in range(0, stride, 4)])
        else:            rows.append([(*plte[i], trns[i] if i < len(trns) else 255) for i in line])
    return w, h, rows


def ink_lines(pixels):
    """Split one page's ink into lines, in writing order, each line's pixels in
    the order she writes them.

    Her strokes run down-right, so pixels belong to the same stroke when they
    touch straight down or down-right of each other (not up-right: on a page
    of zigzag strokes that would chain them all into one). Strokes are written
    from our left to our right, each from its lower end up."""
    pixels = set(pixels)
    parent = {p: p for p in pixels}
    def find(p):
        while parent[p] != p:
            parent[p] = parent[parent[p]]; p = parent[p]
        return p
    for (x, y) in pixels:
        for q in ((x, y + 1), (x + 1, y + 1)):
            if q in pixels:
                parent[find(q)] = find((x, y))
    strokes = {}
    for p in pixels:
        strokes.setdefault(find(p), []).append(p)
    return [sorted(stroke, key=lambda p: (-p[1], -p[0]))   # lower end first
            for stroke in sorted(strokes.values(), key=lambda s: (min(p[0] for p in s), min(p[1] for p in s)))]


def ink_slots(page, pixels):
    """One entry per typed character: the pixel it reveals, or None."""
    slots = []
    for line in ink_lines(pixels):
        lead = INK_LEAD.get(page, 0)
        n = max(INK_SLOTS_PER_LINE, lead + len(line))   # a long line never loses ink
        slots += [None] * lead + line + [None] * (n - lead - len(line))
    return slots


def main():
    pal = read_palette(ASSETS / "lofi_girl.gpl")
    out = ["// Generated by tools/png_to_sprites.py from assets/lofi/*.png — edit the PNGs",
           "// (in Aseprite, with assets/lofi/lofi_girl.gpl as the palette), not this file.",
           "// Each layer: its bounding box within a 240x90 scene, one char per pixel",
           "// (see lofi_girl.gpl for the characters; '.' is transparent). The light",
           "// layer is a greyscale strength per pixel, 0-255, row by row.",
           "#pragma once", "#include <stdint.h>", ""]
    bad = []
    inks = {}
    for name in LAYERS + MASKS:
        path = ASSETS / f"{name}.png"
        w, h, rows = read_png(path)
        if (w, h) != (SCENE_W, SCENE_H):
            sys.exit(f"{path}: must be {SCENE_W}x{SCENE_H} (the scene), is {w}x{h}")
        grid = {}
        for y, row in enumerate(rows):
            for x, (r, g, b, a) in enumerate(row):
                if name in MASKS:   # greyscale strength, 0-255
                    v = round((r * 299 + g * 587 + b * 114) / 1000 * a / 255)
                    if v:
                        grid[(x, y)] = v
                    continue
                if a < 128:
                    continue
                ch = pal.get((r, g, b))
                if ch is None:
                    bad.append(f"  {path.name} ({x}, {y}): #{r:02X}{g:02X}{b:02X}")
                    continue
                grid[(x, y)] = ch
        if not grid:
            sys.exit(f"{path}: layer is empty")
        xs = [p[0] for p in grid]; ys = [p[1] for p in grid]
        x0, y0, x1, y1 = min(xs), min(ys), max(xs), max(ys)
        up = name.upper()
        out.append(f"// {name}: {x1 - x0 + 1}x{y1 - y0 + 1} at ({x0}, {y0})")
        out.append(f"static const int {up}_X = {x0}, {up}_Y = {y0}, {up}_W = {x1 - x0 + 1}, {up}_H = {y1 - y0 + 1};")
        if name in MASKS:
            out.append(f"static const uint8_t {up}[] = {{")
            out += ["    " + ",".join(f"{grid.get((x, y), 0):3d}" for x in range(x0, x1 + 1)) + ","
                    for y in range(y0, y1 + 1)]
        else:
            out.append(f"static const char *const {up}[] = {{")
            out += [f'    "{"".join(grid.get((x, y), ".") for x in range(x0, x1 + 1))}",' for y in range(y0, y1 + 1)]
        out += ["};", ""]
        if name in INK:
            inks[name] = list(grid)
    order = [p for name in INK for p in ink_slots(name, inks.get(name, []))]
    out.append("// Her handwriting across both pages, one entry per typed character: the")
    out.append(f"// {{x, y}} it reveals, or {{{EMPTY}, {EMPTY}}} while she writes out of view")
    out.append(f"static const int INK_N = {len(order)};")
    out.append(f"static const uint8_t INK_EMPTY = {EMPTY};")
    out.append("static const uint8_t INK_ORDER[][2] = {")
    out += [f"    {{{p[0]}, {p[1]}}}," if p else f"    {{{EMPTY}, {EMPTY}}}," for p in order]
    out += ["};", ""]
    if bad:
        print("Colours not in lofi_girl.gpl (use the palette's exact colours):")
        print("\n".join(bad[:30]) + (f"\n  ... and {len(bad) - 30} more" if len(bad) > 30 else ""))
        sys.exit(1)
    OUT.write_text("\n".join(out))
    print(f"Wrote {OUT.relative_to(ROOT) if OUT.is_relative_to(ROOT) else OUT}")


if __name__ == "__main__":
    main()
