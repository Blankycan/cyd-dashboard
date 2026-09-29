#pragma once

// ── User configuration ────────────────────────────────────────────────────
// Values a user is likely to want to tune.  Colour palette lives in theme.h.

// Colour theme ---------------------------------------------------------------
// Pick the active theme. Themes live in src/themes/ as standalone palette
// files — see theme.h for how the selected one gets pulled in. To add a new
// theme: copy src/themes/forest.h to src/themes/<name>.h, tweak the PAL_*
// values, add a CYD_THEME_<NAME> id below, and wire it into theme.h.
#define CYD_THEME_FOREST        1
#define CYD_THEME_GRAPE_EMBER   2
#define CYD_THEME_NEON_ROSE     3
#define CYD_THEME_RAINBOW       4
#define CYD_THEME_COZYFALL      5

#define CYD_THEME  CYD_THEME_COZYFALL

// Sleep & backlight --------------------------------------------------------
// Inactivity time before the display dims (milliseconds)
#define SLEEP_TIMEOUT_MS      (5 * 60 * 1000)
// Backlight level while awake and while sleeping (0 = off, 255 = maximum)
#define BL_FULL               255
#define BL_DIM                12

// Connection ---------------------------------------------------------------
// Silence from the companion app before the display shows "offline" (ms)
#define DISCONNECT_TIMEOUT_MS 5000

// Calendar panel -------------------------------------------------------------
// Default span of the day timeline (hours, local time). It widens on its own to
// fit any meeting outside this range.
#define CAL_DAY_START_H       7
#define CAL_DAY_END_H         16
// Countdown turns to the "soon" colour and pulses this many minutes before a meeting
#define CAL_SOON_MIN          5
#define CAL_PULSE_MS          1500
// Events kept per day — must be >= MAX_EVENTS in companion/gcal.py
#define CAL_MAX_EVENTS        12

// Claude working-session dots ------------------------------------------------
// One small dot per currently-active Claude Code session, shown to the
// right of the "claude" label. Requires the hooks installed by
// install_claude_activity_hooks.sh — see companion/claude_activity.py.
#define CLAUDE_SESSION_MAX_DOTS   8      // how many concurrent sessions to show before collapsing to "+"
#define CLAUDE_SESSION_BREATH_MS  1500   // full fade cycle while a session is active
#define CLAUDE_SESSION_HOLD_MS    4000   // how long a finished dot holds idle color before it vanishes
