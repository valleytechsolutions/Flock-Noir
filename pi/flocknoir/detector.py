"""Time-domain IR pattern detector (port of firmware/FlockNoir/detector.cpp).

One brightness sample per camera frame (brightest pixel), plus the saturated
blob size, each timestamped. Over a sliding window we find rising edges,
measure the interval between them (target ~100 ms) and the duty cycle (target
~20 percent), require a compact bright source, and score a confidence 0..1.
"""
import time
from collections import deque
from dataclasses import dataclass

import config as C


@dataclass
class DetectionResult:
    detected: bool = False
    freqHz: float = 0.0
    dutyCycle: float = 0.0
    confidence: float = 0.0
    goodCycles: int = 0
    levelPP: int = 0
    blobFrac: float = 0.0
    periodScore: float = 0.0     # fraction of edge intervals near the target period
    jitter: float = 0.0          # spread of the matching intervals (0 = perfectly regular)


class Detector:
    def __init__(self, scanned_pixels: int):
        self._scanned = max(1, scanned_pixels)
        self._buf = deque(maxlen=C.SAMPLE_BUFFER)   # (t_us, level, blobPx)
        self._last = DetectionResult()

    def feed(self, level: int, blob_px: int, t_us: int | None = None):
        if t_us is None:
            t_us = time.monotonic_ns() // 1000
        self._buf.append((t_us, int(level), int(blob_px)))

    def last(self) -> DetectionResult:
        return self._last

    def snapshot(self, max_n: int = 64) -> list[int]:
        """Most recent samples normalized 0..100 (for the UI waveform)."""
        n = min(max_n, len(self._buf))
        if n <= 0:
            return []
        recent = [s[1] for s in list(self._buf)[-n:]]
        vmin, vmax = min(recent), max(recent)
        rng = max(1, vmax - vmin)
        return [int((v - vmin) * 100 / rng) for v in recent]

    def analyze(self) -> DetectionResult:
        r = DetectionResult()
        samples = list(self._buf)
        if len(samples) < C.MIN_GOOD_CYCLES * 2:
            self._last = r
            return r

        levels = [s[1] for s in samples]
        vmin, vmax = min(levels), max(levels)
        pp = vmax - vmin
        r.levelPP = pp
        if pp < C.MIN_AMPLITUDE:
            self._last = r
            return r

        hi = vmin + pp * 0.60
        lo = vmin + pp * 0.40

        on = False
        have_rise = False
        last_rise = 0
        on_start = 0
        cycles_in_tol = 0
        total_rises = 0
        on_time = 0.0
        span_sum = 0.0
        in_tol = []                 # the matching intervals (ms), for the jitter check
        blob_sum = 0.0
        blob_n = 0

        for t, v, blob in samples:
            if not on and v >= hi:
                on = True
                on_start = t
                if have_rise:
                    interval_ms = (t - last_rise) / 1000.0
                    total_rises += 1
                    if abs(interval_ms - C.TARGET_PERIOD_MS) <= C.PERIOD_TOL_MS:
                        cycles_in_tol += 1
                        span_sum += (t - last_rise)
                        in_tol.append(interval_ms)
                last_rise = t
                have_rise = True
            elif on and v <= lo:
                on = False
                on_time += (t - on_start)
            if on:
                blob_sum += blob / self._scanned
                blob_n += 1

        r.goodCycles = cycles_in_tol
        if blob_n:
            r.blobFrac = blob_sum / blob_n
        if cycles_in_tol > 0 and span_sum > 0:
            avg_period_ms = (span_sum / cycles_in_tol) / 1000.0
            r.freqHz = 1000.0 / avg_period_ms if avg_period_ms > 0 else 0.0

        total_span = samples[-1][0] - samples[0][0]
        if total_span > 0:
            r.dutyCycle = on_time / total_span

        period_score = (cycles_in_tol / total_rises) if total_rises else 0.0
        r.periodScore = period_score
        # regularity: a real strobe repeats at (almost) the same interval every time;
        # noise that happens to land in the tolerance band is spread all over it.
        if len(in_tol) >= 2:
            mean = sum(in_tol) / len(in_tol)
            var = sum((x - mean) ** 2 for x in in_tol) / len(in_tol)
            r.jitter = (var ** 0.5) / mean if mean > 0 else 1.0
        else:
            r.jitter = 1.0
        if C.DUTY_MIN <= r.dutyCycle <= C.DUTY_MAX:
            duty_score = 1.0
        else:
            d = (C.DUTY_MIN - r.dutyCycle) if r.dutyCycle < C.DUTY_MIN else (r.dutyCycle - C.DUTY_MAX)
            duty_score = max(0.0, 1.0 - d * 6.0)
        cycle_score = min(1.0, cycles_in_tol / C.MIN_GOOD_CYCLES)
        compact_score = 1.0 if r.blobFrac <= C.BLOB_MAX_FRACTION else \
            max(0.0, 1.0 - (r.blobFrac - C.BLOB_MAX_FRACTION) * 3.0)

        r.confidence = (period_score * 0.4 + duty_score * 0.3 +
                        cycle_score * 0.2 + compact_score * 0.1)
        # Decision. Duty and jitter are the gates that actually separate a real
        # strobe from sensor noise at any frame rate (see tools/README notes):
        # a strobe is short-on and metronome-regular; noise is ~50% duty and ragged.
        r.detected = (cycles_in_tol >= C.MIN_GOOD_CYCLES and
                      period_score >= C.PERIOD_SCORE_MIN and
                      r.dutyCycle <= C.DUTY_MAX and
                      r.jitter <= C.JITTER_MAX and
                      r.confidence >= C.DETECT_CONFIDENCE)
        self._last = r
        return r
