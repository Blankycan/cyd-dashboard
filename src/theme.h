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
#elif CYD_THEME == CYD_THEME_COZYFALL
    #include "themes/cozyfall.h"
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

// --- Calendar ---
#ifndef COL_CALENDAR_BG
#define COL_CALENDAR_BG COL_BG
#endif
#ifndef COL_CALENDAR_TIME
#define COL_CALENDAR_TIME COL_TEXT_SEC
#endif
#ifndef COL_CALENDAR_TITLE
#define COL_CALENDAR_TITLE COL_TITLE
#endif
#ifndef COL_CALENDAR_NOW
#define COL_CALENDAR_NOW COL_DOT_ACTIVE
#endif
#ifndef COL_CALENDAR_COUNTDOWN
#define COL_CALENDAR_COUNTDOWN COL_TEXT_SEC
#endif
#ifndef COL_CALENDAR_COUNTDOWN_SOON
#define COL_CALENDAR_COUNTDOWN_SOON COL_WARN
#endif
#ifndef COL_CALENDAR_NEXT
#define COL_CALENDAR_NEXT COL_TEXT_DIM
#endif
#ifndef COL_CALENDAR_DONE
#define COL_CALENDAR_DONE COL_TEXT_DIM
#endif
#ifndef COL_CALENDAR_PROGRESS_BG
#define COL_CALENDAR_PROGRESS_BG COL_BAR_BG
#endif
#ifndef COL_CALENDAR_PROGRESS_FILL
#define COL_CALENDAR_PROGRESS_FILL COL_CALENDAR_NOW
#endif
#ifndef COL_CALENDAR_TRACK
#define COL_CALENDAR_TRACK COL_BAR_BG
#endif
#ifndef COL_CALENDAR_EVENT
#define COL_CALENDAR_EVENT COL_TEXT_SEC
#endif
#ifndef COL_CALENDAR_EVENT_PAST
#define COL_CALENDAR_EVENT_PAST COL_TEXT_DIM
#endif
#ifndef COL_CALENDAR_EVENT_NOW
#define COL_CALENDAR_EVENT_NOW COL_CALENDAR_NOW
#endif
#ifndef COL_CALENDAR_NOW_MARK
#define COL_CALENDAR_NOW_MARK COL_TEXT_PRI
#endif
#ifndef COL_CALENDAR_HOUR
#define COL_CALENDAR_HOUR COL_TEXT_DIM
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
#ifndef COL_CLAUDE_WORK_DOT_ACTIVE
#define COL_CLAUDE_WORK_DOT_ACTIVE COL_DOT_ACTIVE
#endif
#ifndef COL_CLAUDE_WORK_DOT_IDLE
#define COL_CLAUDE_WORK_DOT_IDLE COL_DOT_IDLE
#endif
#ifndef COL_CLAUDE_WORK_DOT_OVERFLOW
#define COL_CLAUDE_WORK_DOT_OVERFLOW COL_DOT_IDLE
#endif
#ifndef COL_CLAUDE_SESSIONS
#define COL_CLAUDE_SESSIONS COL_TEXT_DIM
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
#ifndef COL_STATUS_KEYS
#define COL_STATUS_KEYS COL_TEXT_SEC
#endif
#ifndef COL_STATUS_IP
#define COL_STATUS_IP COL_TEXT_DIM
#endif
