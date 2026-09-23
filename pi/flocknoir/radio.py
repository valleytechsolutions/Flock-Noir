"""Pi radio scanning, bounded live table, watchlists, captures and evidence logs."""
from collections import OrderedDict
import copy
import json
import os
from pathlib import Path
import queue
import re
import socket
import struct
import threading
import time
import uuid

import config as C
from . import native, settings
from .alerts import classify
from .radio_transport import (advertising_reports, command, hci_command,
                              open_hci, prepare_monitor, radiotap)


class Radio:
    def __init__(self, gps, ir, camera, buzzer, wardriver, state, start=True, logger=None):
        self.gps, self.ir, self.camera, self.buzzer = gps, ir, camera, buzzer
        self.wardriver, self.state = wardriver, state
        self.logger = logger
        self.lock = threading.RLock()
        self.stop_event = threading.Event()
        self.queue = queue.Queue(maxsize=256)
        self.devices = OrderedDict()
        prefs = settings.load_section("radio", {})
        self.ble = bool(prefs.get("ble", C.RADIO_BLE_DEFAULT))
        self.watch, self.target = prefs.get("watch", ""), prefs.get("target", "")
        self.channel = self.dashboard_channel = int(prefs.get("channel", 1))
        if self.channel not in C.RADIO_CHANNELS:
            self.channel = self.dashboard_channel = 1
        self.mode, self.capture = "dashboard", False
        self.hop = prefs.get("hop", "priority")
        self.profile = settings.load_section("profile", "general")
        self.wifi_ready = self.ble_ready = self.ble_scanning = False
        self.wifi_error = self.ble_error = self.native_error = ""
        self.packets = self.dropped = self.log_errors = 0
        self.last_alpr = self.last_tone = 0.
        self.events = self.audible_alerts = 0
        self.ir_at = self.camera_at = None
        self.started = time.monotonic()
        self.session = uuid.uuid4().hex[:12]
        self.paths = {"log": Path(C.RADIO_DIR)/("events_"+self.session+".jsonl"),
                      "pcap": Path(C.RADIO_DIR)/("wifi_"+self.session+".pcap"),
                      "ble": Path(C.RADIO_DIR)/("ble_"+self.session+".jsonl")}
        self.sizes = {key: 0 for key in self.paths}
        try:
            Path(C.RADIO_DIR).mkdir(parents=True, exist_ok=True)
        except OSError:
            self.log_errors += 1
        try:
            native.library()
        except OSError as exc:
            self.native_error = "Native parser unavailable; rerun pi/install.sh: " + str(exc)
        if start and not self.native_error:
            for target, name in ((self._wifi, "wifi-monitor"), (self._ble, "ble-scanner"), (self._worker, "radio-log")):
                threading.Thread(target=target, daemon=True, name=name).start()

    def set_profile(self, profile):
        if profile not in ("alpr", "general"):
            raise ValueError("Invalid detection profile")
        with self.lock:
            if profile == "alpr":
                self.configure(self.mode, True, self.capture, self.watch, self.target, self.dashboard_channel, "priority")
            settings.save_section("profile", profile)
            self.profile = profile
            self.buzzer.alert_queue.clear()
            self.buzzer.stop()
            for d in self.devices.values():
                d["alert_tier"] = 0

    def optical(self, source, when):
        with self.lock:
            attr = "ir_at" if source == "ir" else "camera_at"
            old = getattr(self, attr)
            if old is None or when >= old:
                setattr(self, attr, when)

    def fusion(self, when=None):
        when = time.monotonic() if when is None else when
        with self.lock:
            if self.native_error:
                return dict(windowMs=3000,method="unknown",assessment="unavailable",radioTier=0,mask=0,ageMs=[None]*4)
            stamps = [self.ir_at, self.camera_at, None, None]
            tiers = [0, 0, 0, 0]
            for d in self.devices.values():
                if not d.get("alpr") or not d.get("tier"):
                    continue
                i = 2 if d["protocol"] == "ble" else 3
                at = d.get("matched", 0)
                if abs(when-at)>3:
                    continue
                tiers[i] = max(tiers[i], d["tier"])
                if stamps[i] is None or at > stamps[i]:
                    stamps[i] = at
            ages = [min(0x7fffffff, int(abs(when-at)*1000)) if at is not None else -1 for at in stamps]
            return native.fusion(ages, tiers)

    def configure(self, mode, ble, capture, watch, target, channel, hop="priority"):
        if hop not in ("priority", "all"):
            raise ValueError("Invalid hop plan")
        channel = int(channel)
        target = target.strip().upper()
        if mode not in ("dashboard", "field") or channel not in C.RADIO_CHANNELS or len(watch) > 1024:
            raise ValueError("Invalid radio settings")
        if target and not re.fullmatch(r"(?:[0-9A-F]{2}:){5}[0-9A-F]{2}", target):
            raise ValueError("Target must be a MAC address")
        for line in watch.splitlines():
            if not line.strip():
                continue
            kind, sep, value = line.strip().partition(":")
            value = value.strip()
            patterns = {"mac": r"(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}",
                        "oui": r"(?:[0-9a-fA-F]{2}:){2}[0-9a-fA-F]{2}",
                        "cid": r"[0-9a-fA-F]{4}", "svc": r"[0-9a-fA-F]{4}", "name": r"[^\r\n]{1,63}"}
            if not sep or kind not in patterns or not re.fullmatch(patterns[kind], value):
                raise ValueError("Invalid watchlist rule")
        settings.save_section("radio", dict(ble=bool(ble), watch=watch, target=target, channel=channel, hop=hop))
        with self.lock:
            self.mode, self.ble, self.capture = mode, bool(ble), bool(capture)
            self.hop = hop
            self.watch, self.target, self.dashboard_channel = watch, target, channel

    def status(self):
        with self.lock:
            return dict(supported=True, hardware="pi", profile=self.profile, mode=self.mode, channel=self.channel, hop=self.hop,
                        ble=self.ble, bleReady=self.ble_ready, bleScanning=self.ble_scanning,
                        wifiReady=self.wifi_ready, capture=self.capture, packets=self.packets,
                        dropped=self.dropped, logErrors=self.log_errors, freeHeap=0, minFreeHeap=0,
                        captureFull=any(self.sizes[k] >= C.RADIO_CAPTURE_LIMIT for k in ("pcap", "ble")),
                        watch=self.watch, target=self.target,
                        events=self.events, audibleAlerts=self.audible_alerts,
                        detail="; ".join(e for e in (self.native_error, self.wifi_error, self.ble_error) if e),
                        modeHint="Pi: field mode hops the dedicated monitor adapter. The dashboard hotspot stays on.")

    def rows(self):
        with self.lock:
            return [dict(copy.deepcopy(d), ageMs=max(0, int((time.monotonic()-d["seen"])*1000)))
                    for d in reversed(self.devices.values())]

    def clear_live(self):
        with self.lock:
            self.devices.clear()
            self.last_alpr = 0.
            self.ir_at = self.camera_at = None
            self.buzzer.alert_queue.clear()

    def alert(self, alpr_only=False):
        with self.lock:
            recent = [d for d in self.devices.values() if d.get("priority", 0) and (not alpr_only or d.get("alpr")) and
                      time.monotonic()-d.get("matched", 0) <= 8]
            if not recent:
                return None
            best = max(recent, key=lambda d: (d["priority"], d["matched"]))
            ir, camera = self._optical_near(best["matched"])
            return dict({k: best[k] for k in ("category", "method", "tier", "alpr", "mac", "rssi")},
                        assessment=native.assessment(best["alpr"],best["tier"],ir,camera),
                        ir_timing_match=ir,camera_pattern=camera)

    def _optical_near(self, when):
        return (self.ir_at is not None and abs(when-self.ir_at)<=3,
                self.camera_at is not None and abs(when-self.camera_at)<=3)

    def _update_optical(self):
        result = self.ir.result()
        if result["detected"]:
            self.ir_at = result["timestamp"]
        if self.camera.detector.last().detected:
            self.camera_at = time.monotonic()

    def _play_pending(self):
        with self.lock:
            self._update_optical()
            if self.buzzer.update(self.state["muted"]):
                self.audible_alerts += 1

    def nearby_alpr(self, when):
        with self.lock:
            nearby = [d for d in self.devices.values() if d.get("alpr") and d.get("tier",0)>=2
                      and abs(when-d.get("matched",0))<=3]
            return copy.deepcopy(max(nearby, key=lambda d:d["tier"])) if nearby else None

    def recent_alpr(self, when=None):
        when = time.monotonic() if when is None else when
        with self.lock:
            return self.last_alpr > 0 and abs(when-self.last_alpr) <= 3

    def stop(self):
        self.stop_event.set()

    def enqueue(self, protocol, data, **metadata):
        with self.lock:
            self.packets += 1
        observation = dict(protocol=protocol, data=bytes(data[:4096]), timestamp=time.monotonic(), **metadata)
        try:
            self.queue.put_nowait(observation)
        except queue.Full:
            with self.lock:
                    self.dropped += 1

    def observe_survey(self, ap):
        self.enqueue("wifi", b"", survey=ap, rssi=ap["rssi"], channel=ap["chan"])

    def _wifi(self):
        interface = C.RADIO_MONITOR_IFACE
        while not self.stop_event.is_set():
            try:
                prepare_monitor(interface, C.AP_IFACE, C.WD_IFACE)
                with socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(3)) as sock:
                    sock.bind((interface, 0))
                    sock.settimeout(0.1)
                    last_hop, active = 0., 0
                    while not self.stop_event.is_set():
                        now = time.monotonic()
                        wanted = self.dashboard_channel
                        if self.mode == "field":
                            channels = C.RADIO_CHANNELS if self.hop == "all" else (1,6,11)
                            wanted = channels[(channels.index(active)+1) % len(channels)] if active in channels else 1
                        if not active or (self.mode == "field" and now-last_hop >= C.RADIO_DWELL_S) or (self.mode == "dashboard" and active != wanted):
                            command(["iw", "dev", interface, "set", "channel", str(wanted)])
                            self.channel, active, last_hop = wanted, wanted, now
                            self.wifi_ready, self.wifi_error = True, ""
                        try:
                            data, _, flags, address = sock.recvmsg(4096)
                        except socket.timeout:
                            continue
                        if flags & socket.MSG_TRUNC or address[2] == 4:
                            continue
                        frame = radiotap(data) if address[3] == 803 else (data, -127, active) if address[3] == 801 else None
                        if frame and len(frame[0]) >= 24:
                            self.enqueue("wifi", frame[0], rssi=frame[1], channel=frame[2] or active)
            except Exception as exc:
                self.wifi_ready, self.wifi_error = False, str(exc)[:240]
                self.stop_event.wait(5)

    def _ble(self):
        while not self.stop_event.is_set():
            if not self.ble:
                self.stop_event.wait(0.25)
                continue
            sock = None
            try:
                sock = open_hci(C.RADIO_HCI_INDEX)
                self.ble_ready, self.ble_scanning, self.ble_error = True, True, ""
                while self.ble and not self.stop_event.is_set():
                    try:
                        packet = sock.recv(260)
                    except socket.timeout:
                        continue
                    for report in advertising_reports(packet):
                        data = report.pop("data")
                        self.enqueue("ble", data, channel=0, **report)
            except Exception as exc:
                self.ble_ready, self.ble_error = False, str(exc)[:240]
            finally:
                self.ble_scanning = False
                if sock is not None:
                    try:
                        hci_command(sock, 0x200C, b"\x00\x00")
                    except OSError:
                        pass
                    sock.close()
            self.stop_event.wait(3 if self.ble else 0.25)

    def _write(self, kind, data, limited=False):
        if limited and self.sizes[kind]+len(data) > C.RADIO_CAPTURE_LIMIT:
            self.sizes[kind] = C.RADIO_CAPTURE_LIMIT
            return
        try:
            with self.paths[kind].open("ab") as file:
                if file.write(data) != len(data):
                    raise OSError("Short write")
            self.sizes[kind] += len(data)
        except OSError:
            self.log_errors += 1

    @staticmethod
    def _json_bytes(row):
        return (json.dumps(row, allow_nan=False, separators=(",", ":"))+"\n").encode()

    def _capture(self, observation):
        if not self.capture or "survey" in observation:
            return
        if observation["protocol"] == "ble":
            row = dict(uptime_ms=int(observation["timestamp"]*1000),
                       mac=observation["address"].hex(":"), rssi=observation["rssi"],
                       address_type=observation["address_type"], event_type=observation["event_type"],
                       advertisement_hex=observation["data"].hex())
            self._write("ble", self._json_bytes(row), True)
        else:
            header = struct.pack("<IHHIIII", 0xa1b2c3d4, 2, 4, 0, 0, 4096, 105) if not self.sizes["pcap"] else b""
            stamp = max(0, observation["timestamp"])
            data = observation["data"]
            packet = struct.pack("<IIII", int(stamp), int(stamp % 1 * 1_000_000), len(data), len(data))+data
            self._write("pcap", header+packet, True)

    def _watch_match(self, row, packet):
        for line in self.watch.splitlines():
            kind, _, value = line.strip().partition(":")
            value = value.strip()
            hit = (kind == "mac" and row["mac"] == value.upper() or
                   kind == "oui" and row["mac"].startswith(value.upper()) or
                   kind == "name" and value and value.lower() in packet["name"].lower())
            if kind in ("cid", "svc") and re.fullmatch("[0-9A-Fa-f]{4}", value):
                hit = int(value, 16) in packet.get("companies" if kind == "cid" else "services", [])
            if hit:
                row.update(category="Watchlist", method=kind, tier=1 if kind in ("mac", "oui") else 2, alpr=False,
                           priority=0 if kind in ("mac", "oui") else 2)
                return

    def process(self, observation):
        ble = observation["protocol"] == "ble"
        if "survey" in observation:
            ap = observation["survey"]
            packet = native.decode_survey(bytes.fromhex(ap["bssid"].replace(":", "")), ap["ssid"])
        else:
            packet = native.decode(observation["data"], observation.get("address"), observation.get("address_type") == 0)
        with self.lock:
            self._update_optical()
            self._capture(observation)
            if not packet["valid"]:
                return
            if not ble and packet.get("beacon"):
                address = observation["data"][10:16].hex(":")
                self.wardriver.observe_passive(address, packet["name"], observation["channel"],
                                               observation["rssi"], packet["privacy"], observation["timestamp"])
            seen = set()
            for row in packet["rows"]:
                if row["mac"] in seen:
                    continue
                seen.add(row["mac"])
                if not row["tier"]:
                    self._watch_match(row, packet)
                if not ble and not row["tier"] and row["mac"] != self.target:
                    continue
                self._observe(observation, packet, row)

    def _observe(self, observation, packet, row):
        when, protocol = observation["timestamp"], observation["protocol"]
        key = protocol + row["mac"]
        old = self.devices.get(key, {})
        ir, camera = self._optical_near(when)
        corroborated = row["alpr"] and row["tier"] >= 2 and (ir or camera)
        optical_upgrade = corroborated and (not old.get("corroborated") or when-old.get("seen", 0) >= 60)
        stronger = row["tier"] > old.get("tier", 0) or optical_upgrade
        d = dict(old, mac=row["mac"], protocol=protocol, name=packet["name"] or old.get("name", ""),
                 rssi=observation["rssi"], seen=when, count=old.get("count", 0)+1)
        d["corroborated"] = corroborated or (old.get("corroborated",False) and when-old.get("seen",0)<60)
        if row["tier"] and (row["tier"] >= old.get("tier", 0) or when-old.get("matched", 0) > 8):
            d.update(category=row["category"], method=row["method"], tier=row["tier"],
                     alpr=row["alpr"], priority=row["priority"], matched=when)
        if row["priority"]:
            fresh = not old.get("alert_tier") or when-old.get("attention_seen", 0) >= 60
            if (fresh or optical_upgrade or row["tier"] > old.get("alert_tier", 0)) and (self.profile != "alpr" or row["alpr"]) and not self.state["muted"] and self.buzzer.enabled:
                self.buzzer.request_alert(classify(row, protocol, ir, camera))
            d.update(attention_seen=when, alert_tier=row["tier"] if fresh else max(row["tier"], old.get("alert_tier", 0)))
        drone = dict(old.get("drone", {}))
        incoming = packet["drone"] if row["slot"] == 0 else {}
        for field, value in incoming.items():
            if value is not None and value != "":
                drone[field] = value
        if incoming.get("types", 0) & 2:
            for field in ("lat", "lon", "altitude", "speed", "heading"):
                drone[field] = incoming.get(field)
        if incoming.get("types", 0) & 16:
            for field in ("pilotLat", "pilotLon"):
                drone[field] = incoming.get(field)
        drone["types"] = old.get("drone", {}).get("types", 0) | incoming.get("types", 0)
        d.update(drone=drone, droneId=drone.get("id", ""), droneLat=drone.get("lat"), droneLon=drone.get("lon"))
        self.devices[key] = d
        self.devices.move_to_end(key)
        while len(self.devices) > C.RADIO_DEVICE_LIMIT:
            self.devices.popitem(last=False)
        if row["alpr"] and row["tier"] >= 2:
            self.last_alpr = when
        if row["mac"] == self.target and row["slot"] == 0 and (self.profile != "alpr" or row["alpr"]) and not self.state["muted"] and self.buzzer.enabled:
            interval = max(0.15, min(2., (-observation["rssi"]-25)/35))
            if when-self.last_tone >= interval and not self.buzzer.is_playing():
                self.buzzer.play("Track:d=32,o=6,b=200:c")
                self.last_tone = when
        if not row["tier"] or (not stronger and when-old.get("logged", 0) < 10):
            return
        d["logged"] = when
        self.events += 1
        fix = self.gps.fix()
        valid = bool(fix.get("valid")) and time.monotonic()-when <= C.GPS_MAX_AGE_S
        record = dict(schema=1, time=self.gps.iso_utc(), uptime_ms=int(when*1000),
                      protocol=protocol, mac=d["mac"], name=d["name"], category=row["category"],
                      method=row["method"], tier=row["tier"], rssi=d["rssi"], channel=observation["channel"],
                      gps_valid=valid, lat=fix.get("lat") if valid else None, lon=fix.get("lon") if valid else None,
                      ir_timing_match=ir, camera_pattern=camera, alpr=row["alpr"],
                      assessment=native.assessment(row["alpr"],row["tier"],ir,camera),
                      detection_method=self.fusion(when)["method"] if row["alpr"] else protocol,
                      evidence="ir_radio_nearby" if ir and row["alpr"] and row["tier"] >= 2 else "radio_candidate",
                      drone_id=d["droneId"], drone_lat=d["droneLat"], drone_lon=d["droneLon"],
                      operator_id=drone.get("operatorId", ""), altitude_m=drone.get("altitude"),
                      speed_mps=drone.get("speed"), heading_deg=drone.get("heading"),
                      pilot_lat=drone.get("pilotLat"), pilot_lon=drone.get("pilotLon"), odid_types=drone["types"])
        self._write("log", self._json_bytes(record))
        if self.logger is not None:
            self.logger.radio_hit(record)

    def _worker(self):
        while not self.stop_event.is_set():
            try:
                observation = self.queue.get(timeout=0.2)
            except queue.Empty:
                self._play_pending()
                continue
            try:
                self.process(observation)
                self._play_pending()
            except (ValueError, OSError, KeyError):
                with self.lock:
                    self.log_errors += 1
