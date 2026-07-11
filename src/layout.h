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

#define MUSIC_H     52
#define STATS_H     72
#define STATS_ROW_H 22
// CLAUDE_H is sized to consume all remaining space between stats and indicator.
// Formula: SCREEN_H - CONTENT_Y - MUSIC_H - DIV_W - STATS_H - DIV_W - DIV_W - INDICATOR_H
#define CLAUDE_H    126
#define INDICATOR_H 36

// Music panel — right-side animation area
#define MA_W          38
#define MA_X          (PANEL_W - 4 - MA_W)
#define MUSIC_LABEL_W (MA_X - 26)

// Backlight PWM hardware (wiring — do not change unless rewiring the board)
#define BL_PWM_CHANNEL    0
#define BL_PWM_FREQ       5000
#define BL_PWM_BITS       8
