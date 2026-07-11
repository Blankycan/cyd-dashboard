"""
Media monitor — polls playerctl (MPRIS2) for now-playing info.
Works on Wayland/Hyprland via D-Bus without an X server.

Requires playerctl: sudo pacman -S playerctl
"""

import subprocess
import threading
import time

from config import MEDIA_POLL_INTERVAL
FIELD_SEP     = "|||"
FORMAT_STR    = f"{{{{title}}}}{FIELD_SEP}{{{{artist}}}}{FIELD_SEP}{{{{status}}}}"


class MediaMonitor:
    def __init__(self):
        self._lock    = threading.Lock()
        self._info    = None   # dict {title, artist, playing} or None
        self._running = False
        self._thread  = None

    def start(self):
        self._running = True
        self._thread  = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False

    def get(self) -> dict | None:
        """Return {title, artist, playing} or None if nothing is playing."""
        with self._lock:
            return self._info

    def _run(self):
        while self._running:
            info = self._poll()
            with self._lock:
                self._info = info
            time.sleep(MEDIA_POLL_INTERVAL)

    def _poll(self) -> dict | None:
        try:
            raw = subprocess.check_output(
                ["playerctl", "metadata", "--format", FORMAT_STR],
                stderr=subprocess.DEVNULL,
                timeout=2,
            ).decode().strip()

            parts = raw.split(FIELD_SEP)
            if len(parts) < 3:
                return None

            title, artist, status = parts[0].strip(), parts[1].strip(), parts[2].strip()
            if not title:
                return None

            return {
                "title":   title,
                "artist":  artist,
                "playing": status == "Playing",
            }
        except Exception:
            return None
