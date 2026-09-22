"""OPT101 -> MCP3008 sampler using shared validation and latched events.

Linux scheduling is not hard real time. Rate/gap diagnostics expose timing
failures. ADC activity cannot prove an optical sensor is connected.
"""
from collections import deque
import queue
import threading
import time

import config as C
from . import settings
from .native import Pulse


class IrSensor:
    def __init__(self, start=True):
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self.enabled = bool(settings.load_section("irsensor", {}).get("enabled", C.IR_ENABLED_DEFAULT))
        self.events = queue.Queue(maxsize=32)
        self._ring = deque(maxlen=256)
        self.res = dict(detected=False, freqHz=0., dutyCycle=0., amp=0, baseline=0,
                        validCount=0, present=False, clipped=False, pulseMs=0., sampleHz=0.,
                        gaps=0, droppedEvents=0, noise=0., raw=0, timestamp=0.)
        self.error = ""
        if start:
            threading.Thread(target=self._run, daemon=True, name="opt101").start()

    def set_enabled(self, enabled):
        settings.save_section("irsensor", {"enabled": bool(enabled)})
        with self._lock:
            self.enabled = bool(enabled)
            if not self.enabled:
                self.res["detected"] = False

    def result(self):
        with self._lock:
            result = dict(self.res)
            if not self.enabled or time.monotonic() - result["timestamp"] > 0.3:
                result["detected"] = False
            return result

    def pop_event(self, timeout=0.2):
        try:
            return self.events.get(timeout=timeout)
        except queue.Empty:
            return None

    def snapshot(self, max_n=64):
        with self._lock:
            values = list(self._ring)[-max_n:]
        if not values:
            return []
        low, high = min(values), max(values)
        return [int((v-low)*100/max(1, high-low)) for v in values]

    def stop(self):
        self._stop.set()

    def _run(self):
        while not self._stop.is_set():
            adc = pulse = None
            try:
                from gpiozero import MCP3008
                pulse = Pulse(
                    [C.IR_MIN_HZ, C.IR_MAX_HZ, C.IR_DUTY_MIN, C.IR_DUTY_MAX,
                     C.IR_PULSE_MIN_MS, C.IR_PULSE_MAX_MS, C.IR_THR_IDLE, C.IR_THR_LOCKED, 6],
                    [C.IR_REQUIRED_INTERVALS, C.IR_ACTIVE_WINDOW_MS*1000, C.IR_MAX_SAMPLE_GAP_US])
                adc = MCP3008(channel=C.IR_ADC_CHANNEL, port=C.IR_SPI_BUS, device=C.IR_SPI_DEVICE)
                self.error = ""
                self._sample(adc, pulse)
            except Exception as exc:
                self.error = str(exc)
                with self._lock:
                    self.res.update(present=False, detected=False)
                self._stop.wait(5)
            finally:
                if adc is not None:
                    adc.close()
                if pulse is not None:
                    pulse.close()

    def _sample(self, adc, pulse):
        interval = 1 / C.IR_SAMPLE_HZ
        next_sample = rate_at = time.monotonic()
        publish = last_event = 0.
        rate = 0.
        samples = decimation = dropped = 0
        was_enabled = previous_match = False
        while not self._stop.is_set():
            raw = round(adc.value * 4095)
            now = time.monotonic()
            enabled = self.enabled
            if enabled != was_enabled:
                pulse.reset()
                was_enabled, previous_match = enabled, False
            if enabled:
                pulse.feed(raw, int(now * 1_000_000))
            samples += 1
            if now-rate_at >= 1:
                rate = samples / (now-rate_at)
                samples, rate_at = 0, now
            decimation += 1
            if decimation >= max(1, C.IR_SAMPLE_HZ // 200):
                decimation = 0
                with self._lock:
                    self._ring.append(raw)
            if now-publish >= 0.1:
                publish = now
                result = pulse.result()
                result.update(detected=enabled and result["detected"], present=True,
                              sampleHz=rate, raw=raw, timestamp=now)
                if result["detected"] and (not previous_match or (now-last_event)*1000 >= C.ALERT_HOLDOFF_MS):
                    try:
                        self.events.put_nowait(dict(result))
                    except queue.Full:
                        dropped += 1
                    last_event = now
                previous_match = result["detected"]
                result["droppedEvents"] = dropped
                with self._lock:
                    self.res = result
            next_sample += interval
            wait = next_sample-time.monotonic()
            if wait > 0:
                self._stop.wait(wait)
            else:
                next_sample = time.monotonic()
