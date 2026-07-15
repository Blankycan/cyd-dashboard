#pragma once
// =============================================================================
// CYD Dashboard — color theme: Neon Rose (black / hot pink)
//
// Selected via CYD_THEME_NEON_ROSE in config.h (see theme.h for the dispatch).
// =============================================================================

// -----------------------------------------------------------------------------
// RAW PALETTE  (24-bit RGB hex)
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

// Topbar text sits on the bright PAL_PANEL background, so it needs dark colours.
#define COL_TOPBAR_TEXT_PRI LVC(0x1A0010)
#define COL_TOPBAR_TEXT_DIM LVC(0x550035)

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
