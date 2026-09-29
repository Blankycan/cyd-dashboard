"""
Audio energy monitor — beat, tempo, intensity, and bass from what's playing.

Only runs while the media monitor reports something *playing*: it then
captures that player's audio output at low quality through PipeWire/Pulse
(`parec` on the output's monitor source) and analyses it ~43 times a second.
Paused or stopped, the capture is shut down and it costs nothing. The audio
never leaves this process; only a few numbers are sent to the board:

  beat events   {"type":"beat","s":0-100}     the instant an onset is detected
  audio frames  {"type":"au","i":0-100,"b":0-100,"bpm":N}   ~10 per second

Everything is measured relative to the song's own recent levels, so the
volume knob doesn't change any of it:
  s    beat strength: how far the onset rose above the adaptive threshold
  i    intensity: dynamics within the song (quiet intro vs. the drop) blended
       with how busy it is (onsets per second)
  b    bass: low-band energy relative to its own recent peak
  bpm  tempo estimated from the rhythm of the onsets (0 while unsure)

Requires numpy (pip install numpy). Disable with AUDIO_ENERGY_ENABLED in
config.py; without numpy it prints one line and stays off.
"""

import json
import subprocess
import threading
import time
from collections import deque

from config import AUDIO_ENERGY_ENABLED

RATE      = 11025
HOP       = 256            # samples per analysis frame -> ~43 fps
WIN       = 1024
FPS       = RATE / HOP
FRAME_HZ  = 10             # audio frames sent to the board per second
MIN_BEAT_GAP_S = 0.12
BEAT_THRESHOLD = 2.0   # onset must exceed this × the recent median; lower = more beats
TEMPO_WINDOW_S = 6.0


def _find_monitor(player: str | None) -> str:
    """Monitor source of the output the player is playing on, falling back to
    the default output. Matches the playerctl name ("firefox.instance_1_42")
    against each playing stream's application name/binary."""
    if player:
        base = player.split(".")[0].lower()
        try:
            streams = json.loads(subprocess.check_output(
                ["pactl", "-f", "json", "list", "sink-inputs"], stderr=subprocess.DEVNULL, timeout=2))
            sinks = {s["index"]: s["name"] for s in json.loads(subprocess.check_output(
                ["pactl", "-f", "json", "list", "sinks", "short"], stderr=subprocess.DEVNULL, timeout=2))}
            for s in streams:
                if s.get("corked"):
                    continue
                p = s.get("properties", {})
                names = (p.get("application.name", ""), p.get("application.process.binary", ""))
                if any(base in n.lower() for n in names if n) and s.get("sink") in sinks:
                    return sinks[s["sink"]] + ".monitor"
        except Exception:
            pass
    return "@DEFAULT_MONITOR@"


class AudioEnergyMonitor:
    def __init__(self, media, emit):
        self._media   = media      # MediaMonitor: decides when to listen and to what
        self._emit    = emit       # thread-safe callable(dict) that queues an event for the board
        self._running = False
        self._np      = None
        if not AUDIO_ENERGY_ENABLED:
            return
        try:
            import numpy
            self._np = numpy
        except ImportError:
            print("  [audio] numpy not installed — beat/tempo detection disabled (pip install numpy)")

    def start(self):
        if self._np is None:
            return
        self._running = True
        threading.Thread(target=self._run, daemon=True).start()

    def stop(self):
        self._running = False

    # ------------------------------------------------------------------
    def _playing(self) -> bool:
        m = self._media.get()
        return bool(m and m.get("playing"))

    def _run(self):
        while self._running:
            if not self._playing():
                time.sleep(0.5)
                continue
            try:
                self._listen(_find_monitor(self._media.player()))
            except Exception as e:
                print(f"  [audio] capture error: {e}")
                time.sleep(2)
            # Tell scenes the music went quiet
            self._emit({"type": "au", "i": 0, "b": 0, "bpm": 0})

    def _listen(self, source: str):
        proc = subprocess.Popen(
            ["parec", "-d", source, f"--rate={RATE}", "--channels=1",
             "--format=s16le", "--raw", "--latency-msec=20"],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        try:
            self._analyse(proc.stdout)
        finally:
            proc.kill()
            proc.wait()

    def _analyse(self, stream):
        """Analyse 16-bit mono PCM from `stream` until it ends or playback stops.
        Timing is counted in audio frames, not wall-clock time, so results only
        depend on the audio (and the analysis can be tested on a file)."""
        np = self._np
        buf      = np.zeros(WIN, np.float32)
        window   = np.hanning(WIN).astype(np.float32)
        prev     = None
        flux_hist = deque(maxlen=int(TEMPO_WINDOW_S * FPS))   # onset envelope, for threshold + tempo
        beat_times = deque(maxlen=64)
        # Long-running averages the current level is compared against, so a
        # drop after a quiet intro reads as intense, and volume doesn't matter
        loud_avg = bass_avg = None
        avg_rate = 1.0 / (FPS * 15)   # ~15 s memory
        # Onsets weighted toward kick/snare territory over hiss and hi-hats.
        # Weights are per band (each band's bins share it), since the treble
        # band has ten times as many bins as the bass band.
        freqs = np.fft.rfftfreq(WIN, 1.0 / RATE)
        bands = [(freqs < 300, 1.0), ((freqs >= 300) & (freqs < 2000), 0.4), (freqs >= 2000, 0.1)]
        weights = np.zeros_like(freqs, dtype=np.float32)
        for sel, w in bands:
            weights[sel] = w / max(1, int(sel.sum()))
        intensity = bass = 0.0
        bpm = 0.0
        last_beat = last_tempo = -1e9
        frame = 0
        pending = None   # candidate onset, confirmed once the next frame is lower
        bass_bins = slice(4, 14)   # ~43-150 Hz at 10.8 Hz per bin

        while self._running:
            raw = stream.read(HOP * 2)
            if len(raw) < HOP * 2:
                return   # capture ended
            frame += 1
            now = frame / FPS   # seconds of audio analysed so far
            if frame % int(FPS / 2) == 0 and not self._playing():
                return

            buf = np.roll(buf, -HOP)
            buf[-HOP:] = np.frombuffer(raw, np.int16) / 32768.0
            mag  = np.abs(np.fft.rfft(buf * window))
            # Log with a floor relative to the frame, so the spectral flux is
            # the same whatever the playback volume
            spec = np.log(mag + 1e-3 * float(mag.max()) + 1e-9)
            flux = float((np.maximum(spec - prev, 0) * weights).sum()) if prev is not None else 0.0
            prev = spec

            rms = float(np.sqrt(np.mean(buf[-HOP:] ** 2)))
            low = float(mag[bass_bins].mean())
            silent = rms < 1e-4
            if not silent:
                loud_avg = rms if loud_avg is None else loud_avg + (rms - loud_avg) * avg_rate
                bass_avg = low if bass_avg is None else bass_avg + (low - bass_avg) * avg_rate

            # Beat: a local peak in the onset envelope clearly above the
            # recent median (adaptive, so busy and sparse songs both work)
            if pending is not None:
                t, f, thr = pending
                if flux < f and t - last_beat > MIN_BEAT_GAP_S:
                    last_beat = t
                    beat_times.append(t)
                    strength = min(100, int(100 * (f - thr) / (thr + 1e-6)))
                    self._emit({"type": "beat", "s": max(1, strength)})
                pending = None
            if len(flux_hist) > FPS and not silent:
                recent = list(flux_hist)[-int(FPS / 2):]
                thr = float(np.median(recent)) * BEAT_THRESHOLD + 0.02
                if flux > thr:
                    pending = (now, flux, thr)
            flux_hist.append(flux)

            # Tempo: autocorrelation of the onset envelope over the last few
            # seconds, looking for a repeat between 70 and 180 BPM
            if now - last_tempo > 1.0 and len(flux_hist) == flux_hist.maxlen:
                last_tempo = now
                env = np.array(flux_hist, np.float32)
                env -= env.mean()
                ac = np.correlate(env, env, "full")[len(env) - 1:]
                lo, hi = int(FPS * 60 / 180), int(FPS * 60 / 70)
                lag = lo + int(np.argmax(ac[lo:hi + 1]))
                confident = ac[0] > 0 and ac[lag] / ac[0] > 0.15
                est = 60 * FPS / lag if confident else 0.0
                if est and est < 90 and 2 * lag <= hi and ac[lag // 2] > 0.8 * ac[lag]:
                    est *= 2   # prefer the faster of two octave-related candidates
                if est and bpm:
                    # Half/double of the current tempo is the same beat, not a change
                    for k in (0.5, 2.0):
                        if abs(est * k - bpm) < 0.08 * bpm:
                            est *= k
                    bpm = 0.7 * bpm + 0.3 * est
                elif est:
                    bpm = est   # while unsure, keep the last good tempo

            # Intensity & bass, smoothed so they glide rather than flicker
            # Level vs. the running average maps to 0.5 when typical, toward 1
            # when much louder (the drop) and toward 0 when much quieter
            onset_rate = sum(1 for t in beat_times if now - t < 4.0) / 4.0
            if silent or loud_avg is None:
                target_i = target_b = 0.0
            else:
                rel_l = rms / (loud_avg + 1e-9)
                rel_b = low / (bass_avg + 1e-9)
                target_i = 0.6 * rel_l / (rel_l + 1) + 0.4 * min(1.0, onset_rate / 6.0)
                target_b = rel_b / (rel_b + 1)
            intensity += (target_i - intensity) * 0.15
            bass += (target_b - bass) * 0.3

            if frame % int(FPS / FRAME_HZ) == 0:
                self._emit({"type": "au", "i": int(100 * intensity), "b": int(100 * min(1.0, bass)),
                            "bpm": int(round(bpm))})
