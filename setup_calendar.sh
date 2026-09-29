#!/usr/bin/env bash
# Sets up the calendar panel on this machine: installs the Google API
# packages, runs the one-time browser login, and restarts the companion.
# Idempotent — safe to re-run; pass --relogin to replace an existing token.
#
# Needs ~/.config/cyd-dashboard/google_client.json first (the OAuth client
# downloaded from Google Cloud Console, or copied from another machine) —
# see "Setting up the calendar" in README.md.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPANION_DIR="$SCRIPT_DIR/companion"
CONFIG_DIR="$HOME/.config/cyd-dashboard"
CLIENT_FILE="$CONFIG_DIR/google_client.json"
TOKEN_FILE="$CONFIG_DIR/google_token.json"
IDS_FILE="$CONFIG_DIR/calendars.txt"
SERVICE_FILE="$HOME/.config/systemd/user/cyd-dashboard.service"

RELOGIN=false
[[ "${1:-}" == "--relogin" ]] && RELOGIN=true

# Use the same interpreter as the installed service, so the packages land
# where the companion will actually import them. Otherwise pick it the same
# way install_companion_as_service.sh does.
if [[ -f "$SERVICE_FILE" ]]; then
    PYTHON="$(sed -n 's/^ExecStart=\(\S*\) .*/\1/p' "$SERVICE_FILE")"
elif [[ -x "$SCRIPT_DIR/.venv/bin/python3" ]]; then
    PYTHON="$SCRIPT_DIR/.venv/bin/python3"
else
    PYTHON="$(command -v python3 || command -v python)"
fi
echo "Using Python: $PYTHON"

if [[ ! -f "$CLIENT_FILE" ]]; then
    echo "Error: $CLIENT_FILE not found." >&2
    echo "Copy it from your other machine, or download it from Google Cloud Console" >&2
    echo "(see \"Setting up the calendar\" in README.md)." >&2
    exit 1
fi

# Keep the credentials owner-only
chmod 700 "$CONFIG_DIR"
chmod 600 "$CLIENT_FILE"
[[ -f "$IDS_FILE" ]] && chmod 600 "$IDS_FILE"

echo ""
echo "Installing Google API packages..."
"$PYTHON" -m pip install --quiet --disable-pip-version-check \
    google-auth google-auth-oauthlib requests

echo ""
if [[ -f "$TOKEN_FILE" && "$RELOGIN" == false ]]; then
    echo "Already logged in ($TOKEN_FILE exists) — skipping. Use --relogin to redo it."
else
    echo "Opening a browser for the Google login — pick your account and click Allow."
    (cd "$COMPANION_DIR" && "$PYTHON" gcal_auth.py)
fi

echo ""
if [[ -f "$IDS_FILE" ]]; then
    echo "Calendars shown ($IDS_FILE):"
    grep -v '^\s*\(#\|$\)' "$IDS_FILE" | sed 's/^/  /'
else
    echo "No $IDS_FILE — only your primary calendar will be shown."
    echo "To add more, put one ID per line there. Calendars this account can see:"
    (cd "$COMPANION_DIR" && "$PYTHON" gcal.py) | sed 's/^/  /'
fi

if systemctl --user is-active --quiet cyd-dashboard 2>/dev/null; then
    systemctl --user restart cyd-dashboard
    echo ""
    echo "Restarted the cyd-dashboard service."
else
    echo ""
    echo "Companion service isn't running — start it with ./install_companion_as_service.sh"
    echo "or run: cd companion && $PYTHON main.py"
fi
