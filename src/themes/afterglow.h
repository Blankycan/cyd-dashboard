#pragma once
// =============================================================================
// CYD Dashboard — color theme: Afterglow (dusk blue sky with the last orange
// band of sunset on the horizon, matches the Omarchy "afterglow" desktop theme)
//
// Selected via CYD_THEME_AFTERGLOW in config.h (see theme.h for the dispatch).
//
// Ported from ~/.config/omarchy/themes/afterglow/colors.toml with the hardware
// lessons from cozyfall/scarlet-witch applied up front: this panel shifts pale
// tones blue and drifts saturated warm tones yellow. So the desktop's pale
// blue-white text is NOT used (it would just read as washed-out blue); text is
// saturated gold and sky blue instead. The glow orange is pushed redder than
// the desktop's #e98a5c to survive the yellow drift. Reds carry no blue (blue
// in a red turns it purple/pink here). Blues are the one family this panel is
// kind to, so the navy darks are kept fairly neutral rather than extra-blue.
// Expect a hardware tuning pass.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// RAW PALETTE  (24-bit RGB hex)
// -----------------------------------------------------------------------------

#define PAL_GLOW        0xFF5418  // horizon glow     — active accents (desktop accent #e98a5c, pushed red)

// --- UI backgrounds ---
#define PAL_BG          0x050C16  // dusk navy        — main background (desktop #0b131c)
#define PAL_PANEL       0x0C2038  // deep sky         — panel / card background
#define PAL_BORDER      0x2C7CD0  // sky blue         — panel borders (desktop blue #5e9bd6)
#define PAL_DIVIDER     0x14365E  // dim sky          — horizontal dividers

// --- Text ---
#define PAL_TEXT_PRI    0xFFA040  // horizon gold     — primary labels (clock, titles) — saturated, NOT pale cream
#define PAL_TEXT_SEC    0x4CB4F0  // bright sky       — secondary / subtitles (desktop cyan #65c8e8)
#define PAL_TEXT_DIM    0x4A78A8  // steel blue       — timestamps, inactive text

// --- Status / progress ---
#define PAL_OK          0x2C90F0  // sky blue         — idle, normal, connected
#define PAL_WARN        0xFF4A00  // sunset orange    — usage >70%, mild alert (0xFF7A10 read yellow on the panel)
#define PAL_ALERT       0xFF1A08  // red              — usage >90%, disconnect
#define PAL_BAR_TRACK   0x0A1828  // near-black navy  — progress bar background

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
// LEVEL 3 — Per-widget overrides specific to Afterglow
// Topbar sits on the deep-sky panel. The glow orange is the "something is
// happening" colour: active dot and now-playing icon. The skies in the
// synthwave and lofi scenes are repainted like the wallpapers, blue overhead
// fading into an orange band at the horizon (the defaults put the warm accent
// at the top of the lofi sky).
// -----------------------------------------------------------------------------

#define COL_TOPBAR_BG         LVC(PAL_PANEL)
#define COL_TOPBAR_CLOCK      LVC(PAL_TEXT_SEC)   // sky blue; gold clock looked off on hardware
#define COL_TOPBAR_DATE       LVC(PAL_TEXT_DIM)

#define COL_DOT_ACTIVE        LVC(PAL_GLOW)

#define COL_MUSIC_ICON_PLAY   LVC(PAL_GLOW)

#define COL_SCENE_SYNTH_SKY_TOP   LVC(PAL_BG)
#define COL_SCENE_SYNTH_SKY_LOW   lv_color_mix(LVC(PAL_GLOW), LVC(PAL_BG), 120)

#define COL_SCENE_LOFI_SKY_TOP    lv_color_mix(LVC(PAL_BORDER), LVC(PAL_BG), 70)
#define COL_SCENE_LOFI_SKY_LOW    lv_color_mix(LVC(PAL_GLOW), LVC(PAL_BG), 110)

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
