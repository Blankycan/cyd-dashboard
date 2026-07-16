#!/usr/bin/env bash
# Reverses install_companion_as_service.sh: stops and disables the systemd
# user service and removes its unit file. Safe to re-run; does nothing if
# the service was never installed.

set -euo pipefail

SERVICE_DIR="$HOME/.config/systemd/user"
SERVICE_FILE="$SERVICE_DIR/cyd-dashboard.service"

if [[ ! -f "$SERVICE_FILE" ]]; then
    echo "Not installed — $SERVICE_FILE does not exist. Nothing to do."
    exit 0
fi

if systemctl --user is-active --quiet cyd-dashboard 2>/dev/null; then
    echo "Stopping cyd-dashboard..."
fi

systemctl --user disable --now cyd-dashboard 2>/dev/null || true
rm -f "$SERVICE_FILE"
systemctl --user daemon-reload

echo "Removed $SERVICE_FILE and disabled the cyd-dashboard service."
echo "The companion app itself is untouched — you can still run it directly:"
echo "  cd companion && python main.py"
