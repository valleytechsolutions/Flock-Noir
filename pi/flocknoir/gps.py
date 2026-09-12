"""NMEA GPS reader over serial (pyserial + pynmea2) with health counters.

Health mirrors the XIAO build: chars received proves the module is wired and
talking at the right baud; good/fail count valid/invalid sentences. A module
that is connected but has no satellites still shows chars climbing.
"""
import threading
import time
from datetime import datetime, timezone

import config as C

try:
    import serial
    import pynmea2
    _HAVE = True
except Exception:
    _HAVE = False


class GPS:
    def __init__(self):
        self._lock = threading.Lock()
        self.chars = 0
        self.good = 0
        self.fail = 0
        self._fix = {}          # lat, lon, alt, sats, hdop, time (datetime)
        self._valid = False
        self._t = threading.Thread(target=self._run, daemon=True)
        if _HAVE and C.GPS_PORT:
            self._t.start()

    def _run(self):
        while True:
            try:
                with serial.Serial(C.GPS_PORT, C.GPS_BAUD, timeout=1) as ser:
                    while True:
                        raw = ser.readline()
                        if not raw:
                            continue
                        with self._lock:
                            self.chars += len(raw)
                        line = raw.decode("ascii", "ignore").strip()
                        if not line.startswith("$"):
                            continue
                        try:
                            msg = pynmea2.parse(line)
                        except Exception:
                            with self._lock:
                                self.fail += 1
                            continue
                        with self._lock:
                            self.good += 1
                            self._absorb(msg)
            except Exception:
                time.sleep(3)            # port missing / unplugged: retry

    def _absorb(self, msg):
        t = getattr(msg, "sentence_type", "")
        if t == "GGA":
            try:
                q = int(msg.gps_qual or 0)
            except ValueError:
                q = 0
            if q > 0 and msg.latitude and msg.longitude:
                self._fix.update(lat=float(msg.latitude), lon=float(msg.longitude))
                self._valid = True
            try:
                self._fix["sats"] = int(msg.num_sats or 0)
            except ValueError:
                pass
            try:
                self._fix["hdop"] = float(msg.horizontal_dil or 0)
            except (ValueError, TypeError):
                pass
            try:
                self._fix["alt"] = float(msg.altitude)
            except (ValueError, TypeError):
                pass
        elif t == "RMC":
            if getattr(msg, "status", "") == "A" and msg.latitude and msg.longitude:
                self._fix.update(lat=float(msg.latitude), lon=float(msg.longitude))
                self._valid = True
            if getattr(msg, "datestamp", None) and getattr(msg, "timestamp", None):
                try:
                    self._fix["time"] = datetime.combine(msg.datestamp, msg.timestamp,
                                                         tzinfo=timezone.utc)
                except Exception:
                    pass

    # ---- accessors ------------------------------------------------------------
    def fix(self):
        with self._lock:
            d = dict(self._fix)
            d["valid"] = self._valid
            return d

    def health(self):
        with self._lock:
            return {"chars": self.chars, "good": self.good, "fail": self.fail}

    def iso_utc(self):
        f = self.fix()
        t = f.get("time")
        if t:
            return t.strftime("%Y-%m-%dT%H:%M:%SZ")
        # fall back to the Pi's own clock (usually NTP-set, or RTC)
        return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    def wigle_time(self):
        return self.iso_utc().replace("T", " ").rstrip("Z")
