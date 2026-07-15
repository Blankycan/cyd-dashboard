#pragma once
// =============================================================================
// CYD Dashboard — color theme: Rainbow (diagnostic / every token unique)
//
// Every colour token is intentionally different so any fallback that silently
// fires shows the wrong hue immediately. Widget families each own a hue band:
//   Topbar   indigo bg / yellow clock / cyan date
//   Music    dark-green bg / lime text / orange|yellow|sky icons
//   System   dark-violet bg; CPU=reds, RAM=greens, WPM=blues
//   Claude   dark-teal bg / gold tokens; H5=violets, W7=teals
//   Status   dark-gold bg / lime active / sky-blue idle / red offline
//
// Selected via CYD_THEME_RAINBOW in config.h (see theme.h for the dispatch).
// =============================================================================

// -----------------------------------------------------------------------------
// LEVEL 1 — RAW PALETTE  (24-bit RGB hex)
// -----------------------------------------------------------------------------

// Neutrals / structure
#define PAL_BLACK       0x080810  // near-black indigo  — default background
#define PAL_GLOW        0xFFDD00  // gold               — generic accent / glow

// Generic text & structure (intentionally distinct from every widget override)
#define PAL_BG          0x080810  // near-black indigo
#define PAL_PANEL       0x2200AA  // deep indigo        — topbar panel
#define PAL_BORDER      0xFF00FF  // magenta            — generic borders
#define PAL_DIVIDER     0x8800FF  // violet             — between-widget rules

#define PAL_TEXT_PRI    0x00FFFF  // cyan               — generic primary text
#define PAL_TEXT_SEC    0x00FF88  // mint               — generic secondary text
#define PAL_TEXT_DIM    0xFF8800  // orange             — generic dim text

#define PAL_OK          0x00DD44  // green              — generic ok/connected
#define PAL_WARN        0xFFDD00  // gold               — generic warn
#define PAL_ALERT       0xFF2222  // red                — generic alert
#define PAL_BAR_TRACK   0x111122  // dark indigo        — generic bar track

// -----------------------------------------------------------------------------
// LEVEL 1 — LVGL COLOR MACROS
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
// LEVEL 2 — Generic semantic tokens (distinct from every Level 3 override)
// -----------------------------------------------------------------------------

#define COL_TITLE           LVC(0xFFFF00)  // yellow       — generic title
#define COL_DIVIDER_INNER   LVC(0xFF00AA)  // hot pink     — within-widget rules

#define COL_BAR_BG          LVC(0x111122)  // dark indigo  — generic bar bg
#define COL_BAR_FILL        LVC(0x00CCFF)  // sky blue     — generic bar fill
#define COL_BAR_WARN        LVC(0xFFAA00)  // amber        — generic bar warn
#define COL_BAR_ALERT       LVC(0xFF3300)  // orange-red   — generic bar alert
#define COL_BAR_INACTIVE    LVC(0x333366)  // slate        — generic inactive
#define COL_BAR_LABEL       LVC(0xFF8800)  // orange       — generic key label
#define COL_BAR_VALUE       LVC(0x00CCFF)  // sky blue     — generic value text
#define COL_BAR_EXTRA       LVC(0xFF44AA)  // pink         — generic extra info

// -----------------------------------------------------------------------------
// LEVEL 3 — Per-widget overrides (every token explicitly set for diagnostics)
// -----------------------------------------------------------------------------

// Topbar — indigo/yellow/cyan
#define COL_TOPBAR_BG         LVC(0x2200AA)  // deep indigo
#define COL_TOPBAR_CLOCK      LVC(0xFFFF00)  // yellow
#define COL_TOPBAR_DATE       LVC(0x00FFFF)  // cyan

// Music — green family for bg/text, icons spread across orange/yellow/sky
#define COL_MUSIC_BG          LVC(0x001A00)  // very dark green
#define COL_MUSIC_TITLE       LVC(0x00FF44)  // lime
#define COL_MUSIC_TEXT        LVC(0x44FF88)  // mint
#define COL_MUSIC_ICON_PLAY   LVC(0xFF8800)  // orange       (note icon)
#define COL_MUSIC_ICON_PAUSE  LVC(0xFFFF00)  // yellow       (pause bars)
#define COL_MUSIC_ICON_IDLE   LVC(0x00CCFF)  // sky blue     (grass blades)
#define COL_MUSIC_DOT_PLAYING LVC(0xFF4400)  // red-orange
#define COL_MUSIC_DOT_IDLE    LVC(0x0044FF)  // bright blue

// System — dark violet bg; CPU reds, RAM greens, WPM blues
#define COL_SYSTEM_BG         LVC(0x0F001A)  // very dark violet
#define COL_SYSTEM_LABEL      LVC(0xFF00FF)  // magenta (widget-level fallback)
#define COL_SYSTEM_BAR_BG     LVC(0x150015)  // very dark magenta (widget fallback)
#define COL_SYSTEM_BAR_FILL   LVC(0xFF44FF)  // magenta (widget fallback)
#define COL_SYSTEM_BAR_WARN   LVC(0xFF88FF)  // light magenta (widget fallback)
#define COL_SYSTEM_BAR_ALERT  LVC(0xFFAAFF)  // pale magenta (widget fallback)

#define COL_SYSTEM_CPU_LABEL    LVC(0xFF4444)  // red
#define COL_SYSTEM_CPU_BAR_BG   LVC(0x220000)  // very dark red
#define COL_SYSTEM_CPU_BAR_FILL LVC(0xFF4444)  // coral red
#define COL_SYSTEM_CPU_BAR_WARN LVC(0xFF8800)  // orange
#define COL_SYSTEM_CPU_BAR_ALERT LVC(0xFF0000) // pure red

#define COL_SYSTEM_RAM_LABEL    LVC(0x00FF44)  // lime
#define COL_SYSTEM_RAM_BAR_BG   LVC(0x002200)  // very dark green
#define COL_SYSTEM_RAM_BAR_FILL LVC(0x00DD44)  // green
#define COL_SYSTEM_RAM_BAR_WARN LVC(0x88FF00)  // yellow-green
#define COL_SYSTEM_RAM_BAR_ALERT LVC(0xFF8800) // orange

#define COL_SYSTEM_WPM_LABEL    LVC(0x4488FF)  // sky blue
#define COL_SYSTEM_WPM_BAR_BG   LVC(0x000022)  // very dark blue
#define COL_SYSTEM_WPM_BAR_FILL LVC(0x4488FF)  // sky blue
#define COL_SYSTEM_WPM_BAR_WARN LVC(0x8844FF)  // purple
#define COL_SYSTEM_WPM_BAR_ALERT LVC(0xFF44FF) // magenta
#define COL_SYSTEM_WPM_INACTIVE  LVC(0x222244) // dark slate blue

// Claude — dark teal bg; gold tokens; H5=violets, W7=teals
#define COL_CLAUDE_BG           LVC(0x001A1A)  // very dark teal
#define COL_CLAUDE_TITLE        LVC(0x00FFFF)  // cyan
#define COL_CLAUDE_DOT_OK       LVC(0x00FF88)  // bright mint
#define COL_CLAUDE_DOT_WARN     LVC(0xFFAA00)  // amber
#define COL_CLAUDE_DOT_ALERT    LVC(0xFF2244)  // red-pink
#define COL_CLAUDE_DOT_IDLE     LVC(0x0044AA)  // medium blue
#define COL_CLAUDE_TOKENS_OUT   LVC(0xFFDD00)  // gold
#define COL_CLAUDE_TOK_BAR_BG   LVC(0x1A1400)  // dark gold
#define COL_CLAUDE_TOK_BAR_FILL LVC(0xFFDD00)  // gold
#define COL_CLAUDE_TOK_BAR_WARN LVC(0xFF8800)  // orange
#define COL_CLAUDE_TOK_BAR_ALERT LVC(0xFF2200) // red-orange
#define COL_CLAUDE_META         LVC(0x88CCFF)  // light blue
#define COL_CLAUDE_DIVIDER      LVC(0xFF00FF)  // magenta

#define COL_CLAUDE_H5_LABEL     LVC(0xAA44FF)  // violet
#define COL_CLAUDE_H5_RESET     LVC(0xCC88FF)  // light violet
#define COL_CLAUDE_H5_BAR_BG    LVC(0x110022)  // very dark violet
#define COL_CLAUDE_H5_BAR_FILL  LVC(0xAA44FF)  // violet
#define COL_CLAUDE_H5_BAR_WARN  LVC(0xFF44FF)  // magenta
#define COL_CLAUDE_H5_BAR_ALERT LVC(0xFF0088)  // hot pink

#define COL_CLAUDE_W7_LABEL     LVC(0x00CCAA)  // teal
#define COL_CLAUDE_W7_RESET     LVC(0x88FFEE)  // light teal
#define COL_CLAUDE_W7_BAR_BG    LVC(0x001A14)  // very dark teal
#define COL_CLAUDE_W7_BAR_FILL  LVC(0x00CCAA)  // teal
#define COL_CLAUDE_W7_BAR_WARN  LVC(0x00FFAA)  // bright teal
#define COL_CLAUDE_W7_BAR_ALERT LVC(0xFF8800)  // orange

// Status — dark gold bg; lime active, sky idle, red offline
#define COL_STATUS_BG           LVC(0x1A1000)  // very dark gold
#define COL_STATUS_ACTIVE       LVC(0x00FF44)  // lime
#define COL_STATUS_IDLE         LVC(0x4488FF)  // sky blue
#define COL_STATUS_OFFLINE      LVC(0xFF2222)  // red
#define COL_STATUS_IDLE_TIME    LVC(0xFFDD44)  // gold
#define COL_STATUS_IP           LVC(0xFF88CC)  // pink

// -----------------------------------------------------------------------------
// RGB565 MACROS  (for direct TFT_eSPI drawing outside LVGL)
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
