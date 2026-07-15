#pragma once
// =============================================================================
// CYD Dashboard — color theme: Neon Rose (black / hot pink)
//
// Selected via CYD_THEME_NEON_ROSE in config.h (see theme.h for the dispatch).
// =============================================================================

// -----------------------------------------------------------------------------
// LEVEL 1 — RAW PALETTE  (24-bit RGB hex)
// -----------------------------------------------------------------------------

#define PAL_GLOW        0xFF007F  // hot rose          — sparks, active accents

// --- UI backgrounds ---
#define PAL_BG          0x16000F  // very dark rose    — main background
#define PAL_PANEL       0xFF3399  // hot pink          — topbar background
#define PAL_BORDER      0xBB0055  // bright cerise     — panel borders
#define PAL_DIVIDER     0x880044  // medium cerise     — horizontal dividers

// --- Text (main area: dark background, bright pink text) ---
#define PAL_TEXT_PRI    0xFF3399  // hot pink          — primary labels (clock, song title)
#define PAL_TEXT_SEC    0xFF88BB  // light pink        — secondary / subtitles (artist, etc.)
#define PAL_TEXT_DIM    0xFF6699  // bright rose       — timestamps, inactive text

// --- Status / progress ---
#define PAL_OK          0xFF1493  // deep pink         — idle, normal, connected
#define PAL_WARN        0xFFAA00  // golden amber      — usage >70%, mild alert
#define PAL_ALERT       0xFF2244  // vivid red-pink    — usage >90%, disconnect
#define PAL_BAR_TRACK   0x3D0025  // dark rose         — progress bar background

// -----------------------------------------------------------------------------
// LEVEL 1 — LVGL COLOR MACROS  (use these everywhere in firmware UI code)
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
// LEVEL 3 — Per-widget overrides specific to Neon Rose
// Topbar sits on the bright PAL_PANEL pink so needs dark text colours.
// -----------------------------------------------------------------------------

#define COL_TOPBAR_BG         LVC(PAL_PANEL)
#define COL_TOPBAR_CLOCK      LVC(0x1A0010)
#define COL_TOPBAR_DATE       LVC(0x550035)

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
