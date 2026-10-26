"""
Media monitor — polls playerctl (MPRIS2) for now-playing info.
Works on Wayland/Hyprland via D-Bus without an X server.

Requires playerctl: sudo pacman -S playerctl
"""

import json
import subprocess
import threading
import time

from config import MEDIA_POLL_INTERVAL
from text_utils import to_displayable
FIELD_SEP     = "|||"
FORMAT_STR    = (f"{{{{title}}}}{FIELD_SEP}{{{{artist}}}}{FIELD_SEP}{{{{status}}}}"
                 f"{FIELD_SEP}{{{{album}}}}{FIELD_SEP}{{{{mpris:artUrl}}}}")


class MediaMonitor:
    def __init__(self):
        self._lock    = threading.Lock()
        self._info    = None   # dict {title, artist, playing, album, art_url} or None
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
        """Return {title, artist, playing, album, art_url} or None if nothing is
        playing. album and art_url may be empty; only the track card uses them."""
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
        try:
            raw = subprocess.check_output(
                ["playerctl", "-p", player, "metadata", "--format", FORMAT_STR],
                stderr=subprocess.DEVNULL,
                timeout=2,
            ).decode().strip()
            parts = raw.split(FIELD_SEP)
        except subprocess.CalledProcessError:
            # playerctl rejects players whose mpris:trackid isn't a D-Bus
            # object path (Spotify 1.3+ sends a plain string), so read the
            # properties straight off the bus instead.
            parts = self._query_dbus(player)
        if len(parts) < 5:
            return None

        title, artist, status, album, art_url = (p.strip() for p in parts[:5])
        if not title:
            return None

        return {
            "title":   to_displayable(title),
            "artist":  to_displayable(artist),
            "playing": status == "Playing",
            "album":   to_displayable(album),
            "art_url": art_url,
        }

    @staticmethod
    def _is_shadow(player: str, players: list[str]) -> bool:
        """Electron/CEF apps (Spotify 1.3+) also expose their embedded
        Chromium as "chromium.instance<pid>", with worse metadata (artist
        folded into the title, art as a temp file). Skip it when the process
        behind it already has a player of its own name in the list."""
        name, _, inst = player.partition(".instance")
        if name != "chromium" or not inst.isdigit():
            return False
        try:
            with open(f"/proc/{inst}/comm") as f:
                comm = f.read().strip()
        except OSError:
            return False
        return comm != name and comm in players

    def _query_dbus(self, player: str) -> list[str]:
        out = subprocess.check_output(
            ["busctl", "--user", "-j", "get-property",
             f"org.mpris.MediaPlayer2.{player}", "/org/mpris/MediaPlayer2",
             "org.mpris.MediaPlayer2.Player", "Metadata", "PlaybackStatus"],
            stderr=subprocess.DEVNULL,
            timeout=2,
        ).decode().strip().splitlines()
        meta   = {k: v["data"] for k, v in json.loads(out[0])["data"].items()}
        status = json.loads(out[1])["data"]
        artist = meta.get("xesam:artist", [])
        if isinstance(artist, list):
            artist = ", ".join(artist)
        return [meta.get("xesam:title", ""), artist, status,
                meta.get("xesam:album", ""), meta.get("mpris:artUrl", "")]

    def _poll(self) -> tuple[dict | None, str | None]:
        try:
            players = subprocess.check_output(
                ["playerctl", "-l"],
                stderr=subprocess.DEVNULL,
                timeout=2,
            ).decode().strip().splitlines()
            players = [p.strip() for p in players if p.strip()]
            players = [p for p in players if not self._is_shadow(p, players)]
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
