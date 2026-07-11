#pragma once
// =============================================================================
// CYD Dashboard — color palette
// =============================================================================

// -----------------------------------------------------------------------------
// RAW PALETTE  (24-bit RGB hex)
// -----------------------------------------------------------------------------

#define PAL_GLOW        0xE9C46A  // warm gold — sparks, active accents

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
#define TFT_COL_GLOW        PAL565(PAL_GLOW)
