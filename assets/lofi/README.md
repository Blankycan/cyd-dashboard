# Lofi scene artwork

Source art for the lofi scene in `src/scenes/lofi.cpp`: the girl, the lamp,
the mug, and the lamp's light. The PNGs are the source of truth:
`src/scenes/lofi_art.h` is generated from them.

| File | What it is |
|---|---|
| `girl_body.png` | Sweater, scarf, and both arms (the chin arm and the writing arm) |
| `girl_head.png` | Hair, face, and headphones — nods to the music as one piece |
| `girl_fist.png` | The hand under her chin, drawn over the head |
| `girl_hand.png` | The writing hand and pen — moves while you type |
| `prop_lamp.png` | The articulated lamp, as a silhouette |
| `prop_lamp_lit.png` | Only drawn while the lamp is on: the bulb, the glowing rim, the shine on the mug |
| `prop_mug.png` | The mug |
| `prop_book.png` | The notebook she writes in |
| `book_ink.png` | Her handwriting **as it looks on a full page**, in the ink colour. It's revealed one pixel per character you type, the way she writes on a page whose top faces our left: from our left to our right, each line from its lower end up. When the page is full she turns it and starts again. Put the lines wherever you like; leave the rest transparent |
| `light.png` | **Greyscale**: how strongly the lamp lights each pixel — black is unlit, white is the brightest. Covers the glow on the wall, the beam, and the pool on the desk |
| `lofi_girl.gpl` | The palette for every layer except `light.png`. Each colour maps to one character in the art header |
| `scene_backdrop.png` | The empty room (no girl, lamp, mug, book, or light), as a reference layer to draw over — not used by the firmware |

Every layer is a full **240×90** canvas, transparent except for its part, so
where you draw something is exactly where it appears in the scene.

How the light works: the firmware tints everything under `light.png` toward
the lamp's colour — any grey works, and all 256 levels are kept, so soft
gradients stay smooth. The mug, the book, and the ink are lit by the same layer (a pixel under
bright light gets warmer), so repainting the light relights them too. Tapping
the lamp turns off the light, `prop_lamp_lit.png`, and that warmth.

## Editing in Aseprite

1. Open `scene_backdrop.png`, then add the other PNGs as layers on top
   (File → Import, or drag them in), keeping them at 0,0. A good order,
   bottom to top: `light`, `prop_book`, `book_ink`, `prop_mug`, `prop_lamp`,
   `prop_lamp_lit`, then the girl layers. Set `light`'s blend mode to *Screen* or *Add* to preview it.
2. Load `lofi_girl.gpl` as the palette and use only its colours on the colour
   layers (the outline colour is the first swatch). `light.png` takes any grey.
3. Export each layer back to its own PNG (RGB, RGBA, or indexed PNG all work;
   transparent = empty).
4. Regenerate the sprite and flash:

   ```bash
   python tools/png_to_sprites.py
   ~/.platformio/penv/bin/pio run --target upload
   ```

The converter rejects any colour that isn't in the palette and lists the
pixels, so an anti-aliased brush or a colour picked off the backdrop is caught
before it reaches the board.

The palette colours are the character's *natural* colours. In the firmware each
one is a `COL_SCENE_LOFI_*` theme token that by default blends it about 20%
toward the theme's background, so she sits in the room's lighting.
