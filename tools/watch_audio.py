#!/usr/bin/env python3
"""Print the audio events (beat / au) the companion would send to the board.

Runs the companion's own media and audio monitors without opening the serial
port, so it's safe alongside the running service. Only prints while a player
is actually playing.

    .venv/bin/python tools/watch_audio.py        # Ctrl+C to stop
    .venv/bin/python tools/watch_audio.py 10     # or run for 10 seconds
"""
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "companion"))

from media import MediaMonitor           # noqa: E402
from audio import AudioEnergyMonitor     # noqa: E402

t0 = time.monotonic()


def show(ev: dict) -> None:
    print(f"{time.monotonic() - t0:7.2f}s  {json.dumps(ev)}", flush=True)


media = MediaMonitor()
media.start()
AudioEnergyMonitor(media, show).start()

try:
    time.sleep(float(sys.argv[1]) if len(sys.argv) > 1 else 1e9)
except KeyboardInterrupt:
    pass
