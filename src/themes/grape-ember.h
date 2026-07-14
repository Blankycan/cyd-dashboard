#pragma once
// =============================================================================
// CYD Dashboard — color theme: Grape Ember (work / purple 3D-printed case)
//
// Selected via CYD_THEME_GRAPE_EMBER in config.h (see theme.h for the dispatch).
// =============================================================================

// -----------------------------------------------------------------------------
// RAW PALETTE  (24-bit RGB hex)
// -----------------------------------------------------------------------------

#define PAL_GLOW        0xFF8A3D  // vivid orange     — sparks, active accents

// --- UI backgrounds ---
#define PAL_BG          0x270C24  // deep plum        — main background
#define PAL_PANEL       0x3F173B  // dark magenta-vio — panel / card background
#define PAL_BORDER      0x922A8A  // vivid magenta    — panel borders
#define PAL_DIVIDER     0x922A8A  // vivid magenta    — horizontal dividers

// --- Text ---
// Primary text is a fairly saturated pink, not a pale near-white — pastel
// colors are what was reading blue-shifted on this panel at an angle, so it
// needs enough saturation for the true hue to hold up. Only used for the
// clock and the now-playing title.
#define PAL_TEXT_PRI    0xEC8DB8  // rose pink        — primary labels (clock, song title)
#define PAL_TEXT_SEC    0xC29AD6  // soft lavender    — secondary / subtitles (artist, etc.)
#define PAL_TEXT_DIM    0xAA5098  // muted mauve      — timestamps, inactive text

// --- Status / progress ---
// This is a two-colour purple + orange theme — "OK" is a bright violet, not
// green, so status colours stay in the brand palette instead of borrowing
// the semantic green from the original forest theme.
#define PAL_OK          0xE05CE0  // vivid violet     — idle, normal, connected
#define PAL_WARN        0xFFB829  // golden amber     — usage >70%, mild alert
#define PAL_ALERT       0xFF4D6D  // vivid coral-red  — usage >90%, disconnect
#define PAL_BAR_TRACK   0x30132D  // dark track       — progress bar background

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
