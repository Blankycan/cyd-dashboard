#!/usr/bin/env bash
# Installs the CYD Dashboard companion as a systemd user service.
# Run from anywhere; the script resolves its own location.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPANION_DIR="$SCRIPT_DIR/companion"
SERVICE_DIR="$HOME/.config/systemd/user"
SERVICE_FILE="$SERVICE_DIR/cyd-dashboard.service"

# Prefer the repo's own venv (if present) over whatever python3 is on PATH,
# so the service doesn't need the venv activated in the calling shell.
if [[ -x "$SCRIPT_DIR/.venv/bin/python3" ]]; then
    PYTHON="$SCRIPT_DIR/.venv/bin/python3"
else
    PYTHON="$(command -v python3 || command -v python)"
fi

if [[ ! -f "$COMPANION_DIR/main.py" ]]; then
    echo "Error: companion/main.py not found (expected at $COMPANION_DIR/main.py)" >&2
    exit 1
fi

echo "Using Python: $PYTHON"

if ! "$PYTHON" -c 'import psutil, serial, evdev' 2>/dev/null; then
    echo "Error: $PYTHON is missing one of psutil/pyserial/evdev." >&2
    echo "Create a venv and install deps first:" >&2
    echo "  python3 -m venv $SCRIPT_DIR/.venv" >&2
    echo "  $SCRIPT_DIR/.venv/bin/pip install psutil pyserial evdev" >&2
    exit 1
fi

mkdir -p "$SERVICE_DIR"

cat > "$SERVICE_FILE" <<EOF
[Unit]
Description=CYD Dashboard companion
After=network.target

[Service]
WorkingDirectory=$COMPANION_DIR
ExecStart=$PYTHON main.py
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
EOF

systemctl --user daemon-reload
systemctl --user enable --now cyd-dashboard

echo ""
echo "Service installed and started."
echo "  Status : systemctl --user status cyd-dashboard"
echo "  Logs   : journalctl --user -u cyd-dashboard -f"
echo "  Stop   : systemctl --user stop cyd-dashboard"
echo "  Remove : systemctl --user disable --now cyd-dashboard && rm $SERVICE_FILE"
