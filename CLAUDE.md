# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Two programs that talk over USB serial:

- **`src/`** — ESP32 firmware (Arduino framework, PlatformIO, LVGL UI) that runs on
  a CYD ("Cheap Yellow Display", ESP32-2432S028R) board and renders the dashboard.
- **`companion/`** — a Python app that runs on the host PC, gathers stats (CPU/RAM,
  keyboard WPM, now-playing media, Claude token usage), and pushes them to the board
  as a JSON line every `INTERVAL` seconds (`companion/config.py`).

There is no build step linking the two — they're independent programs that agree on
a JSON wire format (see Protocol below). Changing one side's expectations means
updating the other by hand; nothing enforces the schema.

## Commands

```bash
# Firmware: build / flash (PlatformIO; use the full path, `pio` is usually not on PATH)
~/.platformio/penv/bin/pio run
~/.platformio/penv/bin/pio run --target upload                      # auto-detects /dev/ttyUSB0
~/.platformio/penv/bin/pio run --target upload --upload-port /dev/ttyUSB1

# Companion: run directly (auto-detects serial port)
cd companion && python main.py
python main.py --port /dev/ttyUSB1

# Companion: install/manage as a systemd user service
./install_companion_as_service.sh          # idempotent; re-run after changing deps/venv path
systemctl --user restart cyd-dashboard     # after plain code edits to companion/
journalctl --user -u cyd-dashboard -f
./uninstall_companion_as_service.sh        # stop + disable + remove the unit file

# Companion: opt-in Claude Code hooks (enables the Claude panel's working-session dots)
./install_claude_activity_hooks.sh         # idempotent; edits ~/.claude/settings.json machine-wide
./uninstall_claude_activity_hooks.sh       # removes only the cyd-dashboard hook entries
```

There is no test suite and no linter configured for either side — nothing to run
beyond the build/flash commands above.

**Flashing conflicts with the running service.** The companion service holds the
serial port open, so `pio run --target upload` will fail while it's running:

```bash
systemctl --user stop cyd-dashboard
~/.platformio/penv/bin/pio run --target upload
systemctl --user start cyd-dashboard
```

## Architecture

### Firmware (`src/`)

`main.cpp` is the entry point: it owns the single global `DashState state`
(`state.h`), initializes TFT/LVGL/touch, builds the panel tree once in
`build_dashboard()`, then in `loop()` reads one JSON line per iteration from
`Serial`, decodes it with ArduinoJson, mutates `state`, and calls each widget's
`update_*_ui()`. Widgets never touch `Serial` or JSON directly — they only read
`state` and expose `build_*_panel()` / `update_*_ui()`, called from `main.cpp`.

`layout.h` holds screen geometry (240×320, panel heights, hardware pins) as
`#define`s — pure constants, no logic. `state.h` is the `DashState` struct shared
between the packet handler and every widget's update function.

Panels live under `src/widgets/`, stacked top-to-bottom by `build_panels()` in
`main.cpp`: `topbar` (clock/date) → `music` (now-playing + animated icon) →
`system` (CPU/RAM/WPM bars) → `claude` (token counts, rate-limit bars, and opt-in working-session dots) →
`status` (connection dot + IP). Each widget pairs a `.h`/`.cpp`: the `.h`
declares `build_<name>_panel()` and `update_<name>_ui()`; the `.cpp` builds LVGL
objects once and mutates them in place on update (no rebuilding widgets per
packet). `ui_helpers.h/cpp` has the shared factories used across panels —
notably `make_bar_row()`, which builds the common "label + value + bar +
optional reset countdown" row used by the system/claude panels.

When disconnected (`DISCONNECT_TIMEOUT_MS` with no packet), `main.cpp` zeroes
out `state` and calls every widget's `update_*_ui()` directly rather than
waiting for the next packet — see `show_disconnected()`. Sleep (backlight dim +
overlay after `SLEEP_TIMEOUT_MS`) is handled entirely in `main.cpp`, independent
of the widget layer.

**Colour themes** are the one place firmware code is meant to be edited for pure
customization. `theme.h` `#include`s whichever `src/themes/<name>.h` is selected
by `CYD_THEME` in `config.h`. Each theme file is self-contained: it defines
`PAL_*` raw hex colours, then derives `COL_*` (LVGL) and `TFT_COL_*` (raw
RGB565, for non-LVGL drawing) macros from them. `theme.h` also defines fallback
`#ifndef` defaults for every per-widget `COL_<WIDGET>_*` token — a new theme only
needs to override the tokens it wants to diverge from the defaults (see
`neon-rose.h` or `grape-ember.h` for examples of Level-3 per-widget overrides
like inverted topbar colours or a custom accent on one specific icon). To add a
theme: copy `forest.h`, edit its `PAL_*` values, add a `CYD_THEME_<NAME>` id in
`config.h`, and add the `#include` branch in `theme.h`. `rainbow.h` is a
diagnostic theme that assigns a distinct colour to every single token, useful
for spotting anything that fell back to an unintended default.

### Companion app (`companion/`)

`main.py` is the serial loop: `collect_stats()` gathers everything into one dict
each tick, `json.dumps` + newline over `pyserial`, then a small ANSI-coloured
one-line status printout (`theme.py` has the terminal colour codes, unrelated to
the firmware's LVGL themes). Each data source is an independent poller module
constructed once in `main()` and read from on each tick — they run their own
background thread/state and `collect_stats()` never blocks on them:

- `keyboard.py` — `KeyboardMonitor` watches evdev (`/dev/input/`) for keypresses
  and computes WPM over a rolling window; also derives `active`/idle for the
  status dot.
- `media.py` — `MediaMonitor` polls `playerctl` (MPRIS2) for now-playing info.
- `claude_tokens.py` — `ClaudeTokenMonitor` scans `~/.claude/projects/**/*.jsonl`
  for today's token usage, and separately calls `api.anthropic.com` (reading
  rate-limit response headers, not the response body) to get 5-hour/7-day usage
  percentages. Auth is whichever of `~/.claude/.credentials.json` (OAuth from a
  `claude` login) or `ANTHROPIC_API_KEY` is available; if neither, rate-limit
  fields are omitted and the firmware hides that section.
- `claude_activity.py` — `ClaudeActivityMonitor`, unlike the others, needs no
  background thread: it just reads (mtime-cached) the small status file that
  Claude Code itself keeps current via hooks (`hooks/session_touch.py`,
  registered by `install_claude_activity_hooks.sh`), counting sessions
  currently mid-turn. This is opt-in — if the hooks were never installed the
  status file never appears, `working_count()` stays 0, and a one-line warning
  is printed once at startup.

`config.py` holds the tunables (poll `INTERVAL`, idle-message rotation
interval, WPM window). `idle_messages.txt` is the pool of strings shown in the
music panel when nothing is playing — one message per line, `#`-prefixed lines
ignored.

### Protocol

One JSON object per line, newline-delimited, 115200 baud. Firmware boots with
`{"boot":true,"version":"..."}`; each stats packet is `{"type":"stats", cpu,
ram, wpm, time, date, active, ip, idle_msg?, music?, claude?}` — see
`handle_packet()` in `main.cpp` for the authoritative field list and defaults,
and `collect_stats()` in `companion/main.py` for how it's assembled. The
firmware acks each packet with `{"ack":true}` but the companion doesn't depend
on it (non-blocking read, ignored on timeout). If you add a field to one side,
update the other by hand — there's no shared schema.

The `claude` sub-object's `working` field (count of sessions currently
mid-turn, from `claude_activity.py`) is unrelated to its `sessions` field
(count of distinct sessions with token activity *today*, from
`claude_tokens.py`) — same object, two different counting mechanisms, kept
as separate fields (`state.claude_working` vs `state.claude_sessions` in
`state.h`) rather than merged.
