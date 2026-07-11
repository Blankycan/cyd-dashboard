#pragma once
// =============================================================================
// HOTARU THEME  —  CYD Dashboard color palette
// =============================================================================
// All colors used throughout the firmware are defined here.
// To swap the whole palette, update the PAL_* hex values below.
// Nothing else in the codebase should contain raw color literals.
//
// Aesthetic: LOFI / cozy Japanese / solarpunk
// Inspired by forest spirits, paper lanterns, fireflies at dusk.
// =============================================================================

// -----------------------------------------------------------------------------
// RAW PALETTE  (24-bit RGB hex — edit these to retheme the entire project)
// -----------------------------------------------------------------------------

// --- Character "Hotaru" ---
#define PAL_CAP         0xE07A5F  // warm terracotta  — mushroom cap
#define PAL_CAP_SHINE   0xF2CC8F  // sun amber        — cap highlight / rim
#define PAL_SPOTS       0xFEFAE0  // warm cream       — cap underside spots
#define PAL_FACE        0xEDF6F9  // soft off-white   — face and body
#define PAL_EYES        0x264653  // deep teal        — eyes (same as border)
#define PAL_STEM        0x81B29A  // sage green       — stem / body
#define PAL_GLOW        0xE9C46A  // warm gold        — glow, sparks, active pulse

// --- UI backgrounds ---
#define PAL_BG          0x0F1F1A  // very dark forest — main background
#define PAL_PANEL       0x162820  // dark forest      — panel / card background
#define PAL_BORDER      0x264653  // deep teal        — panel borders
#define PAL_DIVIDER     0x1E3A30  // muted forest     — horizontal dividers

// --- Text ---
#define PAL_TEXT_PRI    0xFEFAE0  // warm cream       — primary labels
#define PAL_TEXT_SEC    0x81B29A  // sage green       — secondary / subtitles
#define PAL_TEXT_DIM    0x4A7B6F  // muted sage       — timestamps, inactive text

// --- Status / progress ---
#define PAL_OK          0x81B29A  // sage green       — idle, normal, connected
#define PAL_WARN        0xF2CC8F  // amber            — usage >70%, mild alert
#define PAL_ALERT       0xE07A5F  // terracotta       — usage >90%, disconnect
#define PAL_BAR_TRACK   0x1E3A30  // dark track       — progress bar background

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

#define COL_CAP             LVC(PAL_CAP)
#define COL_CAP_SHINE       LVC(PAL_CAP_SHINE)
#define COL_SPOTS           LVC(PAL_SPOTS)
#define COL_FACE            LVC(PAL_FACE)
#define COL_EYES            LVC(PAL_EYES)
#define COL_STEM            LVC(PAL_STEM)
#define COL_GLOW            LVC(PAL_GLOW)

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
#define TFT_COL_CAP         PAL565(PAL_CAP)
#define TFT_COL_GLOW        PAL565(PAL_GLOW)
