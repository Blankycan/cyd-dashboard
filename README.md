# CYD Dashboard

A desk companion for the **ESP32-2432S028R** ("Cheap Yellow Display") — a 2.8 inch
240×320 touchscreen that shows live stats from your PC: system usage, typing speed,
now-playing music, Claude AI usage, and connection status.

```
┌────────────────────────┐
│ 14:32          Sat 12  │  ← clock + date
├────────────────────────┤
│ ♫  Lofi Hip Hop Radio  │  ← now-playing (artist scrolls)
│    chill beats          │
├────────────────────────┤
│ CPU  43%  ██████░░░░░  │
│ RAM  61%  █████████░░  │  ← system stats
│ WPM  72   ████████░░░  │
├────────────────────────┤
│ claude   1.2k out today│
│ ████████████░░░░░░░░░  │  ← Claude token usage
│ in 8.4k    3 sess      │
├────────────────────────┤
│                        │
│                        │  ← blank space (future widget)
│                        │
├────────────────────────┤
│ ● active    192.168.1.5│  ← connection status + host IP
└────────────────────────┘
```

The status dot cycles between **offline** (red), **idle** (dim), and **active** (green)
based on whether the companion app is connected and whether the keyboard is in use.
The Claude panel rotates between token counts and rate-limit bars every 8 seconds when
API auth is available. The display dims after 5 minutes of inactivity and shows a
minimal clock with a zzz animation.

---

## Platform

The **firmware** runs on the ESP32 and is platform-independent.

The **companion app** runs on the host PC and has the following requirements:

- **Linux** — keyboard monitoring uses evdev (`/dev/input/*`), which is Linux-specific.
  A pynput fallback exists for macOS/Windows but is untested.
- **Wayland** recommended — media info is fetched via `playerctl` (MPRIS2/D-Bus),
  which works on both Wayland and X11. The keyboard monitor uses evdev directly, so
  it works without X11.
- **`input` group membership** required for evdev keyboard access:
  ```
  sudo usermod -aG input $USER
  ```
  Log out and back in to apply.

---

## How it works

The companion app runs on your PC and sends a JSON packet over USB serial every 2
seconds. The ESP32 parses it and updates the display.

**Data sources:**

| Panel | Source |
|---|---|
| Clock / date | `datetime.now()` on the host |
| CPU / RAM | `psutil` |
| WPM | evdev keypress timestamps, rolling 10 s window |
| Music | `playerctl metadata` (MPRIS2) |
| Claude tokens | Scans `~/.claude/projects/**/*.jsonl` for today's usage |
| Claude rate limits | Single minimal API call to `api.anthropic.com` (reads response headers) |
| Host IP | `socket` — routes toward 8.8.8.8 to pick the right interface |
| Connection status | Derived: connected = receiving packets; active = keyboard used recently |

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

```
pip install psutil pyserial evdev
```

`playerctl` for music info (optional):

```
# Arch
sudo pacman -S playerctl

# Debian / Ubuntu
sudo apt install playerctl
```

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

Create `~/.config/systemd/user/cyd-dashboard.service`:

```ini
[Unit]
Description=CYD Dashboard companion
After=network.target

[Service]
WorkingDirectory=/path/to/cyd-dashboard/companion
ExecStart=/usr/bin/python main.py
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
```

Replace `/path/to/cyd-dashboard` with the actual path, then:

```bash
systemctl --user daemon-reload
systemctl --user enable --now cyd-dashboard
systemctl --user status cyd-dashboard
```

To view logs:

```bash
journalctl --user -u cyd-dashboard -f
```

---

## Colour palette

All colours live in `src/theme.h`. Edit the `PAL_*` defines there to retheme
the entire firmware.

| Role | Hex | Description |
|---|---|---|
| Background | `#0F1F1A` | Very dark forest |
| Panel | `#162820` | Slightly lighter cards |
| Primary text | `#FEFAE0` | Warm cream |
| Secondary text | `#81B29A` | Sage green |
| Dim text | `#4A7B6F` | Muted sage |
| OK / connected | `#81B29A` | Sage green |
| Warning (>70%) | `#F2CC8F` | Amber |
| Alert (>90%) | `#E07A5F` | Terracotta |
| Accent / glow | `#E9C46A` | Warm gold |

---

## Project structure

```
companion/          Host-side Python app
  main.py           Entry point — serial loop, packet assembly
  keyboard.py       evdev keypress monitor + WPM calculation
  media.py          playerctl MPRIS2 poller
  claude_tokens.py  JSONL scanner + API rate-limit fetcher
  idle_messages.txt Rotating messages shown when no music is playing

src/                ESP32 firmware (Arduino / PlatformIO)
  main.cpp          Hardware init, sleep overlay, packet handler, setup/loop
  layout.h          All panel size and timing constants
  state.h           Shared DashState struct populated from serial packets
  theme.h           Colour palette
  ui_helpers.h/cpp  Shared LVGL widget factories and formatters
  widgets/
    topbar.*        Clock and date bar
    music.*         Now-playing panel with animated icon
    stats.*         CPU, RAM, and WPM bars
    claude.*        Token usage and rate-limit panels (rotates)
    indicator.*     Connection status dot, label, and host IP
```
