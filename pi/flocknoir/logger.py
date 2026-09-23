"""CSV optical evidence logs; monotonic timestamps and fresh GPS only."""
import csv
import os
import time
import threading
import uuid
from collections import deque
import config as C
from .alerts import detection_method

CSV_HEADER = ("iso_utc,uptime_ms,source,lat,lon,alt_m,sats,hdop,freq_hz,duty,"
              "confidence,blob_x,blob_y,blob_frac,level_pp,evidence,logged_uptime_ms,"
              "detection_method,category,assessment,mac,rssi,radio_method,radio_tier,ir_timing_match,camera_pattern")


class HitLogger:
    def __init__(self, gps):
        self._gps = gps
        self._lock = threading.Lock()
        self.recent = deque(maxlen=C.RECENT_ALERTS)
        self.count = self.errors = 0
        self.ready = False
        self.path = os.path.join(C.LOG_DIR, "flock_" + uuid.uuid4().hex[:12] + ".csv")
        try:
            os.makedirs(C.LOG_DIR, exist_ok=True)
            with open(self.path, "w", encoding="utf-8", newline="") as file:
                file.write(CSV_HEADER + "\n")
            self.ready = True
        except OSError:
            self.errors += 1

    def hit(self, source, freq_hz, duty, conf, bx=0, by=0, blob_frac=0., level_pp=0,
            observed=None, evidence=None, nearby=None, fusion=None):
        now = time.monotonic()
        observed = now if observed is None else observed
        fix = self._gps.fix()
        valid = bool(fix.get("valid")) and now-observed <= C.GPS_MAX_AGE_S
        iso = self._gps.iso_utc()
        lat, lon = (fix.get("lat"), fix.get("lon")) if valid else (None, None)
        evidence = evidence or ("camera_pattern" if source == "camera" else "ir_timing_match")
        ir, camera = source == "ir", source == "camera"
        if fusion:
            ir, camera = bool(fusion["mask"] & 1), bool(fusion["mask"] & 2)
        radio = nearby or {}
        if nearby:
            evidence = "optical_radio_nearby"
        with self._lock:
            self.recent.appendleft(dict(t=iso, src=source, lat=lat, lon=lon,
                                        hz=freq_hz, duty=duty, conf=conf, evidence=evidence))
            self.count += 1
            try:
                if not self.ready:
                    raise OSError("Log unavailable")
                with open(self.path, "a", encoding="utf-8", newline="") as file:
                    csv.writer(file).writerow([iso, int(observed*1000), source, lat, lon,
                                               fix.get("alt") if valid else None, fix.get("sats", 0),
                                               fix.get("hdop"), freq_hz, duty, conf, bx, by,
                                               blob_frac, level_pp, evidence, int(now*1000),
                                               fusion["method"] if fusion else detection_method(ir,camera,radio.get("protocol")),
                                               radio.get("category","Optical pulse candidate"),
                                               "corroborated_camera_candidate" if nearby else "possible_camera",
                                               radio.get("mac"),radio.get("rssi"),radio.get("method"),
                                               radio.get("tier",0),int(ir),int(camera)])
            except OSError:
                self.errors += 1
        print("[ALERT/%s] %s %.1fHz duty=%.0f%% evidence=%s" %
              (source, iso, freq_hz, duty*100, evidence), flush=True)

    def radio_hit(self, record):
        # Optical columns stay empty: a radio packet does not measure pulse Hz.
        row = dict(iso_utc=record["time"],uptime_ms=record["uptime_ms"],source=record["protocol"],
                   lat=record["lat"],lon=record["lon"],evidence=record["evidence"],
                   logged_uptime_ms=int(time.monotonic()*1000),detection_method=record["detection_method"],
                   category=record["category"],assessment=record["assessment"],mac=record["mac"],
                   rssi=record["rssi"],radio_method=record["method"],radio_tier=record["tier"],
                   ir_timing_match=int(record["ir_timing_match"]),camera_pattern=int(record["camera_pattern"]))
        with self._lock:
            try:
                if not self.ready:
                    raise OSError("Log unavailable")
                with open(self.path,"a",encoding="utf-8",newline="") as file:
                    csv.DictWriter(file,fieldnames=CSV_HEADER.split(",")).writerow(row)
            except OSError:
                self.errors += 1

    def recent_list(self):
        with self._lock:
            return list(self.recent)
