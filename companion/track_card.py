"""
Track announcer — tells the board when a new song starts, with its cover art,
for the firmware's "new song" card.

Watches the media monitor; when a different track is playing it sends

  {"type":"track","id":N,"t":title,"a":artist,"al":album,"w":ART_SIZE,"h":ART_SIZE}

followed by the cover as RGB565 (little-endian, row by row) in base64 chunks

  {"type":"art","id":N,"o":byte_offset,"d":"..."}

paced so they never outrun the board's 2 KB serial buffer. "w":0 means there
is no cover and nothing follows. A new track mid-transfer abandons the old one
(the id tells the board which chunks belong together).

Cover art comes from the player's mpris:artUrl (https:// for Spotify,
file:// for browsers), scaled with Pillow. Without Pillow the card still shows,
just without the cover.
"""

import base64
import io
import struct
import threading
import time
from collections import OrderedDict
from urllib.parse import unquote, urlparse

from config import TRACK_CARD_ENABLED
from text_utils import fit_utf8

ART_SIZE    = 80     # px square — must not exceed TRACK_ART_MAX in the firmware's config.h
CHUNK_BYTES = 768    # raw bytes per art line (1 KB of base64)
CHUNK_GAP_S = 0.11   # between art lines: ~9 KB/s, under the 11.5 KB/s link
POLL_S      = 0.5

# Firmware buffer sizes, minus the terminator
MAX_TITLE, MAX_ARTIST, MAX_ALBUM = 63, 47, 47


def _fetch_bytes(url: str) -> bytes:
    if url.startswith("file://"):
        with open(unquote(urlparse(url).path), "rb") as f:
            return f.read()
    import requests
    r = requests.get(url, timeout=5)
    r.raise_for_status()
    return r.content


def _to_rgb565(data: bytes, Image) -> bytes:
    img = Image.open(io.BytesIO(data)).convert("RGB")
    w, h = img.size
    s = min(w, h)   # centre-crop to a square
    img = img.crop(((w - s) // 2, (h - s) // 2, (w - s) // 2 + s, (h - s) // 2 + s))
    img = img.resize((ART_SIZE, ART_SIZE), Image.LANCZOS)
    px = img.tobytes()
    out = bytearray()
    for i in range(0, len(px), 3):
        r, g, b = px[i], px[i + 1], px[i + 2]
        out += struct.pack("<H", ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
    return bytes(out)


class TrackAnnouncer:
    def __init__(self, media, emit):
        self._media   = media
        self._emit    = emit
        self._running = False
        self._image   = None
        self._cache: "OrderedDict[str, bytes]" = OrderedDict()   # art URL -> RGB565
        self._id      = 0
        if not TRACK_CARD_ENABLED:
            return
        try:
            from PIL import Image
            self._image = Image
        except ImportError:
            print("  [track] Pillow not installed — song card without cover art (pip install pillow)")

    def start(self):
        if not TRACK_CARD_ENABLED:
            return
        self._running = True
        threading.Thread(target=self._run, daemon=True).start()

    def stop(self):
        self._running = False

    # ------------------------------------------------------------------
    @staticmethod
    def _key(m) -> tuple | None:
        return (m["title"], m["artist"]) if m and m.get("playing") else None

    def _run(self):
        last = None
        while self._running:
            m = self._media.get()
            key = self._key(m)
            if key and key != last:
                last = key
                try:
                    self._announce(m)
                except Exception as e:
                    print(f"  [track] {e}")
            time.sleep(POLL_S)

    def _art(self, url: str) -> bytes | None:
        if not url or self._image is None:
            return None
        if url in self._cache:
            self._cache.move_to_end(url)
            return self._cache[url]
        try:
            art = _to_rgb565(_fetch_bytes(url), self._image)
        except Exception as e:
            print(f"  [track] cover art: {e}")
            return None
        self._cache[url] = art
        while len(self._cache) > 16:
            self._cache.popitem(last=False)
        return art

    def _announce(self, m: dict):
        key = self._key(m)
        art = self._art(m.get("art_url", ""))
        if self._key(self._media.get()) != key:
            return   # skipped on while the cover downloaded
        self._id = (self._id + 1) % 1000
        size = ART_SIZE if art else 0
        self._emit({"type": "track", "id": self._id,
                    "t":  fit_utf8(m["title"], MAX_TITLE),
                    "a":  fit_utf8(m["artist"], MAX_ARTIST),
                    "al": fit_utf8(m.get("album", ""), MAX_ALBUM),
                    "w": size, "h": size})
        if not art:
            return
        for off in range(0, len(art), CHUNK_BYTES):
            time.sleep(CHUNK_GAP_S)
            if not self._running or self._key(self._media.get()) != key:
                return   # the next track takes over
            self._emit({"type": "art", "id": self._id, "o": off,
                        "d": base64.b64encode(art[off:off + CHUNK_BYTES]).decode()})
