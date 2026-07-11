#pragma once

// ── User configuration ────────────────────────────────────────────────────
// Values a user is likely to want to tune.  Colour palette lives in theme.h.

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
// saturates if you go beyond it.  250 000 suits moderate-heavy daily use;
// raise to 500 000 or 1 000 000 if you consistently hit the ceiling.
#define CLAUDE_TOK_MAX        250000
