#!/usr/bin/env bash
# Installs the CYD Dashboard companion as a systemd user service.
# Run from anywhere; the script resolves its own location.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPANION_DIR="$SCRIPT_DIR/companion"
SERVICE_DIR="$HOME/.config/systemd/user"
SERVICE_FILE="$SERVICE_DIR/cyd-dashboard.service"
PYTHON="$(command -v python3 || command -v python)"

if [[ ! -f "$COMPANION_DIR/main.py" ]]; then
    echo "Error: companion/main.py not found (expected at $COMPANION_DIR/main.py)" >&2
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
