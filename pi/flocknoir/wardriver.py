"""WiGLE logging from passive surveys and dedicated monitor-radio beacons."""
from collections import OrderedDict
import csv
import os
import re
import subprocess
import threading
import time
import uuid

import config as C
from . import settings, __version__


class Wardriver:
    def __init__(self, gps, start=True):
        self._gps = gps
        self._lock = threading.RLock()
        self._stop = threading.Event()
        self.enabled = bool(settings.load_section("wardrive", {}).get("enabled", C.WARDRIVE_DEFAULT_ON))
        self.scanning = False
        self.logged = self.last_total = self.new_last = self.errors = 0
        self.error = ""
        self._seen = OrderedDict()
        self.on_scan = None
        self.path = os.path.join(C.WARDRIVE_DIR, "wigle_"+uuid.uuid4().hex[:12]+".csv")
        self.ready = False
        try:
            os.makedirs(C.WARDRIVE_DIR, exist_ok=True)
            with open(self.path, "w", encoding="utf-8", newline="") as file:
                file.write("WigleWifi-1.4,appRelease=%s,model=Raspberry Pi,release=%s,"
                           "device=%s,display=,board=RaspberryPi,brand=RaspberryPi\n" %
                           (__version__, __version__, C.WIGLE_DEVICE))
                file.write("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,"
                           "CurrentLongitude,AltitudeMeters,AccuracyMeters,Type\n")
            self.ready = True
        except OSError as exc:
            self.errors += 1
            self.error = str(exc)
        if start:
            threading.Thread(target=self._run, daemon=True, name="wardriver").start()

    def set_enabled(self, enabled):
        settings.save_section("wardrive", {"enabled": bool(enabled)})
        self.enabled = bool(enabled)

    def stop(self):
        self._stop.set()

    @staticmethod
    def _hotspot_has_clients():
        if C.WD_IFACE != C.AP_IFACE:
            return False
        try:
            result = subprocess.run(["iw", "dev", C.AP_IFACE, "station", "dump"],
                                    capture_output=True, text=True, timeout=5)
            return result.returncode != 0 or "Station " in result.stdout
        except OSError:
            return True
        except subprocess.TimeoutExpired:
            return True

    @staticmethod
    def _freq_to_channel(mhz):
        if 2412 <= mhz <= 2472:
            return (mhz-2407)//5
        if mhz == 2484:
            return 14
        return (mhz-5000)//5 if 5000 <= mhz <= 5900 else 0

    @staticmethod
    def _auth(block):
        if "SAE" in block:
            return "[WPA3-SAE][ESS]"
        if "RSN:" in block:
            return "[WPA2-PSK-CCMP][ESS]"
        if "WPA:" in block:
            return "[WPA-PSK][ESS]"
        return "[PRIVACY][ESS]" if "Privacy" in block else "[ESS]"

    def _scan(self):
        result = subprocess.run(["iw", "dev", C.WD_IFACE, "scan", "passive"],
                                capture_output=True, text=True, timeout=20)
        if result.returncode:
            raise OSError(result.stderr.strip()[:160] or "WiFi survey failed")
        aps = []
        for block in re.split(r"\nBSS ", "\n"+result.stdout)[1:]:
            match = re.match(r"([0-9a-f:]{17})", block, re.I)
            if not match:
                continue
            ssid = re.search(r"\n\s*SSID: (.*)", block)
            signal = re.search(r"signal: (-?[\d.]+) dBm", block)
            frequency = re.search(r"freq: (\d+)", block)
            aps.append(dict(bssid=match.group(1).lower(), ssid=ssid.group(1).strip()[:128] if ssid else "",
                            rssi=int(float(signal.group(1))) if signal else -127,
                            chan=self._freq_to_channel(int(frequency.group(1))) if frequency else 0,
                            auth=self._auth(block)))
        return aps

    def _record(self, aps, observed):
        if not self.enabled:
            return
        fix = self._gps.fix()
        now = time.monotonic()
        if not fix.get("valid") or now-observed > C.GPS_MAX_AGE_S:
            return
        with self._lock:
            self.last_total = len(aps)
            self.new_last = 0
            for ap in aps:
                key = ap["bssid"].lower()
                if now-self._seen.get(key, -100) < 60:
                    continue
                try:
                    if not self.ready:
                        raise OSError("WiGLE log unavailable")
                    with open(self.path, "a", encoding="utf-8", newline="") as file:
                        csv.writer(file).writerow([key, ap["ssid"], ap["auth"], self._gps.wigle_time(),
                                                  ap["chan"], ap["rssi"], fix["lat"], fix["lon"],
                                                  fix.get("alt", 0.), max(1., fix.get("hdop", 2.)*5), "WIFI"])
                    self._seen[key] = now
                    self._seen.move_to_end(key)
                    while len(self._seen) > 2048:
                        self._seen.popitem(last=False)
                    self.logged += 1
                    self.new_last += 1
                except OSError as exc:
                    self.errors += 1
                    self.error = str(exc)

    def observe_passive(self, address, ssid, channel, rssi, privacy, observed):
        self._record([dict(bssid=address, ssid=ssid, chan=channel, rssi=rssi,
                           auth="[PRIVACY][ESS]" if privacy else "[ESS]")], observed)

    def _run(self):
        while not self._stop.wait(C.WARDRIVE_SCAN_S):
            if not self.enabled or self._hotspot_has_clients():
                continue
            self.scanning = True
            try:
                aps = self._scan()
                self.error = ""
                self.last_total = len(aps)
                if self.on_scan:
                    for ap in aps:
                        self.on_scan(ap)
                self._record(aps, time.monotonic())
            except (OSError, subprocess.TimeoutExpired, ValueError) as exc:
                self.error = str(exc)
            finally:
                self.scanning = False
