"""
Keyboard monitor — evdev (Wayland/Hyprland) with pynput fallback.

Requires membership in the 'input' group for evdev access:
  sudo usermod -aG input $USER   (log out and back in to apply)
"""

import selectors
import threading
import time
from collections import deque

# Rolling window for WPM calculation (seconds)
WPM_WINDOW = 10.0
# Seconds without a keypress before state switches to idle
IDLE_AFTER  = 2.5
# EMA smoothing factor (0 = frozen, 1 = raw)
WPM_ALPHA   = 0.35


class KeyboardMonitor:
    def __init__(self):
        self._lock         = threading.Lock()
        self._timestamps   = deque()  # monotonic times of recent keypresses
        self._last_press   = 0.0
        self._smooth_wpm   = 0.0
        self._thread       = None
        self._running      = False

    # ---- public API -------------------------------------------------------

    def start(self):
        self._running = True
        self._thread  = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False

    def wpm(self) -> int:
        """Current smoothed WPM, decayed toward 0 when idle."""
        with self._lock:
            self._recalc(time.monotonic())
            return int(self._smooth_wpm)

    def is_active(self) -> bool:
        """True if a key was pressed within IDLE_AFTER seconds."""
        with self._lock:
            return (time.monotonic() - self._last_press) < IDLE_AFTER

    # ---- internal ---------------------------------------------------------

    def _press(self):
        now = time.monotonic()
        with self._lock:
            self._last_press = now
            self._timestamps.append(now)
            self._recalc(now)

    def _recalc(self, now: float):
        """Prune stale timestamps and update smoothed WPM (call under lock)."""
        cutoff = now - WPM_WINDOW
        while self._timestamps and self._timestamps[0] < cutoff:
            self._timestamps.popleft()

        count   = len(self._timestamps)
        raw_wpm = (count / 5.0) / (WPM_WINDOW / 60.0)

        # Adaptive alpha: converge faster on large changes
        delta = abs(raw_wpm - self._smooth_wpm)
        alpha = min(WPM_ALPHA + delta * 0.006, 0.85)
        self._smooth_wpm = alpha * raw_wpm + (1.0 - alpha) * self._smooth_wpm

    def _run(self):
        try:
            self._evdev_loop()
        except Exception as e:
            print(f"  [keyboard] evdev unavailable ({e}), falling back to pynput")
            try:
                self._pynput_loop()
            except Exception as e2:
                print(f"  [keyboard] pynput also failed ({e2}), WPM disabled")

    def _evdev_loop(self):
        import evdev
        from evdev import ecodes

        all_devs = [evdev.InputDevice(p) for p in evdev.list_devices()]
        keyboards = [
            d for d in all_devs
            if ecodes.EV_KEY in d.capabilities()
        ]
        if not keyboards:
            raise RuntimeError("no keyboard devices found")

        sel = selectors.DefaultSelector()
        for dev in keyboards:
            try:
                sel.register(dev, selectors.EVENT_READ)
            except Exception:
                pass  # device may have vanished

        while self._running:
            ready = sel.select(timeout=0.25)
            for key, _ in ready:
                try:
                    for event in key.fileobj.read():
                        if event.type == ecodes.EV_KEY and event.value == 1:
                            self._press()
                except Exception:
                    pass  # device disconnected mid-read

    def _pynput_loop(self):
        from pynput import keyboard

        with keyboard.Listener(on_press=lambda _: self._press()):
            while self._running:
                time.sleep(0.1)
