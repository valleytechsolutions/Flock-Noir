"""Wi-Fi wardriver: `iw` scans -> WiGLE-format CSV (separate log), GPS-tagged.

Single-radio caveat: on a Pi Zero 2 W the hotspot and the scanner share wlan0,
and a scan interrupts hotspot clients. So we only scan when no station is
associated with the hotspot (i.e. you are driving with the phone disconnected).
Use a second USB Wi-Fi adapter (set WD_IFACE) for uninterrupted scanning.
"""
import os
import re
import subprocess
import threading
import time

import config as C
from . import settings


class Wardriver:
    def __init__(self, gps):
        self._gps = gps
        self._lock = threading.Lock()
        self.enabled = bool(settings.load_section("wardrive", {}).get("enabled",
                                                                       C.WARDRIVE_DEFAULT_ON))
        self.scanning = False
        self.logged = 0
        self.last_total = 0
        self.new_last = 0
        self._seen = {}                       # bssid -> last time logged
        os.makedirs(C.WARDRIVE_DIR, exist_ok=True)
        self.path = os.path.join(C.WARDRIVE_DIR, "wigle_%d.csv" % int(time.time()))
        with open(self.path, "w", encoding="utf-8") as f:
            f.write("WigleWifi-1.4,appRelease=0.3,model=Raspberry Pi,release=0.3,"
                    "device=%s,display=,board=RaspberryPi,brand=RaspberryPi\n" % C.WIGLE_DEVICE)
            f.write("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,"
                    "CurrentLongitude,AltitudeMeters,AccuracyMeters,Type\n")
        threading.Thread(target=self._run, daemon=True).start()

    def set_enabled(self, e):
        self.enabled = bool(e)
        settings.save_section("wardrive", {"enabled": self.enabled})

    # ---- helpers ----------------------------------------------------------------
    @staticmethod
    def _hotspot_has_clients():
        try:
            out = subprocess.run(["iw", "dev", C.AP_IFACE, "station", "dump"],
                                 capture_output=True, text=True, timeout=5).stdout
            return "Station " in out
        except Exception:
            return False

    @staticmethod
    def _freq_to_channel(mhz):
        if 2412 <= mhz <= 2472:
            return (mhz - 2407) // 5
        if mhz == 2484:
            return 14
        if 5000 <= mhz <= 5900:
            return (mhz - 5000) // 5
        return 0

    @staticmethod
    def _auth(block):
        if "RSN:" in block and "WPA3" in block.upper() or "SAE" in block:
            return "[WPA3-SAE][ESS]"
        if "RSN:" in block:
            return "[WPA2-PSK-CCMP][ESS]"
        if "WPA:" in block:
            return "[WPA-PSK][ESS]"
        if "Privacy" in block:
            return "[WEP][ESS]"
        return "[ESS]"

    def _scan(self):
        try:
            out = subprocess.run(["iw", "dev", C.WD_IFACE, "scan"],
                                 capture_output=True, text=True, timeout=20).stdout
        except Exception:
            return []
        aps = []
        for block in re.split(r"\nBSS ", "\n" + out)[1:]:
            m = re.match(r"([0-9a-f:]{17})", block, re.I)
            if not m:
                continue
            bssid = m.group(1).lower()
            ssid = re.search(r"\n\s*SSID: (.*)", block)
            sig = re.search(r"signal: (-?[\d.]+) dBm", block)
            frq = re.search(r"freq: (\d+)", block)
            aps.append({
                "bssid": bssid,
                "ssid": (ssid.group(1).strip() if ssid else ""),
                "rssi": int(float(sig.group(1))) if sig else -99,
                "chan": self._freq_to_channel(int(frq.group(1))) if frq else 0,
                "auth": self._auth(block),
            })
        return aps

    def _run(self):
        while True:
            time.sleep(C.WARDRIVE_SCAN_S)
            if not self.enabled or self._hotspot_has_clients():
                continue
            self.scanning = True
            aps = self._scan()
            self.scanning = False
            fix = self._gps.fix()
            have_fix = fix.get("valid", False)
            when = self._gps.wigle_time()
            now = time.time()
            new = 0
            rows = []
            for ap in aps:
                if now - self._seen.get(ap["bssid"], 0) < 60:
                    continue                       # logged within the last minute
                self._seen[ap["bssid"]] = now
                new += 1
                if have_fix:
                    ssid = ap["ssid"]
                    if any(c in ssid for c in ',"\n'):
                        ssid = '"' + ssid.replace('"', '""') + '"'
                    rows.append("%s,%s,%s,%s,%d,%d,%.6f,%.6f,%.1f,%.1f,WIFI\n" % (
                        ap["bssid"], ssid, ap["auth"], when, ap["chan"], ap["rssi"],
                        fix.get("lat", 0.0), fix.get("lon", 0.0), fix.get("alt", 0.0), 10.0))
            with self._lock:
                self.last_total = len(aps)
                self.new_last = new
                if rows:
                    try:
                        with open(self.path, "a", encoding="utf-8") as f:
                            f.writelines(rows)
                        self.logged += len(rows)
                    except OSError:
                        pass
