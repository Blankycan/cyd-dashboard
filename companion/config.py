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

# Scene events -------------------------------------------------------------
# Live events for the board's ambient scenes, sent the moment they happen
# rather than with the next stats packet.
KEY_EVENTS_ENABLED   = True   # key-press categories (never which key) — see keyboard.py
AUDIO_ENERGY_ENABLED = True   # beat / tempo / intensity / bass of playing music — see audio.py
                              # (needs numpy; captures audio only while something plays)

# Claude token scanner -----------------------------------------------------
SCAN_INTERVAL  = 60.0    # seconds between JSONL scans for today's session count
FETCH_INTERVAL = 300.0   # seconds between API calls for rate-limit headers

# Google Calendar ------------------------------------------------------------
# OAuth client (Desktop app) from Google Cloud Console, and the token that
# `python gcal_auth.py` saves after the one-time browser login. Both stay
# outside the repo; without them the calendar panel is simply left empty.
GOOGLE_CONFIG_DIR   = Path.home() / ".config" / "cyd-dashboard"
GOOGLE_CLIENT_FILE  = GOOGLE_CONFIG_DIR / "google_client.json"
GOOGLE_TOKEN_FILE   = GOOGLE_CONFIG_DIR / "google_token.json"
GOOGLE_SCOPES       = ["https://www.googleapis.com/auth/calendar.readonly"]
CALENDAR_POLL_INTERVAL = 300.0  # seconds between calendar fetches
# Calendars merged into the panel, one ID per line in CALENDAR_IDS_FILE
# ("primary" = the logged-in account's own; "#" starts a comment). Kept
# outside the repo since IDs are often email addresses. Falls back to just
# the primary calendar if the file is missing. List every calendar the
# account can see with: python gcal.py
CALENDAR_IDS_FILE = GOOGLE_CONFIG_DIR / "calendars.txt"

# Claude activity hooks -----------------------------------------------------
# A session's "active" entry older than this is treated as an orphan (e.g.
# a session that crashed without firing its Stop hook) and ignored, so a
# stale entry can't inflate the working-session count forever.
ACTIVITY_STALE_SECS = 7200
