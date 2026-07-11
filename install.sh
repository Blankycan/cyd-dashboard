#!/usr/bin/env bash
set -euo pipefail

INSTALL_DIR="$HOME/.local/share/cyd-dashboard"
SERVICE_DIR="$HOME/.config/systemd/user"
SERVICE_FILE="$SERVICE_DIR/cyd-dashboard.service"
PORT="${1:-/dev/ttyUSB0}"

echo "Installing CYD Dashboard companion..."

# Copy companion files
mkdir -p "$INSTALL_DIR"
cp -r companion/* "$INSTALL_DIR/"
echo "  Installed companion to $INSTALL_DIR"

# Write systemd user service
mkdir -p "$SERVICE_DIR"
cat > "$SERVICE_FILE" <<EOF
[Unit]
Description=CYD Dashboard companion
After=graphical-session.target
Wants=graphical-session.target

[Service]
ExecStart=$(which python3) $INSTALL_DIR/main.py --port $PORT
WorkingDirectory=$INSTALL_DIR
Restart=always
RestartSec=5
Environment=PYTHONUNBUFFERED=1

[Install]
WantedBy=default.target
EOF
echo "  Wrote $SERVICE_FILE"

# Enable and start
systemctl --user daemon-reload
systemctl --user enable --now cyd-dashboard
echo "  Service enabled and started"

echo ""
echo "Done. Check status with:  systemctl --user status cyd-dashboard"
echo "       Follow logs with:  journalctl --user -fu cyd-dashboard"
echo ""
echo "To change the serial port, edit $SERVICE_FILE and run:"
echo "  systemctl --user daemon-reload && systemctl --user restart cyd-dashboard"
