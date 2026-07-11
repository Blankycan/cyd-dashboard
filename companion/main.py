#!/usr/bin/env python3
# CYD Dashboard companion — Phase 5: stats + WPM + media + idle messages
# Run: python companion/main.py [--port /dev/ttyUSBx]

import argparse
import glob
import json
import random
import sys
import time
from datetime import datetime
from pathlib import Path

import psutil
import serial
import serial.tools.list_ports

from claude_tokens import ClaudeTokenMonitor
from keyboard import KeyboardMonitor
from media import MediaMonitor
from theme import ansi

BAUD       = 115_200
INTERVAL   = 2.0   # seconds between stat packets

ESP32_VIDS = {0x1A86, 0x10C4, 0x303A, 0x0403, 0x067B}

# ---------------------------------------------------------------------------
# Idle messages (shown in music panel when nothing is playing)
# ---------------------------------------------------------------------------
_IDLE_FILE  = Path(__file__).parent / "idle_messages.txt"
_IDLE_MSGS: list[str] = []
_idle_msg   = ""
_idle_msg_t = 0.0
IDLE_MSG_INTERVAL = 30.0   # seconds before rotating to next message


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


def find_port() -> str | None:
    for p in serial.tools.list_ports.comports():
        if p.vid in ESP32_VIDS:
            return p.device
    candidates = sorted(glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*"))
    return candidates[0] if candidates else None


def collect_stats(kb: KeyboardMonitor | None = None,
                  media: MediaMonitor | None = None,
                  claude_tok: ClaudeTokenMonitor | None = None) -> dict:
    m   = media.get()      if media      else None
    tok = claude_tok.get() if claude_tok else None
    stats: dict = {
        "type":   "stats",
        "cpu":    int(psutil.cpu_percent()),
        "ram":    int(psutil.virtual_memory().percent),
        "time":   datetime.now().strftime("%H:%M"),
        "date":   datetime.now().strftime("%a %d %b"),
        "wpm":    kb.wpm() if kb else 0,
        "active": kb.is_active() if kb else False,
    }
    if m:
        stats["music"] = m
    else:
        stats["idle_msg"] = _get_idle_msg()
    if tok:
        stats["claude"] = tok  # {out, inp, sessions, h5_pct, h5_secs, w7_pct, w7_secs}
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
    wpm     = s.get("wpm", 0)
    act_c   = "ok" if act else "text_dim"
    act_str = "ACT" if act else "IDL"
    wpm_str = f"{wpm:>3}"
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
        out_k = tok["out"] / 1000.0
        tok_tok = f"{out_k:.1f}k out  {tok['sessions']} sess"
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
        f"{ansi('text_sec', wpm_str)} WPM   "
        f"{music_s}"
        + (f"   {tok_s}" if tok_s else "")
    )


def main():
    parser = argparse.ArgumentParser(description="CYD Dashboard companion")
    parser.add_argument("--port", help="Serial port (auto-detected if omitted)")
    args = parser.parse_args()

    port = args.port or find_port()
    if not port:
        print(ansi("alert", "  No ESP32 found. Check USB or pass --port /dev/ttyUSBx"))
        sys.exit(1)

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

    print()
    print(ansi("cap", "  CYD Dashboard  -  companion"))
    print(ansi("text_dim", f"  {port} @ {BAUD} baud"))
    print()

    try:
        ser = serial.Serial(port, BAUD, timeout=1)
    except serial.SerialException as e:
        print(ansi("alert", f"  Cannot open {port}: {e}"))
        sys.exit(1)

    time.sleep(2.0)  # wait for ESP32 boot

    # Drain boot message
    boot = ser.readline()
    if boot:
        try:
            b = json.loads(boot.decode().strip())
            ver = b.get("version", "?")
            print(ansi("ok", f"  Connected  -  firmware v{ver}\n"))
        except Exception:
            print(ansi("text_dim", f"  Boot: {boot.decode().strip()}\n"))

    try:
        while True:
            stats = collect_stats(kb, media, claude_tok)
            ser.write((json.dumps(stats) + "\n").encode())
            print_stats(stats)

            # Drain any ack (non-blocking — we don't depend on it)
            ser.timeout = 0.1
            ser.readline()
            ser.timeout = 1.0

            time.sleep(INTERVAL)

    except serial.SerialException as e:
        print(ansi("alert", f"\n  Serial error: {e}"))
    finally:
        kb.stop()
        media.stop()
        claude_tok.stop()
        ser.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(ansi("text_dim", "\n  Stopped."))
