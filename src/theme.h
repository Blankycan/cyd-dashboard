#pragma once
// =============================================================================
// CYD Dashboard — color theme selector
//
// Pick the active theme via CYD_THEME in config.h. Each theme is a standalone
// palette file under src/themes/ — see src/themes/forest.h for the full set
// of PAL_*/COL_*/TFT_COL_* names a theme must define.
// =============================================================================

#include "config.h"

#if CYD_THEME == CYD_THEME_GRAPE_EMBER
    #include "themes/grape-ember.h"
#elif CYD_THEME == CYD_THEME_NEON_ROSE
    #include "themes/neon-rose.h"
#elif CYD_THEME == CYD_THEME_RAINBOW
    #include "themes/rainbow.h"
#else
    #include "themes/forest.h"
#endif

// =============================================================================
// Per-widget colour fallbacks — define if the selected theme doesn't set them.
// Themes that need different values override these by defining the token before
// this file is included (i.e., inside the theme header itself).
// =============================================================================

// --- Generic dot colours ---
#ifndef COL_DOT_ACTIVE
#define COL_DOT_ACTIVE COL_OK
#endif
#ifndef COL_DOT_IDLE
#define COL_DOT_IDLE COL_TEXT_DIM
#endif
#ifndef COL_DOT_WARN
#define COL_DOT_WARN COL_WARN
#endif
#ifndef COL_DOT_ALERT
#define COL_DOT_ALERT COL_ALERT
#endif

// --- Generic bar colours ---
#ifndef COL_BAR_BG
#define COL_BAR_BG COL_BAR_TRACK
#endif
#ifndef COL_BAR_FILL
#define COL_BAR_FILL COL_OK
#endif
#ifndef COL_BAR_WARN
#define COL_BAR_WARN COL_WARN
#endif
#ifndef COL_BAR_ALERT
#define COL_BAR_ALERT COL_ALERT
#endif
#ifndef COL_BAR_INACTIVE
#define COL_BAR_INACTIVE COL_TEXT_DIM
#endif
#ifndef COL_BAR_LABEL
#define COL_BAR_LABEL COL_TEXT_DIM
#endif
#ifndef COL_BAR_VALUE
#define COL_BAR_VALUE COL_BAR_FILL
#endif
#ifndef COL_BAR_EXTRA
#define COL_BAR_EXTRA COL_TEXT_DIM
#endif
#ifndef COL_TITLE
#define COL_TITLE COL_TEXT_PRI
#endif
#ifndef COL_DIVIDER_INNER
#define COL_DIVIDER_INNER COL_DIVIDER
#endif

// --- Topbar ---
#ifndef COL_TOPBAR_BG
#define COL_TOPBAR_BG COL_BG
#endif
#ifndef COL_TOPBAR_CLOCK
#define COL_TOPBAR_CLOCK COL_TEXT_PRI
#endif
#ifndef COL_TOPBAR_DATE
#define COL_TOPBAR_DATE COL_TEXT_DIM
#endif

// --- Music ---
#ifndef COL_MUSIC_BG
#define COL_MUSIC_BG COL_BG
#endif
#ifndef COL_MUSIC_TITLE
#define COL_MUSIC_TITLE COL_TITLE
#endif
#ifndef COL_MUSIC_TEXT
#define COL_MUSIC_TEXT COL_TEXT_SEC
#endif
#ifndef COL_MUSIC_ICON_PLAY
#define COL_MUSIC_ICON_PLAY COL_TEXT_PRI
#endif
#ifndef COL_MUSIC_ICON_PAUSE
#define COL_MUSIC_ICON_PAUSE COL_TEXT_SEC
#endif
#ifndef COL_MUSIC_ICON_IDLE
#define COL_MUSIC_ICON_IDLE COL_TEXT_DIM
#endif
#ifndef COL_MUSIC_DOT_PLAYING
#define COL_MUSIC_DOT_PLAYING COL_DOT_ACTIVE
#endif
#ifndef COL_MUSIC_DOT_IDLE
#define COL_MUSIC_DOT_IDLE COL_DOT_IDLE
#endif

// --- System ---
#ifndef COL_SYSTEM_BG
#define COL_SYSTEM_BG COL_BG
#endif
#ifndef COL_SYSTEM_LABEL
#define COL_SYSTEM_LABEL COL_BAR_LABEL
#endif
#ifndef COL_SYSTEM_BAR_BG
#define COL_SYSTEM_BAR_BG COL_BAR_BG
#endif
#ifndef COL_SYSTEM_BAR_FILL
#define COL_SYSTEM_BAR_FILL COL_BAR_FILL
#endif
#ifndef COL_SYSTEM_BAR_WARN
#define COL_SYSTEM_BAR_WARN COL_BAR_WARN
#endif
#ifndef COL_SYSTEM_BAR_ALERT
#define COL_SYSTEM_BAR_ALERT COL_BAR_ALERT
#endif
#ifndef COL_SYSTEM_CPU_LABEL
#define COL_SYSTEM_CPU_LABEL COL_SYSTEM_LABEL
#endif
#ifndef COL_SYSTEM_CPU_BAR_BG
#define COL_SYSTEM_CPU_BAR_BG COL_SYSTEM_BAR_BG
#endif
#ifndef COL_SYSTEM_CPU_BAR_FILL
#define COL_SYSTEM_CPU_BAR_FILL COL_SYSTEM_BAR_FILL
#endif
#ifndef COL_SYSTEM_CPU_BAR_WARN
#define COL_SYSTEM_CPU_BAR_WARN COL_SYSTEM_BAR_WARN
#endif
#ifndef COL_SYSTEM_CPU_BAR_ALERT
#define COL_SYSTEM_CPU_BAR_ALERT COL_SYSTEM_BAR_ALERT
#endif
#ifndef COL_SYSTEM_RAM_LABEL
#define COL_SYSTEM_RAM_LABEL COL_SYSTEM_LABEL
#endif
#ifndef COL_SYSTEM_RAM_BAR_BG
#define COL_SYSTEM_RAM_BAR_BG COL_SYSTEM_BAR_BG
#endif
#ifndef COL_SYSTEM_RAM_BAR_FILL
#define COL_SYSTEM_RAM_BAR_FILL COL_SYSTEM_BAR_FILL
#endif
#ifndef COL_SYSTEM_RAM_BAR_WARN
#define COL_SYSTEM_RAM_BAR_WARN COL_SYSTEM_BAR_WARN
#endif
#ifndef COL_SYSTEM_RAM_BAR_ALERT
#define COL_SYSTEM_RAM_BAR_ALERT COL_SYSTEM_BAR_ALERT
#endif
#ifndef COL_SYSTEM_WPM_LABEL
#define COL_SYSTEM_WPM_LABEL COL_SYSTEM_LABEL
#endif
#ifndef COL_SYSTEM_WPM_BAR_BG
#define COL_SYSTEM_WPM_BAR_BG COL_SYSTEM_BAR_BG
#endif
#ifndef COL_SYSTEM_WPM_BAR_FILL
#define COL_SYSTEM_WPM_BAR_FILL COL_SYSTEM_BAR_FILL
#endif
#ifndef COL_SYSTEM_WPM_BAR_WARN
#define COL_SYSTEM_WPM_BAR_WARN COL_SYSTEM_BAR_WARN
#endif
#ifndef COL_SYSTEM_WPM_BAR_ALERT
#define COL_SYSTEM_WPM_BAR_ALERT COL_SYSTEM_BAR_ALERT
#endif
#ifndef COL_SYSTEM_WPM_INACTIVE
#define COL_SYSTEM_WPM_INACTIVE COL_BAR_INACTIVE
#endif

// --- Claude ---
#ifndef COL_CLAUDE_BG
#define COL_CLAUDE_BG COL_BG
#endif
#ifndef COL_CLAUDE_TITLE
#define COL_CLAUDE_TITLE COL_TITLE
#endif
#ifndef COL_CLAUDE_DOT_OK
#define COL_CLAUDE_DOT_OK COL_DOT_ACTIVE
#endif
#ifndef COL_CLAUDE_DOT_WARN
#define COL_CLAUDE_DOT_WARN COL_DOT_WARN
#endif
#ifndef COL_CLAUDE_DOT_ALERT
#define COL_CLAUDE_DOT_ALERT COL_DOT_ALERT
#endif
#ifndef COL_CLAUDE_DOT_IDLE
#define COL_CLAUDE_DOT_IDLE COL_DOT_IDLE
#endif
#ifndef COL_CLAUDE_TOKENS_OUT
#define COL_CLAUDE_TOKENS_OUT COL_TEXT_PRI
#endif
#ifndef COL_CLAUDE_TOK_BAR_BG
#define COL_CLAUDE_TOK_BAR_BG COL_BAR_BG
#endif
#ifndef COL_CLAUDE_TOK_BAR_FILL
#define COL_CLAUDE_TOK_BAR_FILL COL_BAR_FILL
#endif
#ifndef COL_CLAUDE_TOK_BAR_WARN
#define COL_CLAUDE_TOK_BAR_WARN COL_BAR_WARN
#endif
#ifndef COL_CLAUDE_TOK_BAR_ALERT
#define COL_CLAUDE_TOK_BAR_ALERT COL_BAR_ALERT
#endif
#ifndef COL_CLAUDE_META
#define COL_CLAUDE_META COL_TEXT_DIM
#endif
#ifndef COL_CLAUDE_DIVIDER
#define COL_CLAUDE_DIVIDER COL_DIVIDER_INNER
#endif
#ifndef COL_CLAUDE_RATE_LABEL
#define COL_CLAUDE_RATE_LABEL COL_BAR_LABEL
#endif
#ifndef COL_CLAUDE_RATE_RESET
#define COL_CLAUDE_RATE_RESET COL_BAR_EXTRA
#endif
#ifndef COL_CLAUDE_RATE_BAR_BG
#define COL_CLAUDE_RATE_BAR_BG COL_BAR_BG
#endif
#ifndef COL_CLAUDE_RATE_BAR_FILL
#define COL_CLAUDE_RATE_BAR_FILL COL_BAR_FILL
#endif
#ifndef COL_CLAUDE_RATE_BAR_WARN
#define COL_CLAUDE_RATE_BAR_WARN COL_BAR_WARN
#endif
#ifndef COL_CLAUDE_RATE_BAR_ALERT
#define COL_CLAUDE_RATE_BAR_ALERT COL_BAR_ALERT
#endif
#ifndef COL_CLAUDE_H5_LABEL
#define COL_CLAUDE_H5_LABEL COL_CLAUDE_RATE_LABEL
#endif
#ifndef COL_CLAUDE_H5_RESET
#define COL_CLAUDE_H5_RESET COL_CLAUDE_RATE_RESET
#endif
#ifndef COL_CLAUDE_H5_BAR_BG
#define COL_CLAUDE_H5_BAR_BG COL_CLAUDE_RATE_BAR_BG
#endif
#ifndef COL_CLAUDE_H5_BAR_FILL
#define COL_CLAUDE_H5_BAR_FILL COL_CLAUDE_RATE_BAR_FILL
#endif
#ifndef COL_CLAUDE_H5_BAR_WARN
#define COL_CLAUDE_H5_BAR_WARN COL_CLAUDE_RATE_BAR_WARN
#endif
#ifndef COL_CLAUDE_H5_BAR_ALERT
#define COL_CLAUDE_H5_BAR_ALERT COL_CLAUDE_RATE_BAR_ALERT
#endif
#ifndef COL_CLAUDE_W7_LABEL
#define COL_CLAUDE_W7_LABEL COL_CLAUDE_RATE_LABEL
#endif
#ifndef COL_CLAUDE_W7_RESET
#define COL_CLAUDE_W7_RESET COL_CLAUDE_RATE_RESET
#endif
#ifndef COL_CLAUDE_W7_BAR_BG
#define COL_CLAUDE_W7_BAR_BG COL_CLAUDE_RATE_BAR_BG
#endif
#ifndef COL_CLAUDE_W7_BAR_FILL
#define COL_CLAUDE_W7_BAR_FILL COL_CLAUDE_RATE_BAR_FILL
#endif
#ifndef COL_CLAUDE_W7_BAR_WARN
#define COL_CLAUDE_W7_BAR_WARN COL_CLAUDE_RATE_BAR_WARN
#endif
#ifndef COL_CLAUDE_W7_BAR_ALERT
#define COL_CLAUDE_W7_BAR_ALERT COL_CLAUDE_RATE_BAR_ALERT
#endif

// --- Status ---
#ifndef COL_STATUS_BG
#define COL_STATUS_BG COL_BG
#endif
#ifndef COL_STATUS_ACTIVE
#define COL_STATUS_ACTIVE COL_DOT_ACTIVE
#endif
#ifndef COL_STATUS_IDLE
#define COL_STATUS_IDLE COL_DOT_IDLE
#endif
#ifndef COL_STATUS_OFFLINE
#define COL_STATUS_OFFLINE COL_DOT_ALERT
#endif
#ifndef COL_STATUS_IDLE_TIME
#define COL_STATUS_IDLE_TIME COL_TEXT_DIM
#endif
#ifndef COL_STATUS_IP
#define COL_STATUS_IP COL_TEXT_DIM
#endif
