"""Passive-piezo buzzer with a non-blocking RTTTL (ringtone) player.

Uses gpiozero's TonalBuzzer (software PWM) on a BCM GPIO pin. The tone library
(name + RTTTL per slot, which slot is the alert tone, enabled flag) is persisted
to settings.json so the same Settings tab works as on the XIAO build.
"""
import json
import threading
import time

import config as C
from .alerts import PRESETS, KINDS, DEFAULTS, AlertQueue, valid

try:
    from gpiozero import TonalBuzzer
    from gpiozero.tones import Tone
    _HAVE_GPIO = True
except Exception:          # not on a Pi, or gpiozero missing
    _HAVE_GPIO = False


def _note_hz(letter, sharp, octave):
    idx = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}.get(letter)
    if idx is None:
        return 0.0
    if sharp:
        idx += 1
    midi = (octave + 1) * 12 + idx            # C4 = 60, A4 = 69 = 440 Hz
    return 440.0 * (2.0 ** ((midi - 69) / 12.0))


def parse_rtttl(rtttl):
    """Return a list of (frequency_hz, duration_ms). frequency 0 = pause."""
    parts = rtttl.split(":")
    if len(parts) < 3:
        return []
    d, o, b = 4, 6, 120
    for tok in parts[1].split(","):
        tok = tok.strip().lower()
        if "=" in tok:
            k, v = tok.split("=", 1)
            try:
                v = int(v)
            except ValueError:
                continue
            if k == "d" and v > 0: d = v
            elif k == "o" and v > 0: o = v
            elif k == "b" and v > 0: b = v
    whole_ms = (60000.0 / b) * 4.0
    out = []
    for tok in parts[2].lower().replace(" ", "").split(","):
        if not tok:
            continue
        i = 0
        dur = 0
        while i < len(tok) and tok[i].isdigit():
            dur = dur * 10 + int(tok[i]); i += 1
        if dur == 0:
            dur = d
        letter = tok[i] if i < len(tok) else "p"; i += 1
        sharp = False
        if i < len(tok) and tok[i] == "#":
            sharp = True; i += 1
        octave, dotted = o, False
        while i < len(tok) and (tok[i].isdigit() or tok[i] == "."):
            if tok[i] == ".": dotted = True
            else: octave = int(tok[i])
            i += 1
        ms = whole_ms / dur
        if dotted:
            ms *= 1.5
        hz = 0.0 if letter == "p" else _note_hz(letter, sharp, octave)
        out.append((hz, ms))
    return out


class Buzzer:
    def __init__(self):
        self.enabled = True
        self.alert_idx = 0
        self.tones = list(C.DEFAULT_TONES)
        self.alert_sounds = dict(DEFAULTS)
        self.alert_queue = AlertQueue()
        self._lock = threading.RLock()
        self._muted = False
        self._was_enabled = True
        self._stop = threading.Event()
        self._thread = None
        self._tb = None
        if _HAVE_GPIO and C.BUZZER_ENABLED:
            try:
                # octaves=4 gives A0..A8 (27.5..7040 Hz) so high RTTTL notes fit
                self._tb = TonalBuzzer(C.BUZZER_PIN, octaves=4)
            except Exception as e:
                print("[BUZZER] init failed:", e, flush=True)
        self.load()

    # ---- persistence --------------------------------------------------------
    def load(self):
        try:
            with open(C.SETTINGS_FILE, encoding="utf-8") as f:
                s = json.load(f).get("buzzer", {})
            self.enabled = bool(s.get("enabled", True))
            self.alert_idx = int(s.get("alertIdx", 0))
            t = s.get("tones")
            if isinstance(t, list) and t:
                self.tones = [(x.get("name", ""), x.get("rtttl", "")) for x in t][:C.BUZZER_MAX_TONES]
            for kind, sound in s.get("alertSounds", {}).items():
                if kind in DEFAULTS and valid(sound, len(self.tones)):
                    self.alert_sounds[kind] = sound
        except (OSError, ValueError):
            pass
        if not 0 <= self.alert_idx < len(self.tones):
            self.alert_idx = 0

    def save(self):
        from .settings import save_section
        save_section("buzzer", {"enabled": self.enabled, "alertIdx": self.alert_idx,
                                "alertSounds": self.alert_sounds,
                                "tones": [{"name": n, "rtttl": r} for n, r in self.tones]})

    def to_json(self):
        return {"enabled": self.enabled, "alertIdx": self.alert_idx,
                "max": C.BUZZER_MAX_TONES,
                "alertKinds": [dict(id=k, name=n, sound=self.alert_sounds[k]) for k,n,_ in KINDS],
                "soundPresets": [dict(id=k, name=n, rtttl=r) for k,n,r in PRESETS],
                "tones": [{"name": n, "rtttl": r} for n, r in self.tones]}

    # ---- playback -----------------------------------------------------------
    def is_playing(self):
        return self._thread is not None and self._thread.is_alive()

    def play(self, rtttl):
        notes = parse_rtttl(rtttl)
        if not notes:
            return
        with self._lock:
            self.stop()
            self._stop.clear()
            self._thread = threading.Thread(target=self._run, args=(notes,), daemon=True)
            self._thread.start()

    def play_alert(self):
        if self.enabled and self.tones:
            self.play(self.tones[self.alert_idx][1])

    def play_device_alert(self):
        if self.enabled:
            self.play_sound(self.alert_sounds["alpr_ble"])

    def play_sound(self, sound):
        if not valid(sound, len(self.tones)):
            raise ValueError("Invalid sound")
        if sound.startswith("slot:"):
            self.play(self.tones[int(sound[-1])][1])
        else:
            rtttl = next(r for k,_,r in PRESETS if k == sound)
            if rtttl:
                self.play(rtttl)
            else:
                self.stop()

    def request_alert(self, kind):
        if self.enabled:
            self.alert_queue.request(kind, time.monotonic())

    def update(self, muted=False):
        if (muted and not self._muted) or (not self.enabled and self._was_enabled):
            self.stop()
        self._muted, self._was_enabled = muted, self.enabled
        if muted or not self.enabled:
            self.alert_queue.clear()
            return False
        kind = self.alert_queue.take(time.monotonic(), muted, self.enabled, self.is_playing())
        if kind is not None:
            self.play_sound(self.alert_sounds[kind])
            return self.alert_sounds[kind] != "silent"
        return False

    def stop(self):
        with self._lock:
            self._stop.set()
            if self._thread and self._thread.is_alive():
                self._thread.join(timeout=0.5)
            self._silence()

    def _silence(self):
        if self._tb:
            try:
                self._tb.stop()
            except Exception:
                pass

    def _run(self, notes):
        for hz, ms in notes:
            if self._stop.is_set():
                break
            if self._tb and hz > 0:
                try:
                    self._tb.play(Tone(hz))
                except Exception:
                    self._silence()          # out of range: rest
            else:
                self._silence()
            if self._stop.wait(ms / 1000.0):
                break
        self._silence()
