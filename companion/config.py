# companion/config.py — user-tunable settings for the CYD Dashboard companion.
# Edit these without touching the rest of the code.

from pathlib import Path

# Serial / packet timing ---------------------------------------------------
INTERVAL          = 2.0    # seconds between stats packets sent to the ESP32

# Idle messages ------------------------------------------------------------
IDLE_MSG_INTERVAL = 30.0   # seconds before rotating to the next idle message

# Keyboard -----------------------------------------------------------------
IDLE_AFTER         = 2.5   # seconds without a keypress before marking keyboard idle
KEYS_SAVE_INTERVAL = 30.0  # seconds between writes of today's keystroke count
KEYS_STATE_FILE    = Path.home() / ".local" / "state" / "cyd-dashboard" / "keys.json"

# Media --------------------------------------------------------------------
MEDIA_POLL_INTERVAL = 3.0  # seconds between playerctl metadata polls

# Claude token scanner -----------------------------------------------------
SCAN_INTERVAL  = 60.0    # seconds between JSONL scans for today's token counts
FETCH_INTERVAL = 300.0   # seconds between API calls for rate-limit headers

# Claude activity hooks -----------------------------------------------------
# A session's "active" entry older than this is treated as an orphan (e.g.
# a session that crashed without firing its Stop hook) and ignored, so a
# stale entry can't inflate the working-session count forever.
ACTIVITY_STALE_SECS = 7200
