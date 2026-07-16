#!/usr/bin/env bash
# Reverses install_claude_activity_hooks.sh: removes the UserPromptSubmit/Stop
# hook entries referencing session_touch.py from ~/.claude/settings.json (only
# those entries — any other hooks you have configured are left alone), and
# deletes the working-session status file/cache dir. Safe to re-run; does
# nothing if the hooks were never installed.

set -euo pipefail

SETTINGS_FILE="$HOME/.claude/settings.json"
CACHE_DIR="$HOME/.cache/cyd-dashboard"

PYTHON="$(command -v python3 || command -v python || true)"
if [[ -z "$PYTHON" ]]; then
    echo "Error: python3 not found on PATH" >&2
    exit 1
fi

if [[ -f "$SETTINGS_FILE" ]]; then
    "$PYTHON" - "$SETTINGS_FILE" <<'PYEOF'
import json
import os
import sys
import time

settings_file = sys.argv[1]

with open(settings_file) as f:
    original_text = f.read()
data = json.loads(original_text)

hooks = data.get("hooks", {})
removed = False

for event in ("UserPromptSubmit", "Stop"):
    groups = hooks.get(event)
    if not groups:
        continue
    kept = [
        g for g in groups
        if not any("session_touch.py" in h.get("command", "") for h in g.get("hooks", []))
    ]
    if len(kept) != len(groups):
        removed = True
    if kept:
        hooks[event] = kept
    else:
        hooks.pop(event, None)

if not removed:
    print("No cyd-dashboard hooks found in settings.json — nothing to remove.")
    sys.exit(0)

if not hooks:
    data.pop("hooks", None)

backup = f"{settings_file}.bak-{int(time.time())}"
with open(backup, "w") as dst:
    dst.write(original_text)
print(f"Backed up existing settings to {backup}")

with open(settings_file, "w") as f:
    json.dump(data, f, indent=2)
    f.write("\n")

print(f"Removed cyd-dashboard hook entries from {settings_file}")
PYEOF
else
    echo "No settings file at $SETTINGS_FILE — nothing to remove there."
fi

if [[ -d "$CACHE_DIR" ]]; then
    rm -rf "$CACHE_DIR"
    echo "Removed $CACHE_DIR"
fi

echo "Done. Restart any open Claude Code sessions to stop them running the old hooks."
