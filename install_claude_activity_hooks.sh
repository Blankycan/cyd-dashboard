#!/usr/bin/env bash
# Installs Claude Code hooks that let the CYD Dashboard show per-session
# "working" dots on the Claude panel (see companion/claude_activity.py and
# companion/hooks/session_touch.py). Run from anywhere; resolves its own
# location. Idempotent — safe to re-run.
#
# This is a machine-wide change: it edits ~/.claude/settings.json, so it
# affects hook firing for every Claude Code session on this machine, not
# just ones related to this dashboard.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOOK_SCRIPT="$SCRIPT_DIR/companion/hooks/session_touch.py"
SETTINGS_FILE="$HOME/.claude/settings.json"

if [[ ! -f "$HOOK_SCRIPT" ]]; then
    echo "Error: $HOOK_SCRIPT not found" >&2
    exit 1
fi

PYTHON="$(command -v python3 || command -v python || true)"
if [[ -z "$PYTHON" ]]; then
    echo "Error: python3 not found on PATH" >&2
    exit 1
fi

echo "Hook script:   $HOOK_SCRIPT"
echo "Settings file: $SETTINGS_FILE"
echo ""

"$PYTHON" - "$SETTINGS_FILE" "$HOOK_SCRIPT" <<'PYEOF'
import json
import os
import sys
import time

settings_file, hook_script = sys.argv[1], sys.argv[2]

data = {}
if os.path.exists(settings_file):
    try:
        with open(settings_file) as f:
            data = json.load(f)
    except Exception as e:
        print(f"Error: could not parse existing {settings_file}: {e}", file=sys.stderr)
        sys.exit(1)

    backup = f"{settings_file}.bak-{int(time.time())}"
    with open(settings_file) as src, open(backup, "w") as dst:
        dst.write(src.read())
    print(f"Backed up existing settings to {backup}")
else:
    os.makedirs(os.path.dirname(settings_file), exist_ok=True)

hooks = data.setdefault("hooks", {})


def ensure_hook(event, command):
    groups = hooks.setdefault(event, [])
    for g in groups:
        for h in g.get("hooks", []):
            if hook_script in h.get("command", ""):
                print(f"{event} hook already present — leaving as-is")
                return
    groups.append({
        "matcher": "",
        "hooks": [{"type": "command", "command": command, "timeout": 5}],
    })
    print(f"Added {event} hook")


ensure_hook("UserPromptSubmit", f"python3 {hook_script} start")
ensure_hook("Stop", f"python3 {hook_script} stop")

with open(settings_file, "w") as f:
    json.dump(data, f, indent=2)
    f.write("\n")

print(f"Wrote {settings_file}")
PYEOF

echo ""
echo "Done. Sessions already open may need to be restarted to pick up the"
echo "new hooks."
echo ""
echo "  Status file : ~/.cache/cyd-dashboard/claude_sessions.json"
echo "  Uninstall   : remove the two hook entries referencing session_touch.py"
echo "                from $SETTINGS_FILE (a timestamped backup was made above)"
