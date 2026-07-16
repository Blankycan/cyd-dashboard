#!/usr/bin/env python3
"""
Claude Code hook script — marks a session as active/idle for the CYD
Dashboard's working-session dots. Registered by install_claude_activity_hooks.sh
against the UserPromptSubmit ("start") and Stop ("stop") hook events.

Stdlib only — this runs inside Claude Code's own hook execution, not the
companion's venv, so it can't assume any third-party packages are installed.

Usage: session_touch.py start|stop   (session_id read from stdin JSON)
"""
import fcntl
import json
import os
import sys
import time
from pathlib import Path

STATUS_FILE  = Path.home() / ".cache" / "cyd-dashboard" / "claude_sessions.json"
LOCK_FILE    = STATUS_FILE.with_suffix(".lock")
PRUNE_AFTER  = 24 * 60 * 60  # drop entries older than this on every write


def _read_session_id() -> str | None:
    try:
        data = json.loads(sys.stdin.read())
        return data.get("session_id")
    except Exception:
        return None


def _prune(data: dict, now: float) -> dict:
    return {sid: entry for sid, entry in data.items()
            if now - entry.get("updated_at", 0) < PRUNE_AFTER}


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("start", "stop"):
        print("usage: session_touch.py start|stop", file=sys.stderr)
        sys.exit(0)  # never block the user's session over a usage error

    action     = sys.argv[1]
    session_id = _read_session_id()
    if not session_id:
        sys.exit(0)

    STATUS_FILE.parent.mkdir(parents=True, exist_ok=True)
    now = time.time()

    # Lock a stable sidecar file, not STATUS_FILE itself — STATUS_FILE gets
    # replaced (new inode) on every write, so locking it directly would let a
    # waiter's already-open fd go stale across another writer's replace and
    # read pre-update data. The lock file's identity never changes, so every
    # waiter that acquires it is guaranteed to freshly re-read STATUS_FILE
    # strictly after the previous holder's replace() completed.
    lock_fd = os.open(LOCK_FILE, os.O_RDWR | os.O_CREAT, 0o644)
    try:
        fcntl.flock(lock_fd, fcntl.LOCK_EX)
        try:
            data = {}
            if STATUS_FILE.exists():
                try:
                    data = json.loads(STATUS_FILE.read_text())
                except Exception:
                    data = {}
            data = _prune(data, now)
            if action == "start":
                data[session_id] = {"state": "active", "updated_at": now}
            else:
                data.pop(session_id, None)

            tmp = STATUS_FILE.with_suffix(".tmp")
            tmp.write_text(json.dumps(data))
            os.replace(tmp, STATUS_FILE)
        finally:
            fcntl.flock(lock_fd, fcntl.LOCK_UN)
    except Exception:
        pass  # never fail the user's Claude Code turn over dashboard bookkeeping
    finally:
        os.close(lock_fd)

    sys.exit(0)


if __name__ == "__main__":
    main()
