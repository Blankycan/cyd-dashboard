# CYD Dashboard

A desk companion for the **ESP32-2432S028R** ("Cheap Yellow Display") — a 2.8 inch
240×320 touchscreen that shows live stats from your PC: system usage, typing speed,
now-playing music, Claude AI usage, and connection status.

```
┌────────────────────────┐
│ 14:32       Sat 12 Jul │  ← clock + date
├────────────────────────┤
│ ● Lofi Hip Hop Radio   │  ← now-playing (artist scrolls)
│   chill beats        ♫ │
├────────────────────────┤
│ CPU  43%  ██████░░░░░  │
│ RAM  61%  █████████░░  │  ← system stats
│ WPM  72   ████████░░░  │
├────────────────────────┤
│ ● claude    1.2k out   │
│             today      │  ← token counts
│ ████████████░░░░░░░░░  │
│ in 8.4k      3 sess    │
│ ·······················│
│ 5h  23%    resets 2h4m │  ← rate-limit bars
│ █████░░░░░░░░░░░░░░░░  │
│ 7d   8%    resets 4d3h │
│ ██░░░░░░░░░░░░░░░░░░░  │
├────────────────────────┤
│ ● active   192.168.1.5 │  ← connection status + host IP
└────────────────────────┘
```

The status dot cycles between **offline** (red), **idle** (dim), and **active** (green)
based on whether the companion app is connected and whether the keyboard is in use.
Rate-limit bars show `--` when no Claude auth is configured. The display dims after
5 minutes of inactivity and shows a minimal clock with a zzz animation.

---

## Platform

The **firmware** runs on the ESP32 and is platform-independent.

The **companion app** runs on the host PC and has the following requirements:

- **Linux** — keyboard monitoring uses evdev (`/dev/input/`*), which is Linux-specific.
A pynput fallback exists for macOS/Windows but is untested.
- **Wayland** recommended — media info is fetched via `playerctl` (MPRIS2/D-Bus),
which works on both Wayland and X11. The keyboard monitor uses evdev directly, so
it works without X11.
- `**input` group membership** required for evdev keyboard access:
  ```
  sudo usermod -aG input $USER
  ```
  Log out and back in to apply.

---

## How it works

The companion app runs on your PC and sends a JSON packet over USB serial every 2
seconds. The ESP32 parses it and updates the display.

**Data sources:**


| Panel              | Source                                                                  |
| ------------------ | ----------------------------------------------------------------------- |
| Clock / date       | `datetime.now()` on the host                                            |
| CPU / RAM          | `psutil`                                                                |
| WPM                | evdev keypress timestamps, rolling 10 s window                          |
| Music              | `playerctl metadata` (MPRIS2)                                           |
| Claude tokens      | Scans `~/.claude/projects/**/*.jsonl` for today's usage                 |
| Claude rate limits | Single minimal API call to `api.anthropic.com` (reads response headers) |
| Host IP            | `socket` — routes toward 8.8.8.8 to pick the right interface            |
| Connection status  | Derived: connected = receiving packets; active = keyboard used recently |


**Claude auth** (for rate-limit bars) uses whichever is available first:

- OAuth token from `~/.claude/.credentials.json` (Claude Code login, no setup needed)
- `ANTHROPIC_API_KEY` environment variable

If neither is present, the rate-limit view is hidden and only token counts are shown.

---

## Hardware

- **ESP32-2432S028R** — any variant with ILI9341 display + XPT2046 touch
- USB-C or micro-USB cable to host PC
- No soldering required

---

## Dependencies

### Firmware (managed by PlatformIO)

Declared in `platformio.ini` — installed automatically on first build:

- `bodmer/TFT_eSPI`
- `lvgl/lvgl ^8.3`
- `bblanchon/ArduinoJson ^7`
- `PaulStoffregen/XPT2046_Touchscreen`

### Companion app (Python 3.10+)

Use a virtualenv so dependencies stay isolated from system Python:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install psutil pyserial evdev
```

`playerctl` for music info (optional):

```
# Arch
sudo pacman -S playerctl

# Debian / Ubuntu
sudo apt install playerctl
```

Remember to `source .venv/bin/activate` again in any new shell before running
`main.py` directly. This also matters when installing the systemd service
(see below) — the install script picks up whatever `python3` is first on
`PATH`, so activate the venv before running it if you want the service to use
the venv's interpreter and packages.

---

## Building and flashing the firmware

Install [PlatformIO](https://platformio.org/install/cli), then:

```bash
# Build
~/.platformio/penv/bin/pio run

# Flash (auto-detects /dev/ttyUSB0)
~/.platformio/penv/bin/pio run --target upload
```

The upload speed is set to 115200 baud in `platformio.ini` for reliable flashing.
If flashing fails on the first attempt, retry — the ESP32 occasionally needs a second
try to enter bootloader mode cleanly.

`platformio.ini` hardcodes `upload_port = /dev/ttyUSB0`. If your board enumerates
on a different port (see [Serial port](#serial-port) below), override it on the
command line instead of editing the file:

```bash
~/.platformio/penv/bin/pio run --target upload --upload-port /dev/ttyUSB1
```

**If the companion app is running as a systemd service** (see
[Installing the companion as a systemd service](#installing-the-companion-as-a-systemd-service)),
it holds the serial port open and the upload will fail to connect. Stop it
first, flash, then start it again:

```bash
systemctl --user stop cyd-dashboard
~/.platformio/penv/bin/pio run --target upload
systemctl --user start cyd-dashboard
```

---

## Serial port

The CYD's USB-serial chip is usually a **CH340** (USB VID `1a86`), sometimes a
CP2102 (`10c4`) or similar. On Linux it shows up as `/dev/ttyUSB0`,
`/dev/ttyUSB1`, etc. — the exact number depends on enumeration order and can
shift between reboots or when other USB-serial devices are plugged in.

To find it:

```bash
# Shows a stable symlink plus the current ttyUSB target
ls -la /dev/serial/by-id/

# Or list connected USB devices and look for the serial chip
lsusb | grep -iE 'ch340|cp210|silicon|qinheng'
```

**Permissions** — your user needs access to the serial device's group, which
varies by distro:

```bash
# Debian / Ubuntu (dialout group)
sudo usermod -aG dialout $USER

# Arch (uucp group)
sudo usermod -aG uucp $USER
```

Log out and back in (or `newgrp <group>`) to apply. Without this, both
`pio run --target upload` and the companion app will fail to open the port
(`could not open port` / `Permission denied`).

The companion app auto-detects the port by first matching known ESP32 USB
vendor IDs, falling back to the first `/dev/ttyUSB*` or `/dev/ttyACM*` found
(see `find_port()` in `companion/main.py`). If you have multiple USB-serial
devices connected, pass `--port` explicitly to avoid ambiguity.

---

## Running the companion app

```bash
cd companion
python main.py
```

The port is auto-detected. To specify it manually:

```bash
python main.py --port /dev/ttyUSB1
```

The terminal shows a live status line with CPU, RAM, WPM, and now-playing info.

---

## Installing the companion as a systemd service

The easiest way is the included install script, which auto-detects the repo
path and Python binary:

```bash
./install_companion_as_service.sh
```

This creates the service file, enables it, and starts it immediately. It also
prints the commands for checking status, tailing logs, stopping, and removing.

Manual setup

Create `~/.config/systemd/user/cyd-dashboard.service`:

```ini
[Unit]
Description=CYD Dashboard companion
After=network.target

[Service]
WorkingDirectory=/path/to/cyd-dashboard/companion
ExecStart=/usr/bin/python3 main.py
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
```

Replace `/path/to/cyd-dashboard` with the actual path, then:

```bash
systemctl --user daemon-reload
systemctl --user enable --now cyd-dashboard
```



To view logs:

```bash
journalctl --user -u cyd-dashboard -f
```

**After changing `companion/` code**, the service needs to pick up the change —
`systemctl --user restart cyd-dashboard` is enough for plain code edits. If you
changed dependencies or moved the venv, re-run `./install_companion_as_service.sh`
instead (it's idempotent) so the service file's `ExecStart` interpreter path
stays correct.

---

## Colour themes

Each theme is a standalone palette file under `src/themes/`. `src/theme.h` is
a small selector that pulls in whichever one is active — pick it by editing
`CYD_THEME` in `src/config.h`:

```c
#define CYD_THEME  CYD_THEME_GRAPE_EMBER
```

then rebuild and reflash. Available themes:

| `CYD_THEME` value        | File                        | Look                                                    |
| ------------------------- | ---------------------------- | -------------------------------------------------------- |
| `CYD_THEME_FOREST`        | `src/themes/forest.h`        | Warm dark forest green, sage/cream text                 |
| `CYD_THEME_GRAPE_EMBER`   | `src/themes/grape-ember.h`   | Deep plum/violet, rose-pink text, orange accents         |
| `CYD_THEME_NEON_ROSE`     | `src/themes/neon-rose.h`     | Black background, hot-pink topbar and accents            |
| `CYD_THEME_RAINBOW`       | `src/themes/rainbow.h`       | Diagnostic — every colour token distinct (see below)      |

**Forest** — `src/themes/forest.h`:

| Role           | Hex       | Description            |
| -------------- | --------- | ---------------------- |
| Background     | `#0F1F1A` | Very dark forest       |
| Panel          | `#162820` | Slightly lighter cards |
| Primary text   | `#FEFAE0` | Warm cream             |
| Secondary text | `#81B29A` | Sage green             |
| Dim text       | `#4A7B6F` | Muted sage             |
| OK / connected | `#81B29A` | Sage green             |
| Warning (>70%) | `#F2CC8F` | Amber                  |
| Alert (>90%)   | `#E07A5F` | Terracotta             |
| Accent / glow  | `#E9C46A` | Warm gold              |

**Grape Ember** — `src/themes/grape-ember.h`:

| Role           | Hex       | Description         |
| -------------- | --------- | -------------------- |
| Background     | `#270C24` | Deep plum             |
| Panel          | `#3F173B` | Dark magenta-violet   |
| Border         | `#922A8A` | Vivid magenta         |
| Primary text   | `#EC8DB8` | Rose pink             |
| Secondary text | `#C29AD6` | Soft lavender         |
| Dim text       | `#AA5098` | Muted mauve           |
| OK / connected | `#E05CE0` | Vivid violet          |
| Warning (>70%) | `#FFB829` | Golden amber          |
| Alert (>90%)   | `#FF4D6D` | Vivid coral-red       |
| Accent / glow  | `#FF8A3D` | Vivid orange          |

The topbar inverts to the primary rose-pink as its background (dark plum/violet
text for contrast), and the music icons, Claude token number/bar, and the
generic "active" dot colour all use the orange accent instead of the violet
OK colour — see the Level-3 overrides at the bottom of `grape-ember.h`.

**Neon Rose** — `src/themes/neon-rose.h`:

| Role           | Hex       | Description          |
| -------------- | --------- | ---------------------- |
| Background     | `#16000F` | Very dark rose          |
| Panel          | `#FF3399` | Hot pink (topbar bg)     |
| Border         | `#BB0055` | Bright cerise            |
| Primary text   | `#FF3399` | Hot pink                 |
| Secondary text | `#FF88BB` | Light pink               |
| Dim text       | `#FF6699` | Bright rose              |
| OK / connected | `#FF1493` | Deep pink                |
| Warning (>70%) | `#FFAA00` | Golden amber             |
| Alert (>90%)   | `#FF2244` | Vivid red-pink           |
| Accent / glow  | `#FF007F` | Hot rose                 |

The topbar sits on the bright pink panel colour, so its clock/date text use
dedicated dark overrides for contrast rather than the generic primary/dim text.

**Rainbow** — `src/themes/rainbow.h` — a diagnostic theme, not meant to look
good. Every single colour token is set to a distinct hue so any widget that
silently falls back to an unintended default is immediately visible. See the
comment block at the top of the file for which hue band each widget owns.

**Adding a new theme**: copy `src/themes/forest.h` to `src/themes/<name>.h`,
edit its `PAL_*` defines, add a `CYD_THEME_<NAME>` id in `config.h`, and add a
matching `#include` branch in `theme.h`.

---

## Project structure

```
companion/          Host-side Python app
  config.py         User-tunable settings (intervals, WPM tuning)
  main.py           Entry point — serial loop, packet assembly
  keyboard.py       evdev keypress monitor + WPM calculation
  media.py          playerctl MPRIS2 poller
  claude_tokens.py  JSONL scanner + API rate-limit fetcher
  idle_messages.txt Rotating messages shown when no music is playing

src/                ESP32 firmware (Arduino / PlatformIO)
  config.h          User-tunable settings (timeouts, brightness, token bar ceiling)
  layout.h          Panel geometry and hardware wiring constants
  state.h           Shared DashState struct populated from serial packets
  theme.h           Colour theme selector (includes the active themes/*.h)
  themes/           Standalone colour palettes — see Colour themes above
    forest.h        Warm dark forest green (original)
    grape-ember.h   Deep plum/violet, orange accents
    neon-rose.h     Black background, hot-pink topbar and accents
    rainbow.h       Diagnostic — every colour token distinct
  ui_helpers.h/cpp  Shared LVGL widget factories and formatters
  main.cpp          Hardware init, sleep overlay, packet handler, setup/loop
  widgets/
    topbar.*        Clock and date bar
    music.*         Now-playing panel with animated icon
    system.*        CPU, RAM, and WPM bars
    claude.*        Token usage and rate-limit panels
    status.*        Connection dot, label, and host IP
```

