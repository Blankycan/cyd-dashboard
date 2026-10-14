"""
Audio energy monitor — beat, tempo, intensity, and bass from what's playing.

Only runs while the media monitor reports something *playing*: it then
captures that player's audio output at low quality through PipeWire/Pulse
(`parec` on the output's monitor source) and analyses it ~43 times a second.
Paused or stopped, the capture is shut down and it costs nothing. The audio
never leaves this process; only a few numbers are sent to the board:

  beat events   {"type":"beat","s":0-100}     the instant an onset is detected
  audio frames  {"type":"au","i":0-100,"b":0-100,"bpm":N,"sp":[12 × 0-99]}   ~14 per second

Everything is measured relative to the song's own recent levels, so the
volume knob doesn't change any of it:
  s    beat strength: how far the onset rose above the adaptive threshold
  i    intensity: dynamics within the song (quiet intro vs. the drop) blended
       with how busy it is (onsets per second)
  b    bass: low-band energy relative to its own recent peak
  bpm  tempo estimated from the rhythm of the onsets (0 while there's no clear pulse)
  nb   nod tempo: the pulse to bob along to, 50-150 BPM (left out while there's no clear pulse)
  ph   where in that pulse we are, 0-1, nodding at 0 (sent with nb)
  sp   spectrum: 12 log-spaced bands from 40 Hz to 5 kHz, against the
       loudest band's recent peak (tilted, as music has less energy up high)

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
FRAME_HZ  = 14             # audio frames sent to the board per second
SPEC_BANDS = 12
SPEC_RANGE = 3.0           # natural-log range shown below the peak (~26 dB)
SPEC_TILT  = 0.3           # log units added per band upward, so treble isn't always lower
SPEC_PEAK_FALL = 0.3       # how fast the remembered peak sinks (log units per second)
MIN_BEAT_GAP_S = 0.12
BEAT_THRESHOLD = 2.0   # onset must exceed this × the recent median; lower = more beats
TEMPO_WINDOW_S = 6.0

# Nod pulse: the beat a listener would bob their head to (see PulseTracker)
NOD_MIN_BPM, NOD_MAX_BPM = 50, 150   # picked in this range (faster songs nod every other beat)
NOD_CENTER_BPM  = 90       # ...leaning toward this pace when two candidates are about as clear
NOD_PRIOR_OCT   = 1.5      # ...but only gently: the rhythm itself mostly decides
NOD_SNARE_BPM   = (45, 90) # she nods on the snare, and in between when it comes this slowly
NOD_FLOOR_KICK  = 0.35     # a kick repeating on every beat (four-on-the-floor), this clearly and as clearly as
                           # every other beat, nods on every beat
PULSE_WINDOW_S  = 8.0      # rhythm looked at for the tempo
PULSE_MIN_S     = 3.0      # first guess after this much audio
PULSE_EVERY_S   = 0.25     # re-estimated this often
PULSE_CONF_ON   = 0.30     # repeat strength (autocorrelation) needed to start nodding...
PULSE_CONF_OFF  = 0.18     # ...and below which it stops
PULSE_CONTRAST  = 1.6      # the last few seconds must still be this much louder on the pulse than between
HP_TIME, HP_FREQ = 9, 31   # percussive split: frames of time (~0.2 s) and bins of frequency (~330 Hz)
# How strongly the pulse must be in the drums (percussive part) to start
# nodding, and to keep on: strict to start so a piano intro doesn't count,
# lenient after so a softer stretch of drums doesn't stop her
PERC_ON, PERC_OFF = 150, 70
METRE_MULTIPLES = (1, 2, 4)
COAST_AFTER_S = 8.0       # after nodding this long to a song, a break in the drums doesn't stop her
PHASE_NUDGE   = 0.15     # timing off by less than this share of a nod is corrected at once...
PHASE_JUMP    = 40        # ...more only after this many estimates (~10 s) agree
OCTAVE_SWITCH = 12        # estimates (~3 s) a half/double pace must keep winning before she switches to it
PULSE_LEAD_S    = 0.0      # sent this far ahead (+) or behind (-), to line her nods up with what you hear


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


class PulseTracker:
    """The pulse a listener nods along to: its tempo and where we are in it.

    Fed the onset envelope (plus the percussive kick and snare) one frame
    at a time. Every PULSE_EVERY_S it autocorrelates the last PULSE_WINDOW_S
    and adds that to a memory of the whole song (its drum-filled parts),
    from which it picks the beat: the clearest repeat that also holds over
    two beats and a bar. How often to nod follows the drums, like a person
    nodding along: down on the snare, and once more in between when the
    snares come slowly (NOD_SNARE_BPM); on every beat when the kick is
    (four-on-the-floor). The phase comes from combing the last few seconds
    of kick and snare hits at that period, and only moves a long way once
    that's held for a while.

    Only starts on a pulse it's sure of: clearly periodic, still there in
    the last few seconds, carried by the drums (not a piano or a voice on
    its own). Once a song's groove is established (COAST_AFTER_S) it keeps
    it through a break in the drums; the music going quiet or a new song
    ends it. Noise, ambient stretches and silence give `period == 0`."""

    def __init__(self, np):
        self._np    = np
        self._env   = deque(maxlen=int(PULSE_WINDOW_S * FPS))
        self._perc  = deque(maxlen=int(PULSE_WINDOW_S * FPS))
        self._drums = deque(maxlen=int(PULSE_WINDOW_S * FPS))   # (kick, snare) band onsets
        self._rms   = deque(maxlen=int(FPS))
        self._every = max(1, int(PULSE_EVERY_S * FPS))
        self._frame = 0
        self.period = 0.0    # frames per nod, 0 = no pulse
        self.phase  = 0.0    # 0-1 through the current nod, nodding at 0
        self._good = self._bad = 0
        self._cand, self._cand_n = 0.0, 0   # a different tempo that keeps winning
        self._jump, self._jump_n = 0.0, 0   # a different phase that keeps winning
        self.new_song()

    def new_song(self):
        """Forget what this song's rhythm has looked like so far. A song
        mostly keeps one pulse throughout, so the tempo is chosen from all
        of it, not just the last few seconds where a syncopated stretch or
        a fill can look like something else."""
        np = self._np
        size = 4 * int(np.ceil(FPS * 60 / NOD_MIN_BPM)) + 4
        self._song_ac = np.zeros(size)
        self._song_n  = np.zeros(size)
        self._song_kick  = np.zeros(size)   # the same for the kick and snare on their own
        self._song_snare = np.zeros(size)
        self._nodded = 0                     # frames she's nodded to this song so far
        self._lose()

    def bpm(self) -> float:
        return 60 * FPS / self.period if self.period else 0.0

    def push(self, env: float, kick: float, snare: float, perc: float, rms: float, loud_avg):
        self._env.append(env)
        self._perc.append(perc)
        self._drums.append((kick, snare))
        self._rms.append(rms)
        self._frame += 1
        if self.period:
            self.phase = (self.phase + 1 / self.period) % 1.0
            self._nodded += 1
        if self._frame % self._every == 0 and len(self._env) >= PULSE_MIN_S * FPS:
            self._estimate(loud_avg)

    def _lose(self):
        self.period = 0.0
        self._good = 0
        self._cand_n = 0

    def _estimate(self, loud_avg):
        np = self._np
        # Gone quiet (the song ended, a soft interlude): nothing to nod to
        if loud_avg is None or float(np.mean(self._rms)) < 0.2 * loud_avg:
            self._lose()
            return
        e = np.array(self._env, np.float32)
        n = len(e)
        x = e - e.mean()
        ac = np.correlate(x, x, "full")[n - 1:]
        if ac[0] <= 0:
            self._lose()
            return
        ac = ac / ac[0] * n / (n - np.arange(n))   # normalised, and unbiased for the shorter overlap
        live = ac   # what the last few seconds say, for whether the beat is there right now

        lo = int(FPS * 60 / NOD_MAX_BPM)
        hi = min(n // 2, int(np.ceil(FPS * 60 / NOD_MIN_BPM)))
        lags = np.arange(lo, hi + 1)
        prior = np.exp(-0.5 * (np.log2(60 * FPS / lags / NOD_CENTER_BPM) / NOD_PRIOR_OCT) ** 2)

        # Add to the song's memory (the reliable part of this window), as
        # much as the drums are beating right now, so an intro or a break
        # without them doesn't count; and pick the tempo from that
        perc = np.array(self._perc, np.float32)
        hit = float(self._comb_scores(perc, float(lags[np.argmax(live[lags] * prior)]), 4.0)[0].max())
        w = float(np.clip((hit - PERC_OFF) / (PERC_ON - PERC_OFF), 0, 1))
        m = min(len(self._song_ac), int(0.75 * n))
        self._song_ac[:m] += w * ac[:m]
        self._song_n[:m] += w
        drums = np.array(self._drums, np.float32)
        live_drums = []   # the kick's and snare's own repeats right now
        for mem, band in ((self._song_kick, drums[:, 0]), (self._song_snare, drums[:, 1])):
            b = band - band.mean()
            bac = np.correlate(b, b, "full")[n - 1:]
            if bac[0] > 0:
                bac = bac / bac[0] * n / (n - np.arange(n))
                mem[:m] += w * bac[:m]
                live_drums.append(bac)
        if self._song_n[0] >= 2:   # a few drum-filled seconds remembered
            ac = np.divide(self._song_ac, self._song_n, out=np.zeros_like(self._song_ac), where=self._song_n > 0)
            n_ac = int((self._song_n > 0).sum())
        else:
            n_ac = int(0.75 * n)
        # A beat repeats at 2 and 4 times its period too (two beats, a bar),
        # where a syncopated rhythm every 1.5 beats doesn't line up: score
        # each candidate on all three, so the dotted feel of a lot of lofi
        # isn't taken for the beat
        peaks = np.maximum(ac, np.maximum(np.roll(ac, 1), np.roll(ac, -1)))   # a frame of slop
        metre = np.array([np.mean([peaks[k * l] for k in METRE_MULTIPLES if k * l < n_ac]) for l in lags])
        l = int(lags[np.argmax(metre * prior)])

        def peak_near(x):   # the autocorrelation peak by lag x, between frames
            l = max((int(x) - 1, int(x), int(x) + 1, int(x) + 2), key=lambda h: ac[h] if 0 < h < n_ac - 1 else -1)
            y0, y1, y2 = ac[l - 1], ac[l], ac[l + 1]
            c = y0 - 2 * y1 + y2
            return l + (float(np.clip(0.5 * (y0 - y2) / c, -0.5, 0.5)) if c < 0 else 0.0)

        # The beat: the fastest pace in this family
        while l / 2 >= lo and ac[int(round(peak_near(l / 2 - 0.5)))] >= 0.5 * ac[l]:
            l = int(round(peak_near(l / 2 - 0.5)))
        beat = peak_near(l - 0.5)
        lag = self._nod_period(beat, n_ac, peak_near)
        # How clearly that repeats right now: in all the onsets, or in the
        # kick or snare alone (a syncopated kick can blur the whole, while
        # the snare keeps perfect time)
        conf = max(float(x[int(round(lag))]) for x in [live] + live_drums)

        if self.period and abs(lag / self.period - 1) < 0.06:
            self.period += 0.3 * (lag - self.period)
            self._cand_n = 0
        elif self.period:
            # A different tempo has to win a few times in a row to take over
            if self._cand_n and abs(lag / self._cand - 1) < 0.06:
                self._cand_n += 1
            else:
                self._cand, self._cand_n = lag, 1
            # Half/double the current pace is the same beat felt differently,
            # so flipping between them takes longer than a real tempo change
            octave = abs(lag / self.period - 2) < 0.12 or abs(lag / self.period - 0.5) < 0.03
            if self._cand_n >= (OCTAVE_SWITCH if octave else 4):
                self.period, self._cand_n = lag, 0
                self.phase = self._comb(e, lag, PULSE_WINDOW_S)[0]

        period = self.period or lag
        # A pulse in the piano or a vocal alone isn't one to nod to: it has
        # to be in the drums
        drums_hit = float(self._comb_scores(perc, period, 4.0)[0].max())
        # Timing from the drums' hits themselves (the percussive kick and
        # snare): the full onset envelope also has the off-beat bass and
        # hi-hats of a dance track, which can pull her onto the off-beat
        anchor = (drums / (drums.mean(axis=0) + 1e-9)).sum(axis=1)
        phase = self._comb(anchor, period, PULSE_WINDOW_S)[0]
        contrast = self._comb(e, period, max(2.0, 2.2 * period / FPS))[1]   # the beat's still there right now
        ok = (conf >= (PULSE_CONF_OFF if self.period else PULSE_CONF_ON) and contrast >= PULSE_CONTRAST
              and drums_hit >= (PERC_OFF if self.period else PERC_ON))
        if not ok:
            if self.period and self._nodded >= COAST_AFTER_S * FPS:
                # Once the song's groove is established, she keeps nodding
                # through a break in the drums, as you would; only the music
                # going quiet (above) or a new song stops her
                return
            self._bad += 1
            self._good = 0
            if self._bad >= 2 or conf < 0.1:
                self._lose()
            return
        self._bad = 0
        if self.period:
            # Small corrections straight away; moving her to another spot in
            # the bar (the snare instead of the kick, the off-beat) only once
            # that keeps winning for a while
            err = (phase - self.phase + 0.5) % 1.0 - 0.5
            if abs(err) < PHASE_NUDGE:
                self.phase = (self.phase + 0.3 * err) % 1.0
                self._jump_n = 0
            elif self._jump_n and abs((err - self._jump + 0.5) % 1.0 - 0.5) < PHASE_NUDGE:
                self._jump_n += 1
                if self._jump_n >= PHASE_JUMP:
                    self.phase, self._jump_n = phase, 0
            else:
                self._jump, self._jump_n = err, 1
        else:
            # Start once two guesses in a row agree
            if self._good and abs(lag / self._cand - 1) < 0.06:
                self._good += 1
            else:
                self._cand, self._good = lag, 1
            if self._good >= 2:
                self.period, self.phase = lag, phase

    def _comb_scores(self, e, period, secs, decay=0.8):
        """Average onset strength for a pulse every `period` frames over the
        last `secs` seconds, for each offset (frames since its latest beat),
        and the average onset strength overall."""
        np = self._np
        seg = e[-int(secs * FPS):]
        seg = np.convolve(seg, [0.25, 0.5, 0.25], "same")   # tolerate a frame of timing slop
        r = len(seg)
        k = np.arange(int((r - 1) / period) + 1)
        w = decay ** k   # the latest beats count most
        scores = []
        for phi in range(int(period) + 1):
            idx = (r - 1 - phi - np.round(k * period)).astype(int)
            ok = idx >= 0
            scores.append(float((seg[idx[ok]] * w[ok]).sum() / w[ok].sum()))
        return np.array(scores), float(seg.mean()) + 1e-9

    def _comb(self, e, period, secs):
        """(phase now, contrast) for a pulse every `period` frames over the
        last `secs` seconds: the offset where the onsets line up best, and how
        much stronger they are there than on average."""
        scores, mean = self._comb_scores(e, period, secs)
        phi = int(self._np.argmax(scores))
        return (phi / period) % 1.0, scores[phi] / mean

    def _nod_period(self, beat, n_ac, peak_near):
        """How often to nod, given the beat (frames): the way you'd nod
        along, anchored on the drums. The head comes down on the snare (the
        backbeat), and once more in between when snares come slowly; with
        the kick on every beat (four-on-the-floor), on every beat."""
        np = self._np
        if self._song_n[0] < 2:   # not enough of the song's drums yet: just the beat, folded
            snare_k = 1
        else:
            have = lambda k: k * beat + 1 < n_ac
            at = lambda mem, k: float(mem[int(round(k * beat)) - 1:int(round(k * beat)) + 2].max()) / self._song_n[0]
            kick = {k: at(self._song_kick, k) for k in (1, 2) if have(k)}
            if kick.get(1, 0) >= NOD_FLOOR_KICK and kick[1] >= 0.8 * kick.get(2, 0):
                return beat
            snare = {k: at(self._song_snare, k) for k in (1, 2, 4) if have(k)}
            top = max(snare.values())
            # The snare's repeat: the shortest that's about as clear as any
            snare_k = min(k for k, v in snare.items() if v >= 0.75 * top) if top > 0.2 else 1
        period = snare_k * beat
        while 60 * FPS / period > NOD_SNARE_BPM[1] and period * 2 < n_ac:
            period *= 2
        while 60 * FPS / period < NOD_SNARE_BPM[0] and period / 2 >= beat:
            period /= 2
        return peak_near(period - 0.5)


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
        # Onsets weighted toward kick/snare territory over hiss and hi-hats,
        # each band by its average bin, since the treble band has ten times
        # as many bins as the bass band.
        freqs = np.fft.rfftfreq(WIN, 1.0 / RATE)
        low_sel, mid_sel, high_sel = freqs < 300, (freqs >= 300) & (freqs < 2000), freqs >= 2000
        # Percussive part (drums, not piano): a hit is broadband, so it
        # survives a median across frequency, while notes are narrow peaks
        # that a median over the last moments of time calls sustained
        hp_hist = np.zeros((HP_TIME, len(freqs)), np.float32)
        perc_sel = freqs < 5000
        kick_sel, snare_sel = (freqs >= 40) & (freqs < 150), (freqs >= 1000) & (freqs < 4000)   # snare / clap noise
        prev_kick = prev_snare = 0.0
        intensity = bass = 0.0
        bpm = 0.0
        pulse = PulseTracker(np)
        last_song = None
        last_beat = last_tempo = -1e9
        frame = 0
        pending = None   # candidate onset, confirmed once the next frame is lower
        bass_bins = slice(4, 14)   # ~43-150 Hz at 10.8 Hz per bin
        edges = np.geomspace(40, 5000, SPEC_BANDS + 1)
        spec_bins = np.searchsorted(freqs, edges)
        spec_tilt = SPEC_TILT * np.arange(SPEC_BANDS)
        spec_peak = -1e9
        spec_val  = np.zeros(SPEC_BANDS)

        while self._running:
            raw = stream.read(HOP * 2)
            if len(raw) < HOP * 2:
                return   # capture ended
            frame += 1
            now = frame / FPS   # seconds of audio analysed so far
            if frame % int(FPS / 2) == 0:
                if not self._playing():
                    return
                m = self._media.get() or {}
                song = (m.get("title"), m.get("artist"))
                if song != last_song:
                    last_song = song
                    pulse.new_song()

            buf = np.roll(buf, -HOP)
            buf[-HOP:] = np.frombuffer(raw, np.int16) / 32768.0
            mag  = np.abs(np.fft.rfft(buf * window))
            # Log with a floor relative to the frame, so the spectral flux is
            # the same whatever the playback volume
            spec = np.log(mag + 1e-3 * float(mag.max()) + 1e-9)
            harm = np.median(hp_hist, axis=0)
            wide = np.median(np.lib.stride_tricks.sliding_window_view(
                np.pad(mag, HP_FREQ // 2, mode="edge"), HP_FREQ), axis=1)
            hits = np.maximum(wide - harm, 0)
            perc = float(hits[perc_sel].sum() / (mag[perc_sel].sum() / perc_sel.sum() + 1e-9))
            # Kick and snare onsets, from the percussive part only (a bass
            # line shares the kick's range, vocals and keys the snare's)
            p_kick, p_snare = float(hits[kick_sel].sum()), float(hits[snare_sel].sum())
            f_kick, f_snare = max(0.0, p_kick - prev_kick), max(0.0, p_snare - prev_snare)
            prev_kick, prev_snare = p_kick, p_snare
            hp_hist[frame % HP_TIME] = mag
            if prev is not None:
                rise = np.maximum(spec - prev, 0)
                flux = (float(rise[low_sel].mean()) + 0.4 * float(rise[mid_sel].mean())
                        + 0.1 * float(rise[high_sel].mean()))
            else:
                flux = 0.0
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
            pulse.push(flux, f_kick, f_snare, perc, rms, loud_avg)

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

            # Spectrum: every band against one slowly sinking peak; jumps up
            # at once, falls back smoothly
            if silent:
                spec_val *= 0.8
            else:
                lv = np.log(np.add.reduceat(mag, spec_bins)[:SPEC_BANDS]
                            / np.maximum(1, np.diff(spec_bins)) + 1e-6) + spec_tilt
                spec_peak = max(float(lv.max()), spec_peak - SPEC_PEAK_FALL / FPS)
                target_s = np.clip((lv - (spec_peak - SPEC_RANGE)) / SPEC_RANGE, 0, 1)
                spec_val = np.where(target_s > spec_val, target_s, spec_val * 0.85 + target_s * 0.15)

            if frame % int(FPS / FRAME_HZ) == 0:
                ev = {"type": "au", "i": int(100 * intensity), "b": int(100 * min(1.0, bass)),
                      "bpm": int(round(bpm)) if pulse.period else 0, "sp": [int(99 * v) for v in spec_val]}
                if pulse.period:
                    ev["nb"] = round(pulse.bpm(), 1)
                    ev["ph"] = round((pulse.phase + PULSE_LEAD_S * FPS / pulse.period) % 1.0, 3)
                self._emit(ev)
