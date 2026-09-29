#pragma once
#include "config.h"

// Screen geometry and hardware wiring constants for the 240×320 CYD display.
// User-tunable settings (timeouts, brightness, limits) live in config.h.

#define SCREEN_W    240
#define SCREEN_H    320
#define BL_PIN      21

#define TOPBAR_H    30
#define DIV_W       1

#define CONTENT_Y   (TOPBAR_H + DIV_W)
#define CONTENT_H   (SCREEN_H - CONTENT_Y)

#define PANEL_W     SCREEN_W

// Every stacked panel keeps PANEL_PAD_Y between its top edge and the top of
// its first line's capital letters, and between its last content (text
// baseline or bar) and its bottom edge.
#define PANEL_PAD_Y 6

#define MUSIC_H     42
#define STATS_H     50
#define STATS_ROW_H 22
#define CLAUDE_H    73
#define INDICATOR_H TOPBAR_H
// CALENDAR_H consumes all remaining space between the topbar and the status bar
// (four DIV_W dividers: below calendar, music, stats, and claude).
#define CALENDAR_H  (CONTENT_H - MUSIC_H - STATS_H - CLAUDE_H - INDICATOR_H - 4 * DIV_W)

// Music panel — right-side animation area
#define MA_W          38
#define MA_X          (PANEL_W - 4 - MA_W)
#define MUSIC_LABEL_W (MA_X - 26)

// Backlight PWM hardware (wiring — do not change unless rewiring the board)
#define BL_PWM_CHANNEL    0
#define BL_PWM_FREQ       5000
#define BL_PWM_BITS       8
