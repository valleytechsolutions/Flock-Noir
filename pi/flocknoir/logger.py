"""CSV hit logging + the recent-detections list shown in the UI.

Same columns as the XIAO firmware so logs from both boards can be merged:
iso_utc,unix_ms,source,lat,lon,alt_m,sats,hdop,freq_hz,duty,confidence,blob_x,blob_y,blob_frac,level_pp
"""
import os
import time
import threading
from collections import deque

import config as C

CSV_HEADER = ("iso_utc,unix_ms,source,lat,lon,alt_m,sats,hdop,freq_hz,duty,"
              "confidence,blob_x,blob_y,blob_frac,level_pp")


class HitLogger:
    def __init__(self, gps):
        self._gps = gps
        self._lock = threading.Lock()
        self.recent = deque(maxlen=C.RECENT_ALERTS)
        self.count = 0
        os.makedirs(C.LOG_DIR, exist_ok=True)
        self.path = os.path.join(C.LOG_DIR, "flock_%d.csv" % int(time.time()))
        with open(self.path, "w", encoding="utf-8") as f:
            f.write(CSV_HEADER + "\n")

    def hit(self, source, freq_hz, duty, conf, bx=0, by=0, blob_frac=0.0, level_pp=0):
        fix = self._gps.fix()
        iso = self._gps.iso_utc()
        lat = fix.get("lat", float("nan"))
        lon = fix.get("lon", float("nan"))
        alt = fix.get("alt", float("nan"))
        sats = fix.get("sats", 0)
        hdop = fix.get("hdop", float("nan"))
        with self._lock:
            self.recent.appendleft({"t": iso, "src": source, "lat": lat, "lon": lon,
                                    "hz": freq_hz, "duty": duty, "conf": conf})
            self.count += 1
            try:
                with open(self.path, "a", encoding="utf-8") as f:
                    f.write("%s,%d,%s,%.6f,%.6f,%.1f,%d,%.1f,%.2f,%.3f,%.3f,%d,%d,%.3f,%d\n" % (
                        iso, int(time.time() * 1000), source, lat, lon, alt, sats, hdop,
                        freq_hz, duty, conf, bx, by, blob_frac, level_pp))
            except OSError:
                pass
        print("[ALERT/%s] %s %.6f,%.6f %.1fHz duty=%.0f%% conf=%.2f" %
              (source, iso, lat, lon, freq_hz, duty * 100, conf), flush=True)

    def recent_list(self):
        with self._lock:
            return list(self.recent)
