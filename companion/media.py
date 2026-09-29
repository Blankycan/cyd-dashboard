"""
Media monitor — polls playerctl (MPRIS2) for now-playing info.
Works on Wayland/Hyprland via D-Bus without an X server.

Requires playerctl: sudo pacman -S playerctl
"""

import subprocess
import threading
import time

from config import MEDIA_POLL_INTERVAL
from text_utils import to_displayable
FIELD_SEP     = "|||"
FORMAT_STR    = f"{{{{title}}}}{FIELD_SEP}{{{{artist}}}}{FIELD_SEP}{{{{status}}}}"


class MediaMonitor:
    def __init__(self):
        self._lock    = threading.Lock()
        self._info    = None   # dict {title, artist, playing} or None
        self._player  = None   # playerctl name of the player _info came from
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

    def player(self) -> str | None:
        """playerctl name of the current player (e.g. "spotify",
        "firefox.instance_1_42"), or None. Lets the audio monitor find which
        audio output the music is actually playing on."""
        with self._lock:
            return self._player

    def _run(self):
        while self._running:
            info, player = self._poll()
            with self._lock:
                self._info   = info
                self._player = player
            time.sleep(MEDIA_POLL_INTERVAL)

    def _query(self, player: str) -> dict | None:
        raw = subprocess.check_output(
            ["playerctl", "-p", player, "metadata", "--format", FORMAT_STR],
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
            "title":   to_displayable(title),
            "artist":  to_displayable(artist),
            "playing": status == "Playing",
        }

    def _poll(self) -> tuple[dict | None, str | None]:
        try:
            players = subprocess.check_output(
                ["playerctl", "-l"],
                stderr=subprocess.DEVNULL,
                timeout=2,
            ).decode().strip().splitlines()
            players = [p.strip() for p in players if p.strip()]
            if not players:
                return None, None

            # Multiple players can be registered at once (e.g. a paused
            # browser tab left in the background). Prefer whichever one is
            # actually playing over the first player playerctl happens to
            # list, falling back to the first player with any metadata.
            fallback = (None, None)
            for player in players:
                try:
                    info = self._query(player)
                except Exception:
                    continue
                if info is None:
                    continue
                if info["playing"]:
                    return info, player
                if fallback[0] is None:
                    fallback = (info, player)
            return fallback
        except Exception:
            return None, None
