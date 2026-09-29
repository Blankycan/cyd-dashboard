#!/usr/bin/env python3
# CYD Dashboard companion — stats + keystrokes + media + Claude usage
# Run: python companion/main.py [--port /dev/ttyUSBx]

import argparse
import glob
import json
import random
import socket
import termios
import time
from datetime import datetime
from pathlib import Path

import psutil
import serial
import serial.tools.list_ports

from claude_activity import ClaudeActivityMonitor
from claude_tokens import ClaudeTokenMonitor
from config import INTERVAL, IDLE_MSG_INTERVAL
from keyboard import KeyboardMonitor
from media import MediaMonitor
from theme import ansi

BAUD       = 115_200
ESP32_VIDS = {0x1A86, 0x10C4, 0x303A, 0x0403, 0x067B}

# ---------------------------------------------------------------------------
# Idle messages (shown in music panel when nothing is playing)
# ---------------------------------------------------------------------------
_IDLE_FILE  = Path(__file__).parent / "idle_messages.txt"
_IDLE_MSGS: list[str] = []
_idle_msg   = ""
_idle_msg_t = 0.0


def _load_idle_messages() -> None:
    global _IDLE_MSGS
    try:
        lines = _IDLE_FILE.read_text().splitlines()
        _IDLE_MSGS = [l.strip() for l in lines
                      if l.strip() and not l.startswith("#")]
    except Exception:
        _IDLE_MSGS = ["in the vibe zone"]


def _get_idle_msg() -> str:
    global _idle_msg, _idle_msg_t
    if not _idle_msg or time.time() - _idle_msg_t > IDLE_MSG_INTERVAL:
        candidates = [m for m in _IDLE_MSGS if m != _idle_msg] or _IDLE_MSGS
        _idle_msg   = random.choice(candidates) if candidates else ""
        _idle_msg_t = time.time()
    return _idle_msg


def get_local_ip() -> str:
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("8.8.8.8", 80))
            return s.getsockname()[0]
    except Exception:
        return ""


def find_port() -> str | None:
    for p in serial.tools.list_ports.comports():
        if p.vid in ESP32_VIDS:
            return p.device
    candidates = sorted(glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*"))
    return candidates[0] if candidates else None


def collect_stats(kb: KeyboardMonitor | None = None,
                  media: MediaMonitor | None = None,
                  claude_tok: ClaudeTokenMonitor | None = None,
                  claude_activity: ClaudeActivityMonitor | None = None) -> dict:
    m   = media.get()      if media      else None
    tok = claude_tok.get() if claude_tok else None
    stats: dict = {
        "type":   "stats",
        "cpu":    int(psutil.cpu_percent()),
        "ram":    int(psutil.virtual_memory().percent),
        "time":   datetime.now().strftime("%H:%M"),
        "date":   datetime.now().strftime("%a %d %b"),
        "keys":   kb.keys_today() if kb else 0,
        "active": kb.is_active() if kb else False,
        "ip":     get_local_ip(),
    }
    if m:
        stats["music"] = m
    else:
        stats["idle_msg"] = _get_idle_msg()
    if tok:
        tok = dict(tok)  # sessions, h5_pct, h5_secs, w7_pct, w7_secs
        tok["working"] = claude_activity.working_count() if claude_activity else 0
        stats["claude"] = tok
    return stats


def pct_color(pct: int) -> str:
    if pct >= 90: return "alert"
    if pct >= 70: return "warn"
    return "ok"


def print_stats(s: dict) -> None:
    cpu_c   = pct_color(s["cpu"])
    ram_c   = pct_color(s["ram"])
    cpu_str = f"{s['cpu']:>3}%"
    ram_str = f"{s['ram']:>3}%"
    act     = s.get("active", False)
    keys    = s.get("keys", 0)
    act_c   = "ok" if act else "text_dim"
    act_str = "ACT" if act else "IDL"
    keys_str = f"{keys / 1000:.1f}k" if keys >= 1000 else str(keys)
    m       = s.get("music")
    if m:
        play_c   = "ok" if m["playing"] else "text_dim"
        play_sym = ">" if m["playing"] else "#"
        title    = m["title"][:28]
        artist   = m["artist"][:20]
        music_s  = f"{ansi(play_c, play_sym)} {ansi('text_pri', title)} {ansi('text_dim', artist)}"
    else:
        music_s = ansi("text_dim", "no media")
    tok = s.get("claude")
    if tok:
        def fmt_secs(secs):
            if secs < 0:  return "--"
            if secs < 60: return "< 1m"
            m = secs // 60; h = m // 60; d = h // 24
            if d > 0:     return f"{d}d {h % 24}h"
            if h > 0:     return f"{h}h {m % 60}m"
            return f"{m}m"
        def pct_c(p):
            if p < 0:   return "text_dim"
            if p >= 90: return "alert"
            if p >= 70: return "warn"
            return "ok"
        tok_tok = f"{tok['sessions']} sess"
        if tok.get("working"):
            working_lbl = f"{tok['working']} working"
            tok_tok += f"  {ansi('ok', working_lbl)}"
        h5, w7  = tok["h5_pct"], tok["w7_pct"]
        if h5 >= 0 or w7 >= 0:
            tok_rl = (
                f"5h {ansi(pct_c(h5), f'{h5}%' if h5 >= 0 else '--')} "
                f"{ansi('text_dim', fmt_secs(tok['h5_secs']))}  "
                f"7d {ansi(pct_c(w7), f'{w7}%' if w7 >= 0 else '--')} "
                f"{ansi('text_dim', fmt_secs(tok['w7_secs']))}"
            )
            tok_s = f"{ansi('glow', tok_tok)}   {tok_rl}"
        else:
            tok_s = ansi("glow", tok_tok)
    else:
        tok_s = ""
    print(
        f"  {ansi('text_pri', s['time'])}   "
        f"CPU {ansi(cpu_c, cpu_str)}   "
        f"RAM {ansi(ram_c, ram_str)}   "
        f"{ansi(act_c, act_str)}  "
        f"{ansi('text_sec', keys_str)} keys   "
        f"{music_s}"
        + (f"   {tok_s}" if tok_s else "")
    )


RECONNECT_DELAY    = 5
BOOT_WARMUP_DELAY  = 10  # seconds


def run_session(port: str, kb, media, claude_tok, claude_activity,
                 force_warmup_reset: bool = False) -> None:
    """Connect to `port` and stream stats until the link drops or errors out."""
    print()
    print(ansi("cap", "  CYD Dashboard  -  companion"))
    print(ansi("text_dim", f"  {port} @ {BAUD} baud"))
    print()

    # Open with DTR=False and RTS=False to minimise reset-circuit triggering.
    # On some USB-serial chips (CH340) a brief RTS pulse still slips through
    # during open regardless, so we also wait for the firmware boot message
    # before sending stats — that way packets never arrive mid-setup().
    ser = serial.Serial()
    ser.port     = port
    ser.baudrate = BAUD
    ser.timeout  = 1
    ser.dtr      = False
    ser.rts      = False
    ser.open()
    # Disable HUPCL so the kernel doesn't assert DTR/RTS on port close.
    attrs = termios.tcgetattr(ser.fd)
    attrs[2] &= ~termios.HUPCL
    termios.tcsetattr(ser.fd, termios.TCSANOW, attrs)

    try:
        # Wait for the firmware's JSON boot line, draining ROM garbage in between.
        # Timeout after 4 s so we don't block forever if the board was already
        # running (no reset occurred, no boot message will come).
        firmware_ver = None
        deadline = time.time() + 4.0
        while time.time() < deadline:
            line = ser.readline()
            if not line:
                break
            txt = line.decode("utf-8", errors="replace").strip()
            try:
                b = json.loads(txt)
                if b.get("boot"):
                    firmware_ver = b.get("version", "?")
                    break
            except Exception:
                pass  # ROM / non-JSON line — discard silently

        if firmware_ver:
            print(ansi("ok", f"  Connected  -  firmware v{firmware_ver}\n"))
        else:
            print(ansi("text_dim", "  Board already running — no boot message\n"))

        session_start = time.time()
        warmup_pending = force_warmup_reset

        no_ack_streak = 0
        while True:
            # On a fresh boot, the board's first tft.init() can silently fail
            # if it races the host's own USB/power settling right after the PC
            # powers on — the ESP32 stays fully alive and keeps acking over
            # serial, but the screen never lights up. A restart used to be the
            # only fix because reopening the port pulses DTR/RTS and resets
            # the board, giving tft.init() a second try once things have
            # calmed down. Do that one reconnect automatically instead of
            # waiting for a human to notice a black screen and do it by hand.
            if warmup_pending and time.time() - session_start > BOOT_WARMUP_DELAY:
                print(ansi("text_dim",
                           "  Forcing one reconnect to recover from a possible "
                           "cold-boot display glitch...\n"))
                return

            stats = collect_stats(kb, media, claude_tok, claude_activity)
            ser.write((json.dumps(stats) + "\n").encode())
            print_stats(stats)

            # Drain any output from the board (non-blocking).
            # Log anything that isn't a normal ack — firmware panics appear here.
            ser.timeout = 0.1
            raw = ser.readline()
            ser.timeout = 1.0
            if raw:
                txt = raw.decode("utf-8", errors="replace").strip()
                try:
                    if json.loads(txt).get("ack"):
                        no_ack_streak = 0
                    else:
                        print(ansi("warn", f"  ESP32: {txt}"))
                except Exception:
                    print(ansi("alert", f"  ESP32: {txt}"))
            else:
                no_ack_streak += 1
                if no_ack_streak == 3:
                    print(ansi("warn", "  ESP32 not acking — board may be frozen"))

            time.sleep(INTERVAL)
    finally:
        ser.close()


def main():
    parser = argparse.ArgumentParser(description="CYD Dashboard companion")
    parser.add_argument("--port", help="Serial port (auto-detected if omitted)")
    args = parser.parse_args()

    _load_idle_messages()

    # Prime psutil — first cpu_percent() call always returns 0.0
    psutil.cpu_percent()
    time.sleep(0.5)

    kb = KeyboardMonitor()
    kb.start()

    media = MediaMonitor()
    media.start()

    claude_tok = ClaudeTokenMonitor()
    claude_tok.start()

    claude_activity = ClaudeActivityMonitor()
    claude_activity.start()

    try:
        # Loop forever: a missing board at startup (USB not yet enumerated —
        # common right after boot) or a serial error mid-session (board reset,
        # USB hiccup) just falls back into this loop and retries rather than
        # exiting, so a full boot race never needs a manual service restart.
        first_connection = True
        while True:
            port = args.port or find_port()
            if not port:
                print(ansi("alert", "  No ESP32 found. Retrying..."))
                time.sleep(RECONNECT_DELAY)
                continue

            try:
                run_session(port, kb, media, claude_tok, claude_activity,
                            force_warmup_reset=first_connection)
            except serial.SerialException as e:
                print(ansi("alert", f"\n  Serial error: {e}"))
            first_connection = False

            print(ansi("text_dim", f"  Reconnecting in {RECONNECT_DELAY}s...\n"))
            time.sleep(RECONNECT_DELAY)
    finally:
        kb.stop()
        media.stop()
        claude_tok.stop()
        claude_activity.stop()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(ansi("text_dim", "\n  Stopped."))
