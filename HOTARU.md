# Hotaru (蛍) — Character Design

Hotaru means "firefly" in Japanese. She is the resident spirit of the CYD Dashboard —
a small mushroom yokai who lives on the display, reacts to typing activity, and sleeps
when the computer is idle. Her aesthetic draws from LOFI, cozy Japanese folklore, and
solarpunk: warm earthy tones, soft glows, sage greens.

---

## Color Palette

All colors live in `src/theme.h` as `PAL_*` constants. Edit them there to retheme
the entire project. The palette below is for human reference.

| Role                   | Name          | Hex       | Swatch |
|------------------------|---------------|-----------|--------|
| Mushroom cap           | PAL_CAP       | `#E07A5F` | warm terracotta |
| Cap highlight / rim    | PAL_CAP_SHINE | `#F2CC8F` | sun amber       |
| Cap underside spots    | PAL_SPOTS     | `#FEFAE0` | warm cream      |
| Face / body            | PAL_FACE      | `#EDF6F9` | soft off-white  |
| Eyes                   | PAL_EYES      | `#264653` | deep teal       |
| Stem / lower body      | PAL_STEM      | `#81B29A` | sage green      |
| Glow / sparks / pulse  | PAL_GLOW      | `#E9C46A` | warm gold       |
| Main background        | PAL_BG        | `#0F1F1A` | dark forest     |
| Panel / card bg        | PAL_PANEL     | `#162820` | dark forest+    |
| Borders                | PAL_BORDER    | `#264653` | deep teal       |
| Dividers               | PAL_DIVIDER   | `#1E3A30` | muted forest    |
| Primary text           | PAL_TEXT_PRI  | `#FEFAE0` | warm cream      |
| Secondary text         | PAL_TEXT_SEC  | `#81B29A` | sage green      |
| Dim / inactive text    | PAL_TEXT_DIM  | `#4A7B6F` | muted sage      |
| Normal status          | PAL_OK        | `#81B29A` | sage green      |
| Warning (>70%)         | PAL_WARN      | `#F2CC8F` | amber           |
| Alert (>90%)           | PAL_ALERT     | `#E07A5F` | terracotta      |
| Progress bar track     | PAL_BAR_TRACK | `#1E3A30` | dark track      |

---

## Visual Design

```
      ╭─────────╮          ← mushroom cap (PAL_CAP, terracotta)
     ╱  ·     ·  ╲         ← cap spots (PAL_SPOTS, cream)
    │  ╭───────╮  │        ← cap rim (PAL_CAP_SHINE, amber)
    ╰──╯       ╰──╯
      ╭─────────╮          ← face / body (PAL_FACE, off-white)
      │  ◉   ◉  │          ← eyes (PAL_EYES, deep teal)
      │    ᵕ    │          ← gentle curve smile
      ╰─────────╯
         ╭───╮             ← stem (PAL_STEM, sage green)
         ╰───╯
```

Sprite target: 48×96 px at 1× scale (displayed at 1× in a 96px-wide sidebar column).
Drawn as a series of ~8 LVGL canvas frames (see Phase 4 implementation).

---

## Animation States

| State          | Trigger                          | What happens |
|----------------|----------------------------------|--------------|
| **Idle**       | No typing for >2 s               | Slow vertical bob (±3 px, 2 s cycle). Blinks every 4–6 s. Cap tilts ±2° on blink. |
| **Typing slow**| WPM 1–30                         | Faster bob (1 s cycle). Tiny gold sparkle dots appear beside her, fade over 0.5 s. |
| **Typing fast**| WPM 30+                          | Quick bounce (0.4 s cycle). Gold sparks scatter outward. Glow halo brightens. |
| **Sleep onset**| No keyboard activity for N min   | Over 3 s: cap gradually droops to one side. Eyes half-close (drawn with shorter oval). |
| **Sleeping**   | Sleep onset complete             | Cap fully drooped. Single small "z" glyph floats upward on a 2 s loop. Overall opacity dims to ~60%. Glow dims to 20%. |
| **Waking**     | Any keystroke while sleeping     | Cap snaps upright in one frame. Eyes open wide (2-frame flash). Returns to Idle. |

---

## Glow Effect

Hotaru's signature is a soft warm-gold aura (PAL_GLOW, `#E9C46A`).
- Implemented as a shadow on the mushroom cap object (`lv_obj_set_style_shadow_*`).
- During sleep: shadow opacity fades from 40% → 8%.
- During fast typing: shadow width expands from 14 → 22 px, opacity peaks at 70%.
- The sidebar background (PAL_BG column) pulses slightly during fast typing:
  background opacity cycles from 100% → 85% → 100% over 0.4 s.

---

## Phase Roadmap for Hotaru

- **Phase 1** (current): LVGL primitive placeholder — cap, body, eyes drawn with
  `lv_obj_create` shapes. Verifies theme colors on real hardware.
- **Phase 4**: Replace placeholder with proper C-array sprite frames.
  Implement state machine: IDLE → TYPING_SLOW → TYPING_FAST → SLEEP_ONSET → SLEEPING → WAKING.
- **Future**: Add ear-twitch variation, seasonal cap color (autumn/spring swap via config).
