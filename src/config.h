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

#define CYD_THEME  CYD_THEME_RAINBOW

// Sleep & backlight --------------------------------------------------------
// Inactivity time before the display dims (milliseconds)
#define SLEEP_TIMEOUT_MS      (5 * 60 * 1000)
// Backlight level while awake and while sleeping (0 = off, 255 = maximum)
#define BL_FULL               255
#define BL_DIM                12

// Connection ---------------------------------------------------------------
// Silence from the companion app before the display shows "offline" (ms)
#define DISCONNECT_TIMEOUT_MS 5000

// Claude token bar ---------------------------------------------------------
// Daily output-token ceiling: the bar reaches 100% at this value and
// saturates if you go beyond it.  500 000 suits moderate-heavy daily use;
// raise to 1 000 000 if you consistently hit the ceiling.
#define CLAUDE_TOK_MAX        500000
