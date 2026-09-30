# CYD Dashboard

A desk companion for the **ESP32-2432S028R** ("Cheap Yellow Display") — a 2.8 inch
240×320 touchscreen that shows live stats from your PC: your next meeting,
system usage, keystrokes, now-playing music, Claude AI usage, and connection status.

```
┌────────────────────────┐
│ 14:32       Sat 12 Jul │  ← clock + date
├────────────────────────┤
│ 14:30 Standup   in 12m │  ← next meeting + countdown
│ then 16:00 1:1 w/ Kim  │
│ ▁██▁▁▁│▁▁▁▁▁██▁▁▁▁▁▁▁  │  ← today's timeline, │ = now
│ 7  9  11  13  15       │
├────────────────────────┤
│ ● Lofi Hip Hop Radio   │  ← now-playing (artist scrolls)
│   chill beats        ♫ │
├────────────────────────┤
│ CPU  43%  ██████░░░░░  │  ← system stats
│ RAM  61%  █████████░░  │
├────────────────────────┤
│ ● claude 3 sessions ●● │  ← sessions today + working dots
│ 5h  23%          2h 4m │  ← rate-limit bars
│ █████░░░░░░░░░░░░░░░░  │
│ 7d   8%          4d 3h │
│ ██░░░░░░░░░░░░░░░░░░░  │
├────────────────────────┤
│ ● idle 14:20 ⌨12k .1.5 │  ← status, keystrokes today, host IP
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
- **`input` group membership** required for evdev keyboard *and mouse* access:
  ```
  sudo usermod -aG input $USER
  ```
  A plain log out/in is often **not enough** to apply this — the systemd
  `--user` manager that runs the companion service is a long-lived process
  that can survive a logout (it's tied to the user, not the session), so it
  keeps its stale group list until it's actually restarted. **Reboot** to be
  sure, then confirm with `id $USER` that `input` is listed.

  Don't try to work around this with a `SupplementaryGroups=input` line in
  the systemd unit — unprivileged `--user` services can't call `setgroups()`
  (`CAP_SETGID` is required, even to set a group the manager already has),
  so that directive just crash-loops the service
  (`status=216/GROUP`/`Changing group credentials failed`). Once the account
  is actually in the group and the manager has restarted, a plain unit with
  no group directives inherits it automatically — no unit changes needed.

  If wake-on-keypress/click still doesn't work after that, don't assume the
  group fix failed — a keyboard/mouse can expose *several* `/dev/input/eventN`
  nodes (e.g. a split/QMK keyboard commonly registers separate nodes for
  keys, mouse-emulation, System Control and Consumer Control), and it's
  logind's dynamic per-device ACLs — not group membership — that can tag only
  *some* of those nodes as accessible while leaving the others (including the
  actual key-matrix one) `Permission denied`. That's exactly what static
  `input` group membership sidesteps once it's actually in effect. To check
  which devices the running service can see:
  ```
  ls -l /proc/$(systemctl --user show cyd-dashboard -p MainPID --value)/fd | grep input
  ```

---

## How it works

The companion app runs on your PC and sends a JSON packet over USB serial every 2
seconds. The ESP32 parses it and updates the display.

**Data sources:**


| Panel              | Source                                                                  |
| ------------------ | ----------------------------------------------------------------------- |
| Clock / date       | `datetime.now()` on the host                                            |
| Calendar           | Google Calendar API (read-only) — see [Setting up the calendar](#setting-up-the-calendar) |
| CPU / RAM          | `psutil`                                                                |
| Keystrokes today   | evdev keypress count since local midnight (survives restarts)           |
| Music              | `playerctl metadata` (MPRIS2)                                           |
| Claude sessions    | Scans `~/.claude/projects/**/*.jsonl` for sessions active today         |
| Claude rate limits | Single minimal API call to `api.anthropic.com` (reads response headers) |
| Claude working     | Opt-in Claude Code hooks — see [Enabling Claude working-session dots](#enabling-claude-working-session-dots) |
| Host IP            | `socket` — routes toward 8.8.8.8 to pick the right interface            |
| Connection status  | Derived: connected = receiving packets; active = keyboard used recently |


**Claude auth** (for rate-limit bars) uses whichever is available first:

- OAuth token from `~/.claude/.credentials.json` (Claude Code login, no setup needed)
- `ANTHROPIC_API_KEY` environment variable

If neither is present, the rate-limit bars show `--`.

### Serial protocol

One JSON object per line (newline-terminated), 115200 baud, sent every
`INTERVAL` seconds (2 s by default, `companion/config.py`). The firmware
announces itself on boot with `{"boot":true,"version":"..."}` and acks every
stats packet with `{"ack":true}`; the companion logs a warning if acks stop
but doesn't otherwise depend on them.

```json
{"type":"stats","cpu":3,"ram":25,"time":"15:02","date":"Tue 29 Sep",
 "keys":4120,"active":true,"ip":"10.0.0.12","idle_msg":"...",
 "music":{"title":"...","artist":"...","playing":true},
 "claude":{"sessions":4,"working":1,"h5_pct":34,"h5_secs":7800,"w7_pct":12,"w7_secs":356400},
 "cal":[[720,780,"Lunch"],[990,1050,"Sprint planning"]]}
```

Between stats packets the companion also sends short **live event** lines the
moment they happen, for the ambient scenes (none of these are acked):

```json
{"type":"key","k":"c"}                     key-press category: c char, s space, e enter,
                                           b backspace/delete, m modifier, o other
{"type":"beat","s":72}                     a beat in the playing music, strength 1-100
{"type":"au","i":55,"b":40,"bpm":122}      music intensity / bass (0-100) and tempo, ~10 per second
```

Only the category of a key is ever sent — never which key — so nothing you
type (passwords included) leaves the PC. `beat`/`au` come from the optional
audio energy detection; see [Ambient scenes](#ambient-scenes).

`music`, `idle_msg`, `claude`, and `cal` are optional — each is only sent when
its source has data, and the firmware keeps its defaults otherwise. The
authoritative field list is `handle_packet()` in `src/main.cpp`; the
companion side is `collect_stats()` in `companion/main.py`. Nothing enforces
the schema, so a field added on one side has to be added to the other by hand.

**Calendar data.** `cal` is today's timed events as
`[start_min, end_min, title]`, with times in minutes since local midnight
(16:30 → `990`), sorted by start. The companion fetches from Google every 5
minutes (`CALENDAR_POLL_INTERVAL`) but resends the latest list in every
packet, which keeps the firmware stateless about it and costs only a few
percent of the link. Countdowns aren't sent — the firmware derives the
current / next meeting and every countdown from its own clock, so they keep
ticking between packets and while the companion is offline. At most 12
events (`CAL_MAX_EVENTS` in `src/config.h`, `MAX_EVENTS` in
`companion/gcal.py`) are sent, with titles trimmed to 39 bytes of UTF-8 to fit
the firmware's buffer.

**Receive buffer.** At 115200 baud bytes arrive at roughly 11.5 per
millisecond, and `loop()` only reads serial between LVGL redraws, which can
take tens of milliseconds. The ESP32's default 256-byte UART receive buffer
would overflow on a long packet mid-redraw (the JSON then fails to parse and
that packet is silently dropped), so `setup()` raises it to 2 KB with
`Serial.setRxBufferSize()` — enough for a worst-case ~1 KB packet with a full
calendar.

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
pip install psutil pyserial evdev unidecode

# Only needed for beat/tempo detection in the ambient scenes
pip install numpy

# Only needed for the calendar panel
pip install google-auth google-auth-oauthlib requests
```

`playerctl` for music info (optional — the music panel just shows "no media"
without it):

```
# Arch
sudo pacman -S playerctl

# Debian / Ubuntu
sudo apt install playerctl
```

**Troubleshooting:** if the music panel stops updating after a distro/OS
upgrade (e.g. an Omarchy version bump), check that `playerctl` is still
installed — `which playerctl`. Upgrades can silently drop it as a package
dependency even though nothing in this repo changed.

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

The terminal shows a live status line with CPU, RAM, keystrokes, and now-playing info.

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

To remove the service:

```bash
./uninstall_companion_as_service.sh
```

Stops and disables the service and removes its unit file. The companion app
itself isn't touched — you can still run it directly (see above). Safe to
re-run; does nothing if the service was never installed.

---

## Setting up the calendar

The calendar panel under the clock shows your current or next meeting with a
live countdown, the meeting after that, and a timeline of the day. It reads
Google Calendar through the official API with **read-only** access. This
works even when your Google Workspace admin has disabled "Secret address in
iCal format", and a single login covers every calendar that account can see
(for example a private calendar shared into your work account).

Without this set up the panel just says "no calendar"; nothing else is
affected.

### 1. Create a Google Cloud OAuth client (once per Google account)

In the [Google Cloud Console](https://console.cloud.google.com/), signed in
with the account whose calendars you want:

1. **Create a project**, e.g. `cyd-dashboard`.
2. **Enable the Google Calendar API** (APIs & Services → Library →
   "Google Calendar API" → Enable).
3. **Configure the OAuth consent screen** (Google Auth Platform → Get started).
   Any app name and your own email for the contact fields. For a Workspace
   account pick **Internal** as the audience — only your organisation can use
   it and Google doesn't need to review it. A personal Gmail account has to
   use External and add itself as a test user.
4. **Create an OAuth client** (Clients → Create client → Application type
   **Desktop app**) and click **Download JSON** before closing the dialog.

Save that file as `~/.config/cyd-dashboard/google_client.json` and keep it
private:

```bash
mkdir -p ~/.config/cyd-dashboard && chmod 700 ~/.config/cyd-dashboard
mv ~/Downloads/client_secret_*.json ~/.config/cyd-dashboard/google_client.json
chmod 600 ~/.config/cyd-dashboard/google_client.json
```

### 2. Log in (once per machine)

```bash
./setup_calendar.sh
```

This installs the Google API packages into the companion's Python (the one
the systemd service uses, if it's installed), runs the login, shows which
calendars will be used, and restarts the service. It's safe to re-run; it
skips the login if a token already exists (`--relogin` forces a new one).
The login step on its own is `cd companion && python gcal_auth.py`.

A browser tab opens asking for read-only calendar access; pick the account
and click **Allow**. A refresh token is saved to
`~/.config/cyd-dashboard/google_token.json` (owner-only), and the companion
renews access from it by itself from then on. Re-run this only if the token
is revoked (e.g. from your Google account's security settings).

### 3. Pick which calendars to show

By default only the logged-in account's own calendar is shown. To merge in
others, list them one per line in `~/.config/cyd-dashboard/calendars.txt`
(`primary` means the account's own calendar; `#` starts a comment):

```
primary
someone@gmail.com   # private calendar shared into this account
```

It lives outside the repo because calendar IDs are often email addresses.
To see every calendar the account can read, with the IDs to put there:

```bash
cd companion && python gcal.py
```

Changes are picked up on the next fetch (within 5 minutes), no restart
needed. Events that appear on several calendars (the same meeting on
work and private) are shown once; all-day, cancelled, and declined events are
skipped.

### Tuning (firmware, `src/config.h`)

| Setting | Default | Meaning |
|---|---|---|
| `CAL_DAY_START_H` / `CAL_DAY_END_H` | `7` / `16` | Default timeline span in hours. It widens automatically to fit any meeting outside it. |
| `CAL_SOON_MIN` | `5` | Minutes before a meeting when the countdown turns to the "soon" colour and pulses. |

Every calendar colour is a `COL_CALENDAR_*` token in `src/theme.h`, so themes
can restyle it like any other panel.

### On another computer

The OAuth client file isn't tied to one machine, so the Google Cloud setup in
step 1 is only done once. On the other machine:

1. Copy `~/.config/cyd-dashboard/google_client.json` and `calendars.txt`
   from this one to the same folder there.
2. Run `./setup_calendar.sh`.

That gives the machine its own token. Copying `google_token.json` as well
also works and skips the login, but a separate token per machine means you
can revoke one without breaking the other. None of these files belong in
git.

---

## Ambient scenes

When no meeting is near, the calendar slot takes turns with small animated
**scenes**: by default a scene plays for 3 minutes, the calendar shows for 30
seconds, then the next scene, looping. From 10 minutes before a meeting until
it ends the calendar stays up, whatever the settings below; the running scene
is asked to wrap up and gets a grace period to finish (leaves fall out of
view, Life dissolves) before it's cut off. Scenes pause while the display
sleeps.

### Settings menu

The gear at the right end of the topbar opens the settings menu. It covers the
panels below the scene slot and leaves the slot itself showing, so you can see
what you're changing:

- **Scenes**: off means calendar only.
- **Calendar between**: on, the calendar shows for 30 s between scenes;
  off, scenes follow each other directly.
- **Shuffle**: a new random order every cycle (never repeating a scene across
  the boundary), or alphabetical.
- **Scene length**: 1 to 10 minutes.
- **The scene list**: tick the scenes to include in the rotation (All / None
  at the top). With several ticked they take turns; with one ticked only that
  one plays (and with the calendar between off, it just keeps going); with
  none ticked it's calendar only. Tap a scene's **name** to play it right now
  in the slot above, which works as a preview; it plays its full time (unless
  a meeting gets near), then the rotation carries on.

Changes apply straight away and are saved to the board's flash when the menu
closes (the ✕, a minute untouched, or the display going to sleep), so they
survive reboots and reflashes. The defaults for the first boot are in
`src/config.h` (`SCENES_DEFAULT_ON`, `CAL_BETWEEN_DEFAULT`, ...).

### The scenes

| Scene | What it does | Touch |
|---|---|---|
| Autumn leaves | Autumn leaves tumble down on a wandering breeze | Blow leaves away from your finger |
| Snow | Snow in two depth layers, near flakes bigger and faster | Puff flakes away from your finger |
| Game of Life | Conway's Game of Life; ends by itself when the colony dies out or gets stuck in a loop | Press or drag to bring cells to life |
| Starfield | Starfield flight, stars streaming out from a vanishing point | Press for a warp boost, drag to steer |
| Fish tank | Fish tank with seaweed and bubbles; the fish swim off screen when it ends | Tap to drop food; the nearest fish eats it |
| Quote of the day | Quote of the day, typed out, then fades out by itself. Quotes live in `src/scenes/quotes.h` | Tap for a different quote |
| Pac-Man | Pac-Man in a mini maze generated to fit the area, with chasing ghosts and power pellets. Ends by itself when the maze is cleared or the lives run out | Tap on a side of Pac-Man to steer that way |
| Space Invaders | Space Invaders: a marching formation, bombs, and a cannon. On stop the remaining aliens chain-explode | Drag to move the cannon, tap to fire |
| Asteroids | Vector-outline asteroids that split when shot. On stop the ship warps out and the rocks drift away | Hold to steer toward your finger, thrust, and fire |
| Dino runner | The offline-browser T-rex jumps cacti and ducks pterodactyls as the pace picks up, with the distance counter and best run at the top right. A moon and stars come out at night. Space on the keyboard jumps. On stop the dino sprints off | Tap to jump |
| Flappy Bird | The bird flaps through gaps between pipes over a skyline, the score counting up at the top. Space on the keyboard flaps. On stop it flies up and away | Tap to flap |
| Breakout | A wall of bricks that shatter into falling chips, a new pattern for each wall, scored 1/4/7 by row with three balls a game. A Claude session finishing splits the ball in three. On stop the bricks crumble | Drag to move the paddle |
| Missile Command | Enemy missiles (some splitting mid-air) rain on six cities; three bases with 10 missiles a wave each fire back, and fireballs chain. Arcade scoring and wave multipliers, a city back every 10,000 points, THE END when the last one falls. A Claude session finishing clears the sky | Tap to fire at that spot |

| Synthwave drive | Sunset drive: striped sun, mountains, a neon grid rushing toward you. With music the grid scrolls one line per beat and flashes on every beat. On stop the sun sets and the car drives off | Steer the car toward your finger; press for a burst of speed |
| Lofi girl | A silhouetted girl with headphones writing by a rainy window over the city. Nods to the beat while music plays, notes drift up, and every character you type becomes ink in her notebook | Tap the window for lightning, tap the lamp to switch it |
| Fireworks | Rockets burst into peonies, rings, and golden willows over a skyline. Launches on the beat with music playing; Enter fires a big one; a Claude session finishing earns a golden willow | Tap to launch a rocket that bursts where you tapped |
| Campfire | A campfire under the stars with sparks and a flickering ground glow. The flames surge with the bass and beats throw sparks; on stop it dies down to embers | Tap to toss on a log |

| Night city | A night skyline in three depths, windows switching on and off, a train on an elevated track, a blinking plane. The sky follows the real clock (day, dusk, night) with the sun or moon arcing across it. Birds live on the rooftops: a murmuration wheels over the city at dusk and dawn, roosts on the skyline at night (the train startles the ones on the rail), and by day a few pigeons hop between roofs. Typing switches office lights on; a Claude session finishing sends a shooting star; beats make the flock swerve | Tap near birds to scatter them, a building to light all its windows, the sky for a shooting star |
| Lava lamp | A lava lamp: blobs rest on the heater, rise, pause at the top and sink, merging as they pass. They swell with the bass | Tap to add a blob, drag to push them |

The arcade scenes play themselves. Touching one takes over the controls, and
the computer takes them back 5 seconds after your last touch.

**Touch.** While a scene shows, touches inside it go to the scene, and a tap
anywhere else on the screen flips to the calendar. While the calendar shows,
a tap anywhere starts the next scene, so two taps skip a scene. A scene
started by tapping (or from the menu) while a meeting is already near plays
its full time; one started earlier still gives way when the meeting gets
near. The tap that wakes a sleeping display only wakes it.

**Events.** Scenes can also react to what's happening on the PC: key presses
(by category only), music starting/pausing/stopping, track changes, Claude
sessions starting or finishing work, the hour changing, and — with audio
energy detection on — the beat, tempo, intensity, and bass of the music
that's playing. For example, the starfield surges as you type and pulses on
the beat, leaves gust on Enter and on a new track, the fish startle at
Enter, and every character you type seeds new cells in the Game of Life.

**Audio energy detection** runs in the companion and is switched with
`AUDIO_ENERGY_ENABLED` in `companion/config.py` (and key events with
`KEY_EVENTS_ENABLED`). It needs `numpy`. While music is *playing* it captures
the output the player is using (via `parec`) at low quality and analyses it:
beats are sudden rises in the kick/snare frequencies, tempo comes from how
regularly they repeat, and intensity/bass are measured against the song's own
recent levels, so the volume knob doesn't affect any of it. It uses about 3%
of one CPU core while music plays and nothing otherwise. The audio never
leaves the companion; only those few numbers are sent. `BEAT_THRESHOLD` in
`companion/audio.py` tunes how eagerly beats are detected.

To see what it detects, play something and run:

```bash
.venv/bin/python tools/watch_audio.py        # Ctrl+C to stop; or pass a duration in seconds
```

It runs the same monitors as the companion but prints the events instead of
sending them (it doesn't open the serial port, so the service can keep
running):

```
   2.99s  {"type": "beat", "s": 76}
   3.02s  {"type": "au", "i": 41, "b": 86, "bpm": 0}
   3.13s  {"type": "au", "i": 39, "b": 78, "bpm": 0}
   3.36s  {"type": "beat", "s": 100}
   ...
   6.10s  {"type": "au", "i": 46, "b": 73, "bpm": 78}
```

`au` arrives about 10 times a second; `beat` whenever one hits, with its
strength. `bpm` stays 0 for the first few seconds until enough beats have
been heard to estimate the tempo.

**Timing** (`src/config.h`): `CAL_PEEK_MS`, `CAL_QUIET_MIN`
(minutes before a meeting when the calendar takes over),
`SCENE_STOP_GRACE_MS`, and `SCENE_FRAME_MS` (25 fps).

**Colours** are `COL_SCENE_*` tokens in `src/theme.h`. Each falls back to
the theme's own palette (leaves use the warn/alert/glow colours, snow the
text colours, Life the OK colour), so every scene matches whichever theme is
active without any extra work; a theme can override them individually.

**Writing a scene.** A scene is one self-contained file in `src/scenes/`
that fills in the `Scene` interface from `src/scenes/scene.h`:
`start(area, w, h)` builds it inside the area it's given (any size, so the
same scene could run fullscreen later), `tick(dt)` animates it,
`request_stop()` asks it to wrap up, `is_done()` reports when it has, and the
optional `touch()`, `finish()`, and `event()` handle input, free non-LVGL
memory, and react to host events (`scene_ctx()` has the current music/tempo/
typing state at any time, e.g. to check if music is already playing at start).
Set `SCENE_EVENT_LOG 1` in `config.h` to log every event a scene receives. The
player deletes the scene's area and everything in it when the scene ends, so
a scene can't leave objects behind. Register it with one line under its
`Scene` definition, `SCENE_REGISTER(scene_<name>, "Menu title");`, and it
shows up in the menu and the rotation on its own; add its `COL_SCENE_<NAME>_*` fallbacks to `theme.h`. Sprite-heavy scenes
can draw into a single pixel canvas with `src/scenes/pixfb.h` (fills, lines,
circles, triangles, 1-bit sprites, multi-colour sprites from text art,
5x7 numbers for scores, colour mixing) instead of creating an
object per sprite; it batches each frame's changes into a few redraw areas.
Painted scenes can draw a backdrop once, `pixfb_bg_save()` it, and erase
moving things with `pixfb_bg_restore()`. Each scene
transition is logged as `CYD: scene ...` in the companion's output, together
with the scene's average frame time (target 40 ms), which is handy for
checking the rotation and spotting a scene that's too heavy to draw.

The firmware also runs a 5-second loop watchdog: if the main loop ever stalls
(e.g. a scene stuck in an endless loop), the board reboots itself and prints a
backtrace, which shows up in the companion's log, instead of freezing.

---

## Enabling Claude working-session dots

The Claude panel can show a small dot per Claude Code session that's
currently mid-turn (thinking or running tools), next to the existing
rate-limit dot — see [Colour themes](#colour-themes) below for how that's
rendered. This is opt-in and off by default, because detecting "is Claude
working right now" isn't something Claude Code exposes passively — it
requires registering [hooks](https://docs.claude.com/en/docs/claude-code/hooks)
that fire on every prompt submit / turn end.

```bash
./install_claude_activity_hooks.sh
```

This edits `~/.claude/settings.json`, adding a `UserPromptSubmit` hook and a
`Stop` hook that both point at `companion/hooks/session_touch.py` — a small
stdlib-only script that tracks which sessions are currently active in
`~/.cache/cyd-dashboard/claude_sessions.json`. It's idempotent (safe to
re-run) and additive (backs up your existing settings file first and merges
in alongside any hooks you already have configured).

**This is a machine-wide change** — once installed, the hooks fire for
*every* Claude Code session on the machine, not just ones related to this
dashboard, since Claude Code has no concept of "which project cares about
this." Already-open sessions may need to be restarted to pick up the new
hooks. If the companion app is run without this installed, it prints a
one-line warning and the working-session dots simply stay empty — nothing
else is affected.

```bash
./uninstall_claude_activity_hooks.sh
```

This removes only the two hook entries referencing `session_touch.py` from
`~/.claude/settings.json` (any other hooks you have configured are left
untouched, and a timestamped backup is made first) and deletes
`~/.cache/cyd-dashboard/`. Safe to re-run; does nothing if the hooks were
never installed.

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
| `CYD_THEME_COZYFALL`      | `src/themes/cozyfall.h`      | Autumn lofi — espresso brown, pumpkin-orange accents      |
| `CYD_THEME_RAINBOW`       | `src/themes/rainbow.h`       | Diagnostic — every colour token distinct (see below)      |

**Forest** — `src/themes/forest.h` — warm dark forest green with sage and
cream text (the original theme).

**Grape Ember** — `src/themes/grape-ember.h` — deep plum/violet with rose-pink
text. The topbar inverts to the primary rose-pink as its background (dark
plum/violet text for contrast), and the music icons and the generic
"active" dot colour all use an orange accent instead of the
violet OK colour — see the Level-3 overrides at the bottom of the file.

**Neon Rose** — `src/themes/neon-rose.h` — black background with a hot-pink
topbar and accents. The topbar sits on the bright pink panel colour, so its
clock/date text use dedicated dark overrides for contrast rather than the
generic primary/dim text.

**Cozyfall** — `src/themes/cozyfall.h` — autumn lofi, matches the
hand-authored Omarchy `cozyfall` desktop theme (retuned for this panel's
color response — see the comment block at the top of the file).

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
  config.py         User-tunable settings (intervals, calendars, keyboard idle threshold)
  main.py           Entry point — serial loop, packet assembly
  gcal.py           Google Calendar poller (run directly to list calendars)
  gcal_auth.py      One-time Google login — saves the read-only token
  text_utils.py     Shared helpers for strings sent to the firmware
  keyboard.py       evdev keypress monitor, daily keystroke count, key categories
  audio.py          Beat/tempo/intensity detection for the ambient scenes (optional)
  media.py          playerctl MPRIS2 poller
  claude_tokens.py  JSONL scanner + API rate-limit fetcher
  claude_activity.py  Reads the working-session status file (see hooks/ below)
  hooks/
    session_touch.py Claude Code hook script — marks a session active/idle
  idle_messages.txt Rotating messages shown when no music is playing

src/                ESP32 firmware (Arduino / PlatformIO)
  config.h          User-tunable settings (theme, scenes, timeouts, brightness, calendar)
  layout.h          Panel geometry and hardware wiring constants
  state.h           Shared DashState struct populated from serial packets
  theme.h           Colour theme selector (includes the active themes/*.h)
  themes/           Standalone colour palettes — see Colour themes above
    forest.h        Warm dark forest green (original)
    grape-ember.h   Deep plum/violet, orange accents
    neon-rose.h     Black background, hot-pink topbar and accents
    cozyfall.h      Autumn lofi, espresso brown with pumpkin-orange accents
    rainbow.h       Diagnostic — every colour token distinct
  ui_helpers.h/cpp  Shared LVGL widget factories and formatters
  main.cpp          Hardware init, sleep overlay, packet handler, setup/loop
  settings.*        Menu settings, kept in NVS flash across reboots
  scenes/           Ambient scenes that take turns with the calendar
    scene.h         The Scene plug-in interface
    scene_player.*  Rotation, calendar hand-off, touch routing
    registry.*      Every scene, each added by SCENE_REGISTER in its own file
    leaves.cpp      Falling autumn leaves
    snow.cpp        Snowfall in two depth layers
    life.cpp        Conway's Game of Life
    stars.cpp       Starfield flight
    fish.cpp        Fish tank
    quote.cpp       Quote of the day (quotes in quotes.h)
    pacman.cpp      Pac-Man demo in a generated mini maze
    invaders.cpp    Space Invaders demo
    asteroids.cpp   Asteroids demo
    synthwave.cpp   Synthwave sunset drive
    lofi.cpp        Lofi girl by a rainy window
    fireworks.cpp   Fireworks over a skyline
    campfire.cpp    Campfire night
    city.cpp        Night city skyline that follows the clock, with its birds
    lava.cpp        Lava lamp
    dino.cpp        Dino runner
    flappy.cpp      Flappy Bird
    breakout.cpp    Breakout
    missile.cpp     Missile Command
    pixfb.*         Pixel-canvas drawing helper (shapes, sprites, text art, numbers, saved backdrop)
  widgets/
    topbar.*        Clock, date, and the settings menu button
    menu.*          Settings menu: scenes on/off, calendar between, length, scene list
    calendar.*      Current/next meeting, countdown, and day timeline
    music.*         Now-playing panel with animated icon
    system.*        CPU and RAM bars
    claude.*        Session count, working dots, and rate-limit bars
    status.*        Connection dot, idle time, keystrokes, and host IP

tools/
  png_to_sprites.py Converts assets/lofi/*.png into src/scenes/lofi_art.h
  watch_audio.py    Prints the beat/audio events live, without the board
```

