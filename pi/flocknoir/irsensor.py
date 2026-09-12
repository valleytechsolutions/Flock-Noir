"""Analog IR photodiode detector via an MCP3008 ADC (the Pi has no ADC).

Same algorithm as the XIAO irsensor.cpp / Noflock approach: ~1 kHz samples,
EMA ambient baseline, AC pulse extraction, hysteresis edge detection with a
refractory gap, and 5-15 Hz interval validation. Optional; off by default.
Wiring: MCP3008 on SPI0 (gpiozero default), photodiode front end into CH0.
"""
import threading
import time

import config as C
from . import settings

try:
    from gpiozero import MCP3008
    _HAVE = True
except Exception:
    _HAVE = False


class IrSensor:
    def __init__(self):
        self._lock = threading.Lock()
        self.enabled = bool(settings.load_section("irsensor", {}).get("enabled",
                                                                       C.IR_ENABLED_DEFAULT))
        self._adc = None
        self.res = {"detected": False, "freqHz": 0.0, "dutyCycle": 0.0, "amp": 0,
                    "baseline": 0, "validCount": 0, "present": False}
        self._ring = []
        if _HAVE:
            try:
                self._adc = MCP3008(channel=C.IR_ADC_CHANNEL)
            except Exception as e:
                print("[IR] MCP3008 not available:", e, flush=True)
        if self._adc:
            threading.Thread(target=self._run, daemon=True).start()

    def set_enabled(self, e):
        self.enabled = bool(e)
        settings.save_section("irsensor", {"enabled": self.enabled})

    def result(self):
        with self._lock:
            return dict(self.res)

    def snapshot(self, max_n=64):
        with self._lock:
            r = self._ring[-max_n:]
        if not r:
            return []
        vmin, vmax = min(r), max(r)
        rng = max(1, vmax - vmin)
        return [int((v - vmin) * 100 / rng) for v in r]

    def _run(self):
        period = 1.0 / C.IR_SAMPLE_HZ
        baseline = self._adc.value * 4095.0
        high = False
        last_edge = 0.0
        prev_edge = 0.0
        valid = 0
        amp = 0.0
        on_acc = tot_acc = 0.0
        last_active = 0.0
        pub = 0.0
        decim = 0
        nxt = time.monotonic()
        while True:
            raw = self._adc.value * 4095.0
            now = time.monotonic()
            baseline += (raw - baseline) * C.IR_BASELINE_ALPHA
            ac = max(0.0, raw - baseline)
            amp = ac if ac > amp else amp * 0.995
            tot_acc = tot_acc * 0.999 + 1.0
            on_acc = on_acc * 0.999 + (1.0 if ac > C.IR_THR_LOCKED else 0.0)

            if not high and ac > C.IR_THR_IDLE and (now - last_edge) * 1000 >= C.IR_REFRACTORY_MS:
                high = True
                interval = now - last_edge
                if prev_edge:
                    hz = 1.0 / interval if interval > 0 else 0
                    if C.IR_MIN_HZ <= hz <= C.IR_MAX_HZ:
                        valid += 1
                        last_active = now
                    elif interval > 0.4:
                        valid = 0
                prev_edge, last_edge = last_edge, now
            elif high and ac < C.IR_THR_LOCKED:
                high = False

            decim += 1
            if decim >= max(1, C.IR_SAMPLE_HZ // 200):
                decim = 0
                with self._lock:
                    self._ring.append(int(raw))
                    if len(self._ring) > 256:
                        del self._ring[0]

            if now - pub >= 0.1:
                pub = now
                active = (now - last_active) <= C.IR_ACTIVE_WINDOW_MS / 1000.0
                det = active and valid >= C.IR_REQUIRED_INTERVALS
                iv = last_edge - prev_edge
                hz = (1.0 / iv) if (0 < iv < 0.4) else 0.0
                with self._lock:
                    self.res = {"detected": det, "freqHz": hz if det else 0.0,
                                "dutyCycle": (on_acc / tot_acc) if tot_acc > 1 else 0.0,
                                "amp": int(amp), "baseline": int(baseline), "validCount": valid,
                                "present": 30 < baseline < 4060}
                if not active:
                    valid = 0

            nxt += period
            delay = nxt - time.monotonic()
            if delay > 0:
                time.sleep(delay)
            else:
                nxt = time.monotonic()
