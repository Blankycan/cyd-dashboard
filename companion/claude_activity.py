"""
Claude session-activity monitor — reports how many local Claude Code
sessions are currently mid-turn ("working"), for the firmware's per-session
dots on the Claude panel.

This is a thin reader of a small status file kept fresh by Claude Code
itself via hooks (see companion/hooks/session_touch.py and
install_claude_activity_hooks.sh) — no scanning or API calls of its own.
If those hooks were never installed, the status file simply never appears
and working_count() stays 0 forever; __init__ warns about that once so it
doesn't look like a silent bug.
"""
import json
import os
import time
from pathlib import Path

from config import ACTIVITY_STALE_SECS

STATUS_FILE   = Path.home() / ".cache" / "cyd-dashboard" / "claude_sessions.json"
SETTINGS_FILE = Path.home() / ".claude" / "settings.json"


def _hooks_installed() -> bool:
    try:
        return "session_touch.py" in SETTINGS_FILE.read_text()
    except Exception:
        return False


class ClaudeActivityMonitor:
    def __init__(self):
        self._cache_mtime = None
        self._cache_data   = {}
        if not _hooks_installed():
            print("  [claude-activity] hooks not installed — working-session dots "
                  "will stay empty. Run ./install_claude_activity_hooks.sh to enable them.")

    def start(self):
        pass  # no background work needed — reads are cheap enough per-tick

    def stop(self):
        pass

    def working_count(self) -> int:
        data = self._load()
        now  = time.time()
        return sum(
            1 for entry in data.values()
            if entry.get("state") == "active"
            and now - entry.get("updated_at", 0) < ACTIVITY_STALE_SECS
        )

    # ------------------------------------------------------------------
    def _load(self) -> dict:
        try:
            mtime = os.path.getmtime(STATUS_FILE)
        except OSError:
            self._cache_mtime = None
            self._cache_data   = {}
            return self._cache_data

        if mtime != self._cache_mtime:
            try:
                self._cache_data = json.loads(STATUS_FILE.read_text())
            except Exception:
                self._cache_data = {}
            self._cache_mtime = mtime

        return self._cache_data
