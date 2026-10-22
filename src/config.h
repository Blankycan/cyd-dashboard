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
#define CYD_THEME_SCARLET_WITCH 6

#define CYD_THEME  CYD_THEME_SCARLET_WITCH

// Ambient scenes -------------------------------------------------------------
// Played in the calendar slot whenever no meeting is near. Which scenes play,
// and how, is picked on the board itself in the settings menu (gear icon, top
// right) and kept across reboots and reflashes. These are only the first-boot
// defaults. Every scene in src/scenes/ is available and starts out switched on.
#define SCENES_DEFAULT_ON        1   // play scenes at all (0 = calendar only)
#define CAL_BETWEEN_DEFAULT      1   // show the calendar between scenes...
#define CAL_SEC_DEFAULT          10  // ...for this long (seconds), also after a tap...
#define CAL_SEC_MIN              5   // ...settable in the menu from this
#define CAL_SEC_MAX              60  // ...to this
#define CAL_SEC_STEP             5   // ...in steps of this
#define SCENE_SHUFFLE_DEFAULT    1   // 1 = random order each cycle, 0 = alphabetical
#define SCENE_MIN_DEFAULT        3   // how long each scene plays (minutes)...
#define SCENE_MIN_MAX            10  // ...settable in the menu from 1 to this

#define CAL_QUIET_MIN         10               // calendar only from this many minutes before a meeting until it ends
#define SCENE_STOP_GRACE_MS   (12 * 1000)      // time a scene gets to wrap up before it's cut off
#define SCENE_FRAME_MS        40               // scene frame period (25 fps)
#define SCENE_READY_MS        2000             // games wait this long on a "READY" sign before playing themselves
#define SCENE_EVENT_LOG       0                // 1 = log every host event a scene receives (debugging)
#define MENU_IDLE_CLOSE_MS    (60 * 1000)      // the settings menu closes itself after this long untouched

// New song card ----------------------------------------------------------------
// Slides in over the scene slot when a new song starts (switch in the menu).
#define TRACK_CARD_DEFAULT    1                // first-boot default: on
#define TRACK_CARD_MS         (7 * 1000)       // how long it stays up
#define TRACK_ART_WAIT_MS     4000             // longest wait for the cover before showing without it
#define TRACK_ART_MAX         80               // cover size limit (px) — must be >= ART_SIZE in companion/track_card.py
#define SCENE_IN_USE_MS       (15 * 1000)      // no card for this long after someone touches a scene

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
#define CAL_MAX_ALLDAY        6

// Claude working-session dots ------------------------------------------------
// One small dot per currently-active Claude Code session, shown to the
// right of the "claude" label. Requires the hooks installed by
// install_claude_activity_hooks.sh — see companion/claude_activity.py.
#define CLAUDE_SESSION_MAX_DOTS   8      // how many concurrent sessions to show before collapsing to "+"
#define CLAUDE_SESSION_BREATH_MS  1500   // full fade cycle while a session is active
#define CLAUDE_SESSION_HOLD_MS    4000   // how long a finished dot holds idle color before it vanishes
