"""
Media monitor — polls playerctl (MPRIS2) for now-playing info.
Works on Wayland/Hyprland via D-Bus without an X server.

Requires playerctl: sudo pacman -S playerctl
Requires unidecode: pip install unidecode
"""

import subprocess
import threading
import time

from unidecode import unidecode

from config import MEDIA_POLL_INTERVAL
FIELD_SEP     = "|||"
FORMAT_STR    = f"{{{{title}}}}{FIELD_SEP}{{{{artist}}}}{FIELD_SEP}{{{{status}}}}"


def _to_displayable(text: str) -> str:
    """Fold characters the firmware's font can't render down to ASCII.

    The firmware font covers Basic Latin + Latin-1 Supplement (U+0000-U+00FF),
    which includes accented letters like é, ñ, Å, Ä, Ö, ü — those pass through
    untouched. Anything past that (e.g. stylized "small caps" Unicode some
    titles use) gets transliterated to its closest ASCII equivalent instead of
    showing as a missing/wrong glyph.
    """
    return "".join(c if ord(c) <= 0xFF else unidecode(c) for c in text)


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
            "title":   _to_displayable(title),
            "artist":  _to_displayable(artist),
            "playing": status == "Playing",
        }

    def _poll(self) -> dict | None:
        try:
            players = subprocess.check_output(
                ["playerctl", "-l"],
                stderr=subprocess.DEVNULL,
                timeout=2,
            ).decode().strip().splitlines()
            players = [p.strip() for p in players if p.strip()]
            if not players:
                return None

            # Multiple players can be registered at once (e.g. a paused
            # browser tab left in the background). Prefer whichever one is
            # actually playing over the first player playerctl happens to
            # list, falling back to the first player with any metadata.
            fallback = None
            for player in players:
                try:
                    info = self._query(player)
                except Exception:
                    continue
                if info is None:
                    continue
                if info["playing"]:
                    return info
                if fallback is None:
                    fallback = info
            return fallback
        except Exception:
            return None
