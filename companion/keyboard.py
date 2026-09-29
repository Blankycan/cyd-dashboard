"""
Keyboard monitor — evdev (Wayland/Hyprland) with pynput fallback.

Tracks two things: whether the keyboard is currently active (for the status
dot), and how many keys were pressed today. The daily count is persisted to
KEYS_STATE_FILE so a companion restart mid-day doesn't reset it to zero.

Requires membership in the 'input' group for evdev access:
  sudo usermod -aG input $USER   (log out and back in to apply)
"""

import json
import selectors
import threading
import time
from datetime import date

from config import IDLE_AFTER, KEYS_STATE_FILE, KEYS_SAVE_INTERVAL


class KeyboardMonitor:
    def __init__(self):
        self._lock         = threading.Lock()
        self._last_press   = 0.0
        self._thread       = None
        self._running      = False
        self._day          = date.today().isoformat()
        self._keys_today   = 0
        self._last_save    = 0.0
        self._load()

    # ---- public API -------------------------------------------------------

    def start(self):
        self._running = True
        self._thread  = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False
        with self._lock:
            self._save()

    def keys_today(self) -> int:
        """Keypresses since local midnight."""
        with self._lock:
            self._rollover()
            if time.monotonic() - self._last_save >= KEYS_SAVE_INTERVAL:
                self._save()
            return self._keys_today

    def is_active(self) -> bool:
        """True if a key was pressed within IDLE_AFTER seconds."""
        with self._lock:
            return (time.monotonic() - self._last_press) < IDLE_AFTER

    # ---- internal ---------------------------------------------------------

    def _press(self):
        with self._lock:
            self._last_press = time.monotonic()
            self._rollover()
            self._keys_today += 1

    def _rollover(self):
        """Reset the daily count once the local date changes (call under lock)."""
        today = date.today().isoformat()
        if today != self._day:
            self._day        = today
            self._keys_today = 0

    def _load(self):
        try:
            d = json.loads(KEYS_STATE_FILE.read_text())
            if d.get("date") == self._day:
                self._keys_today = int(d.get("count", 0))
        except Exception:
            pass  # missing or corrupt — start today from zero

    def _save(self):
        """Persist today's count (call under lock). Best-effort."""
        self._last_save = time.monotonic()
        try:
            KEYS_STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
            tmp = KEYS_STATE_FILE.with_suffix(".tmp")
            tmp.write_text(json.dumps({"date": self._day, "count": self._keys_today}))
            tmp.replace(KEYS_STATE_FILE)
        except Exception:
            pass

    def _run(self):
        try:
            self._evdev_loop()
        except Exception as e:
            print(f"  [keyboard] evdev unavailable ({e}), falling back to pynput")
            try:
                self._pynput_loop()
            except Exception as e2:
                print(f"  [keyboard] pynput also failed ({e2}), keyboard tracking disabled")

    def _evdev_loop(self):
        import evdev
        from evdev import ecodes

        # Re-open every keyboard device on a timer rather than once at
        # startup. A device that re-enumerates (USB replug, suspend/resume,
        # Bluetooth reconnect) gets a fresh backing instance while our old
        # fd goes silently deaf — no exception, it just stops delivering
        # events — so periodic reopen is what actually detects that instead
        # of requiring a service restart.
        RESCAN_INTERVAL = 30  # seconds

        sel = selectors.DefaultSelector()
        registered = {}  # path -> InputDevice

        def close_all():
            for dev in registered.values():
                try:
                    sel.unregister(dev)
                except Exception:
                    pass
                try:
                    dev.close()
                except Exception:
                    pass
            registered.clear()

        def rescan():
            close_all()
            for path in evdev.list_devices():
                try:
                    dev = evdev.InputDevice(path)
                    if ecodes.EV_KEY not in dev.capabilities():
                        dev.close()
                        continue
                    sel.register(dev, selectors.EVENT_READ, path)
                    registered[path] = dev
                except Exception:
                    pass  # device unreadable or vanished mid-scan

        rescan()
        if not registered:
            raise RuntimeError("no keyboard devices found")

        try:
            last_rescan = time.monotonic()
            while self._running:
                for key, _ in sel.select(timeout=0.25):
                    try:
                        for event in key.fileobj.read():
                            if event.type == ecodes.EV_KEY and event.value == 1:
                                self._press()
                    except Exception:
                        pass  # device disconnected mid-read; next rescan heals it

                now = time.monotonic()
                if now - last_rescan >= RESCAN_INTERVAL:
                    rescan()
                    last_rescan = now
        finally:
            close_all()

    def _pynput_loop(self):
        from pynput import keyboard

        with keyboard.Listener(on_press=lambda _: self._press()):
            while self._running:
                time.sleep(0.1)
