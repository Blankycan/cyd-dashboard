#pragma once
// =============================================================================
// CYD Dashboard — color theme: Cozyfall (autumn lofi, matches the Omarchy
// "cozyfall" desktop theme)
//
// Selected via CYD_THEME_COZYFALL in config.h (see theme.h for the dispatch).
//
// Ported from ~/.config/omarchy/themes/cozyfall/colors.toml, then re-tuned
// twice after seeing it on real hardware. This panel does two unflattering
// things to this palette: pale/low-saturation tones (Omarchy's near-white
// text, its sage-green "OK") shift visibly blue, and saturated warm tones
// drift yellow/brown instead of reading as orange. So: no near-white or
// pastel colours anywhere (bump saturation instead of lightening toward
// white), and the warm hues are pushed redder than they'd need to be on a
// normal screen, to compensate for the yellow drift. Still expect to tweak
// further — screen response varies board to board.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// RAW PALETTE  (24-bit RGB hex)
// -----------------------------------------------------------------------------

#define PAL_GLOW        0xF2560A  // vivid red-orange — sparks, active accents

// --- UI backgrounds ---
#define PAL_BG          0x220E05  // dark espresso    — main background
#define PAL_PANEL       0x3D1C08  // warm espresso    — panel / card background
#define PAL_BORDER      0xD65C14  // vivid burnt orange — panel borders
#define PAL_DIVIDER     0x6E2E0A  // warm burnt orange — horizontal dividers

// --- Text ---
#define PAL_TEXT_PRI    0xFFC878  // bright orange-gold — primary labels (clock, titles) — saturated, NOT a pale cream/near-white
#define PAL_TEXT_SEC    0xE87A28  // vivid caramel    — secondary / subtitles
#define PAL_TEXT_DIM    0xB07A3E  // muted orange-tan — timestamps, inactive text

// --- Status / progress ---
#define PAL_OK          0xE0651A  // burnt orange     — idle, normal, connected
#define PAL_WARN        0xF5B82E  // vivid golden amber — usage >70%, mild alert
#define PAL_ALERT       0xE23A1E  // vivid red        — usage >90%, disconnect
#define PAL_BAR_TRACK   0x180B04  // near-black brown — progress bar background

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
// LEVEL 3 — Per-widget overrides specific to Cozyfall
// Topbar sits on the lighter panel brown rather than the bare background, like
// a mug on a table — a subtle lift rather than a full colour inversion. The
// pumpkin accent (the theme's defining colour) shows up on the "active" dot,
// the now-playing icon, and the Claude token count/bar, instead of the sage
// "OK" green being reused everywhere.
// -----------------------------------------------------------------------------

#define COL_TOPBAR_BG         LVC(PAL_PANEL)
#define COL_TOPBAR_CLOCK      LVC(PAL_TEXT_PRI)
#define COL_TOPBAR_DATE       LVC(PAL_TEXT_SEC)

#define COL_DOT_ACTIVE        LVC(PAL_GLOW)

#define COL_MUSIC_ICON_PLAY   LVC(PAL_GLOW)

#define COL_CLAUDE_TOKENS_OUT   LVC(PAL_GLOW)
#define COL_CLAUDE_TOK_BAR_FILL LVC(PAL_GLOW)

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
