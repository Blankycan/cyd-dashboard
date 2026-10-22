#pragma once
// =============================================================================
// CYD Dashboard — color theme: Scarlet Witch (crimson and black Halloween,
// matches the Omarchy "scarlet-witch" desktop theme)
//
// Selected via CYD_THEME_SCARLET_WITCH in config.h (see theme.h for the dispatch).
//
// Ported from ~/.config/omarchy/themes/scarlet-witch/colors.toml, then re-tuned
// after seeing it on real hardware. The first pass added a little blue to the
// scarlets to keep them from drifting orange, and this panel turned every one
// of them purple, maroon or hot pink — the near-black backgrounds went purple
// too. So: no blue in any colour, darks get a little green (warmth) to beat
// the panel's cool backlight, and reds are kept red by holding green low
// rather than adding blue. A second pass read too yellow/gold overall (the
// panel's warm drift again), so reds now have ~no green either, and only the
// primary labels stay gold — secondary and dim text are reds.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// RAW PALETTE  (24-bit RGB hex)
// -----------------------------------------------------------------------------

#define PAL_GLOW        0xFF0800  // pure scarlet     — active accents (desktop accent #e8302a)

// --- UI backgrounds ---
#define PAL_BG          0x140300  // near-black red   — main background (desktop #050505)
#define PAL_PANEL       0x420600  // deep blood red   — panel / card background
#define PAL_BORDER      0xFF0A00  // scarlet          — panel borders
#define PAL_DIVIDER     0x800800  // dark scarlet     — horizontal dividers

// --- Text ---
#define PAL_TEXT_PRI    0xFFA838  // red-gold         — primary labels (clock, titles) — the only gold; redder than looks right on a normal screen
#define PAL_TEXT_SEC    0xFF3A18  // scarlet-red      — secondary / subtitles
#define PAL_TEXT_DIM    0xD03418  // muted red        — timestamps, inactive text (brightened from 0xB02810 for legibility)

// --- Status / progress ---
#define PAL_OK          0xFF0A00  // scarlet          — idle, normal, connected
#define PAL_WARN        0xFF7000  // vivid orange     — usage >70%, mild alert
#define PAL_ALERT       0xFFE000  // bright yellow    — usage >90%, disconnect (must stand apart from the scarlet OK)
#define PAL_BAR_TRACK   0x240300  // near-black red   — progress bar background

// -----------------------------------------------------------------------------
// LVGL COLOR MACROS  (use these everywhere in firmware UI code)
// lv_color_hex() accepts 0xRRGGBB and handles RGB565 conversion internally.
// -----------------------------------------------------------------------------
#include <lvgl.h>

#define LVC(hex)            lv_color_hex(hex)

#define COL_BG              LVC(PAL_BG)
#define COL_PANEL           LVC(PAL_PANEL)
#define COL_BORDER          LVC(PAL_BORDER)
#define COL_DIVIDER         LVC(PAL_DIVIDER)

#define COL_TEXT_PRI        LVC(PAL_TEXT_PRI)
#define COL_TEXT_SEC        LVC(PAL_TEXT_SEC)
#define COL_TEXT_DIM        LVC(PAL_TEXT_DIM)

#define COL_OK              LVC(PAL_OK)
#define COL_WARN            LVC(PAL_WARN)
#define COL_ALERT           LVC(PAL_ALERT)
#define COL_BAR_TRACK       LVC(PAL_BAR_TRACK)

#define COL_GLOW            LVC(PAL_GLOW)

// -----------------------------------------------------------------------------
// LEVEL 2 — Per-widget semantic tokens
// Derived from palette; override per-widget appearance without touching PAL_*.
// -----------------------------------------------------------------------------

#define COL_TITLE           LVC(PAL_TEXT_PRI)
#define COL_DIVIDER_INNER   LVC(PAL_DIVIDER)
#define COL_BAR_BG          LVC(PAL_BAR_TRACK)
#define COL_BAR_FILL        LVC(PAL_OK)
#define COL_BAR_WARN        LVC(PAL_WARN)
#define COL_BAR_ALERT       LVC(PAL_ALERT)
#define COL_BAR_INACTIVE    LVC(PAL_TEXT_DIM)
#define COL_BAR_LABEL       LVC(PAL_TEXT_DIM)
#define COL_BAR_VALUE       LVC(PAL_OK)
#define COL_BAR_EXTRA       LVC(PAL_TEXT_DIM)

// -----------------------------------------------------------------------------
// LEVEL 3 — Per-widget overrides specific to Scarlet Witch
// Topbar sits on the oxblood panel rather than the bare black, echoing the
// desktop's red-tinted windows. The scarlet accent marks the "active" dot and
// the now-playing icon; the clock stays candle gold like the desktop's text.
// -----------------------------------------------------------------------------

#define COL_TOPBAR_BG         LVC(PAL_PANEL)
#define COL_TOPBAR_CLOCK      LVC(PAL_TEXT_PRI)
#define COL_TOPBAR_DATE       LVC(PAL_TEXT_SEC)

#define COL_DOT_ACTIVE        LVC(PAL_GLOW)

#define COL_MUSIC_ICON_PLAY   LVC(PAL_GLOW)

// -----------------------------------------------------------------------------
// RGB565 MACROS  (use these for any direct TFT_eSPI drawing outside LVGL)
// -----------------------------------------------------------------------------
#define _R5(hex)  (((hex) >> 19) & 0x1F)
#define _G6(hex)  (((hex) >> 10) & 0x3F)
#define _B5(hex)  (((hex) >>  3) & 0x1F)
#define PAL565(hex) ((_R5(hex) << 11) | (_G6(hex) << 5) | _B5(hex))

#define TFT_COL_BG          PAL565(PAL_BG)
#define TFT_COL_PANEL       PAL565(PAL_PANEL)
#define TFT_COL_TEXT_PRI    PAL565(PAL_TEXT_PRI)
#define TFT_COL_TEXT_SEC    PAL565(PAL_TEXT_SEC)
#define TFT_COL_OK          PAL565(PAL_OK)
#define TFT_COL_WARN        PAL565(PAL_WARN)
#define TFT_COL_ALERT       PAL565(PAL_ALERT)
#define TFT_COL_GLOW        PAL565(PAL_GLOW)
