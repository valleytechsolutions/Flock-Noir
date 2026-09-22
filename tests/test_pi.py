"""Pi parser, transport, persistence and API regressions without attached hardware."""
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"pi"))
import config as C
from flocknoir import native, settings
from flocknoir.detector import Detector
from flocknoir.gps import GPS
from flocknoir.irsensor import IrSensor
from flocknoir.logger import HitLogger
from flocknoir.radio import Radio
from flocknoir.radio_transport import advertising_reports, hci_command, prepare_monitor, radiotap
from flocknoir.wardriver import Wardriver
from flocknoir.web import create_app


def probe():
    frame = bytearray(24)
    frame[0] = 0x40
    frame[4:10] = frame[16:22] = b"\xff"*6
    frame[10:16] = bytes.fromhex("b41e52010203")
    return bytes(frame)+b"\x00\x00"


def ad_name(name):
    name = name.encode()
    return bytes([len(name)+1, 9])+name


class PiTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        base = self.temp.name
        self.config = patch.multiple(C, DATA_DIR=base, LOG_DIR=base+"/logs",
                                     RADIO_DIR=base+"/radio", WARDRIVE_DIR=base+"/wardrive",
                                     REC_DIR=base+"/videos", SETTINGS_FILE=base+"/settings.json", GPS_PORT=None)
        self.config.start()
        self.gps = GPS()
        self.ir = IrSensor(start=False)
        self.log = HitLogger(self.gps)
        self.wd = Wardriver(self.gps, start=False)
        self.cam = SimpleNamespace(detector=Detector(100), ok=True, fps=25., muted=True, jpeg=lambda: b"")
        self.buz = SimpleNamespace(enabled=True, is_playing=lambda: False, play=lambda _: None,
                                   play_alert=lambda: None, play_device_alert=lambda: None, to_json=lambda: {})
        self.state = dict(muted=True, start=time.monotonic())
        self.radio = Radio(self.gps, self.ir, self.cam, self.buz, self.wd, self.state, start=False)
        rec = SimpleNamespace(active=False, audio=False, seconds=lambda: 0, frames=lambda: 0)
        self.ctx = SimpleNamespace(camera=self.cam, gps=self.gps, buzzer=self.buz, wardriver=self.wd,
                                   irsensor=self.ir, recorder=rec, logger=self.log, state=self.state, radio=self.radio)
        self.client = create_app(self.ctx).test_client()

    def tearDown(self):
        self.radio.stop()
        self.ir.stop()
        self.wd.stop()
        self.config.stop()
        self.temp.cleanup()

    def test_shared_pulse_acceptance_and_rejection(self):
        for period, width, expected in ((100, 20, True), (100, 60, False), (20, 10, False)):
            pulse = native.Pulse()
            for ms in range(2000):
                pulse.feed(1300 if ms % period < width else 500, ms*1000)
            self.assertEqual(pulse.result()["detected"], expected)
            pulse.feed(500, 2_100_000)
            self.assertFalse(pulse.result()["detected"])
            self.assertGreater(pulse.result()["gaps"], 0)
            pulse.close()

    def test_pulse_clipping_and_rollover(self):
        pulse = native.Pulse()
        for ms in range(1000):
            pulse.feed(1300 if ms % 100 < 20 else 500, (0xffffff00+ms*1000) & 0xffffffff)
        self.assertTrue(pulse.result()["detected"])
        pulse.feed(4095, (0xffffff00+1_000_000) & 0xffffffff)
        self.assertTrue(pulse.result()["clipped"])
        self.assertFalse(pulse.result()["detected"])
        pulse.close()

    def test_signatures_and_malformed_input(self):
        result = native.decode(probe())
        self.assertEqual(result["rows"][0]["method"], "wildcard_probe")
        self.assertEqual(result["rows"][0]["tier"], 3)
        self.assertFalse(native.decode(probe()+b"\xdd\xff")["valid"])
        address = bytes.fromhex("b41e52010203")
        self.assertEqual(native.decode(b"", address, True)["rows"][0]["tier"], 1)
        self.assertEqual(native.decode(b"", address, False)["rows"][0]["tier"], 0)
        self.assertFalse(native.decode(b"\xff\x09x", address)["valid"])
        # Company alone is insufficient; composite company + service matches.
        meta = b"\x03\xff\x53\x0d"
        self.assertEqual(native.decode(meta, address)["rows"][0]["tier"], 0)
        self.assertEqual(native.decode(meta+b"\x03\x03\x5f\xfd", address)["rows"][0]["category"], "Meta glasses")
        for size in range(80):
            native.decode(bytes([255])*size)
            native.decode(bytes([255])*size, address)

    def test_remote_id(self):
        msg = bytearray(25)
        msg[0] = 2
        msg[2:9] = b"DRONE42"
        data = bytes([30, 0x16, 0xfa, 0xff, 0x0d, 0])+msg
        result = native.decode(data, bytes(6))
        self.assertEqual(result["rows"][0]["method"], "ble_remote_id")
        self.assertEqual(result["drone"]["id"], "DRONE42")
        self.assertIsNone(result["drone"]["lat"])

    def test_radiotap_bounds_fcs_and_metadata(self):
        header = struct.pack("<BBHI", 0, 0, 15, 0x2a)+b"\x10\x00"+struct.pack("<HHb", 2412, 0, -42)
        frame, signal, channel = radiotap(header+probe()+bytes(4))
        self.assertEqual((frame, signal, channel), (probe(), -42, 1))
        bad = bytearray(header+probe()+bytes(4))
        bad[8] |= 0x40
        self.assertIsNone(radiotap(bad))
        for length in range(15):
            self.assertIsNone(radiotap(header[:length]))

    def test_hci_multiple_reports_and_truncation(self):
        name = ad_name("flock")
        report = b"\x00\x00"+bytes.fromhex("030201521eb4")+bytes([len(name)])+name+b"\xd6"
        payload = b"\x02\x02"+report+report
        packet = bytes([4, 0x3e, len(payload)])+payload
        reports = advertising_reports(packet)
        self.assertEqual(len(reports), 2)
        self.assertEqual(reports[0]["address"], bytes.fromhex("b41e52010203"))
        self.assertEqual(reports[0]["rssi"], -42)
        for length in range(len(packet)):
            self.assertEqual(advertising_reports(packet[:length]), [])

    def test_hci_command_rejection(self):
        sock = SimpleNamespace(sendall=lambda _: None, recv=lambda _: bytes.fromhex("040e04010c200c"))
        with self.assertRaises(OSError):
            hci_command(sock, 0x200c, b"\x01\x00")

    def test_monitor_never_changes_dashboard(self):
        with patch("flocknoir.radio_transport.command") as cmd:
            with self.assertRaises(ValueError):
                prepare_monitor("wlan0", "wlan0", "wlan0")
            with self.assertRaises(ValueError):
                prepare_monitor("bad;command", "wlan0", "wlan0")
            cmd.assert_not_called()

    def test_gps_expiry_and_invalid_fix(self):
        gga = SimpleNamespace(sentence_type="GGA", gps_qual="1", latitude=0., longitude=1.,
                              num_sats="5", horizontal_dil="1.2", altitude="20")
        self.gps._absorb(gga)
        self.assertTrue(self.gps.fix()["valid"])
        self.gps._fix_at -= 4
        self.assertFalse(self.gps.fix()["valid"])
        self.gps._absorb(gga)
        gga.gps_qual = "0"
        self.gps._absorb(gga)
        self.assertFalse(self.gps.fix()["valid"])

    def test_optical_logs_exclude_stale_positions(self):
        self.gps._fix.update(lat=1., lon=2.)
        self.gps._valid, self.gps._fix_at = True, time.monotonic()
        self.log.hit("ir", 10, .2, .8, observed=time.monotonic()-5)
        row = self.log.recent_list()[0]
        self.assertIsNone(row["lat"])
        self.assertEqual(row["evidence"], "ir_timing_match")
        data = self.client.get("/api/status").get_json()
        self.assertFalse(data["irEn"])
        self.assertEqual(data["version"], "0.4.2")
        self.assertNotIn("NaN", self.client.get("/api/status").text)

    def test_radio_api_and_real_capture_format(self):
        response = self.client.post("/api/radio", data=dict(mode="field", ble="1", capture="1",
                                    channel="6", watch="name:flock", target=""))
        self.assertEqual(response.status_code, 200)
        observation = dict(protocol="wifi", data=probe(), timestamp=time.monotonic(), rssi=-40, channel=6)
        self.radio.process(observation)
        status = self.client.get("/api/radio").get_json()
        self.assertEqual(status["hardware"], "pi")
        self.assertEqual(status["mode"], "field")
        rows = self.client.get("/api/radio/devices").get_json()
        self.assertEqual(rows[0]["method"], "wildcard_probe")
        self.assertIn("ageMs", rows[0])
        with self.client.get("/api/radio/pcap") as response:
            capture = response.data
        self.assertEqual(struct.unpack_from("<I", capture, 20)[0], 105)
        self.assertEqual(capture[40:], probe())
        with self.client.get("/api/radio/log") as response:
            log = response.text
        self.assertIsNone(json.loads(log)["lat"])
        self.assertGreater(len(self.client.get("/api/radio/files").get_json()), 0)
        for name in ("../settings.json", "../../etc/passwd", "settings.json"):
            self.assertEqual(self.client.get("/api/radio/file", query_string={"name": name}).status_code, 400)
        self.client.post("/api/radio/clear")
        self.assertEqual(self.client.get("/api/radio/devices").get_json(), [])
        self.assertTrue(self.radio.paths["pcap"].exists())

    def test_survey_never_fabricates_captured_packets(self):
        self.radio.capture = True
        ap = dict(bssid="b4:1e:52:01:02:03", ssid="flock", rssi=-45, chan=1)
        self.radio.process(dict(protocol="wifi", data=b"", survey=ap, rssi=-45, channel=1, timestamp=time.monotonic()))
        self.assertEqual(self.radio.rows()[0]["method"], "survey_ssid")
        self.assertFalse(self.radio.paths["pcap"].exists())

    def test_watch_validation_persistence_and_caps(self):
        for watch in ("cid:12", "mac:GG:11:22:33:44:55", "wrong:x"):
            self.assertEqual(self.client.post("/api/radio", data=dict(mode="dashboard", channel=1, watch=watch)).status_code, 400)
        settings.save_section("irsensor", {"enabled": False})
        self.radio.configure("dashboard", True, True, "name:hello", "", 1)
        self.assertFalse(settings.load_section("irsensor")["enabled"])
        self.radio.sizes["ble"] = C.RADIO_CAPTURE_LIMIT-1
        self.radio.process(dict(protocol="ble", data=ad_name("hello"), address=bytes(6), address_type=1,
                                event_type=0, rssi=-40, channel=0, timestamp=time.monotonic()))
        self.assertTrue(self.radio.status()["captureFull"])
        self.assertFalse(self.radio.paths["ble"].exists())

    def test_camera_compactness_and_staleness(self):
        for blob, expected in ((1, True), (100, False)):
            detector = Detector(100)
            for ms in range(2000):
                high = ms % 100 < 20
                detector.feed(200 if high else 10, blob if high else 0, ms*1000)
            self.assertEqual(detector.analyze().detected, expected)
            detector._fed_at -= 1
            self.assertFalse(detector.last().detected)

    def test_requested_devices_log_and_mario_alert_without_ir(self):
        cases = ((ad_name("Penguin-1234567890"), "Flock battery"),
                 (ad_name("1234567890"), "Flock battery"),
                 (ad_name("DfuTarg"), "Flock DFU"),
                 (bytes.fromhex("03ff4d03"), "Axon"),
                 (bytes.fromhex("03ff530d03035ffd"), "Meta"),
                 (bytes.fromhex("0303823003190086"), "Flipper"))
        self.state["muted"] = False
        for i, (data, category) in enumerate(cases):
            with self.subTest(category=category), patch.object(self.buz, "play_device_alert") as tone:
                observation = dict(protocol="ble", data=data, address=bytes([2,0,0,0,0,i]),
                                   address_type=1, event_type=0, rssi=-45, channel=0, timestamp=time.monotonic())
                self.radio.last_alert = None
                self.radio.process(observation)
                self.radio._play_pending()
                tone.assert_called_once()
                self.radio.process(observation)
                self.radio._play_pending()
                tone.assert_called_once()  # duplicate packets do not replay sound
                record = json.loads(self.radio.paths["log"].read_text().splitlines()[-1])
                self.assertIn(category, record["category"])
                self.assertFalse(record["ir_timing_match"])
                self.assertIsNone(record["lat"])
        self.assertIsNotNone(self.client.get("/api/status").get_json()["radioAlert"])
        self.assertEqual(self.client.get("/api/status").get_json()["radioEvents"], len(cases))
        self.assertFalse(self.ir.enabled)

    def test_alert_busy_cooldown_mute_and_reappearance(self):
        self.state["muted"] = False
        observation = dict(protocol="ble", data=ad_name("Flipper Test"), address=bytes(6),
                           address_type=1, event_type=0, rssi=-45, channel=0, timestamp=time.monotonic())
        with patch.object(self.buz, "play_device_alert") as tone:
            self.radio.process(observation)
            with patch.object(self.buz, "is_playing", return_value=True):
                self.radio._play_pending()
                tone.assert_not_called()
            self.radio._play_pending()
            tone.assert_called_once()
            self.radio.pending_alert = time.monotonic()
            self.radio._play_pending()
            tone.assert_called_once()
            self.state["muted"] = True
            self.radio._play_pending()
            self.assertIsNone(self.radio.pending_alert)
            self.state["muted"] = False
            self.radio.last_alert = None
            next(iter(self.radio.devices.values()))["attention_seen"] -= 61
            self.radio.process(observation)
            self.radio._play_pending()
            self.assertEqual(tone.call_count, 2)
            for d in self.radio.devices.values():
                d["matched"] -= 9
            self.assertIsNone(self.radio.alert())

    def test_pineapple_ap_not_client_or_ordinary_ssid(self):
        address = bytes.fromhex("101112010203")
        self.assertFalse(native.decode_survey(address, "Flock Noir")["rows"][0]["tier"])
        for ssid in ("Pineapple_1337", "Pineapple_Management", "WiFi Pineapple"):
            result = native.decode_survey(address, ssid)["rows"][0]
            self.assertIn("Pineapple", result["category"])
            self.assertFalse(result["alpr"])
        for ssid in ("Open", "Pineapple Pizza", "Pineapple_ZZZZ"):
            self.assertFalse(native.decode_survey(address, ssid)["rows"][0]["tier"])

    def test_camera_assessment_uses_radio_and_fresh_optical_evidence(self):
        self.assertEqual(native.assessment(True,2),"possible_camera")
        self.assertEqual(native.assessment(True,4),"camera_signature_match")
        self.assertEqual(native.assessment(True,2,True),"corroborated_camera_candidate")
        self.assertEqual(native.assessment(True,1,True),"possible_camera")
        self.assertEqual(native.assessment(False,3,True),"device_candidate")
        when=time.monotonic()
        self.radio.ir_at=when-2
        self.radio.process(dict(protocol="wifi",data=probe(),rssi=-40,channel=1,timestamp=when))
        record=json.loads(self.radio.paths["log"].read_text().splitlines()[-1])
        self.assertEqual(record["assessment"],"corroborated_camera_candidate")
        self.assertTrue(record["ir_timing_match"])
        self.radio.ir_at=when-4
        self.assertEqual(self.radio.alert()["assessment"],"camera_signature_match")

    def test_optical_upgrade_is_logged_inside_duplicate_window(self):
        self.state["muted"] = False
        when = time.monotonic()
        observation = dict(protocol="wifi",data=probe(),rssi=-40,channel=1,timestamp=when)
        self.radio.process(observation)
        self.radio.pending_alert = None
        self.radio.ir_at = when
        self.radio.process(dict(observation,timestamp=when+.1))
        records = [json.loads(line) for line in self.radio.paths["log"].read_text().splitlines()]
        self.assertEqual(len(records),2)
        self.assertEqual(records[0]["assessment"],"camera_signature_match")
        self.assertEqual(records[1]["assessment"],"corroborated_camera_candidate")
        self.assertIsNotNone(self.radio.pending_alert)
        self.radio.process(dict(observation,timestamp=when+.2))
        self.assertEqual(self.radio.events,2)
        self.radio.ir_at = None
        self.radio.process(dict(observation,timestamp=when+.3))
        self.radio.ir_at = when
        self.radio.process(dict(observation,timestamp=when+.4))
        self.assertEqual(self.radio.events,2)  # intermittent light does not spam upgrades

    def test_pineapple_candidate_logs_and_alerts(self):
        self.state["muted"] = False
        ap = dict(bssid="10:11:12:01:02:03",ssid="Pineapple_1337",rssi=-45,chan=1)
        with patch.object(self.buz,"play_device_alert") as tone:
            self.radio.process(dict(protocol="wifi",data=b"",survey=ap,rssi=-45,channel=1,timestamp=time.monotonic()))
            self.radio._play_pending()
            tone.assert_called_once()
        record = json.loads(self.radio.paths["log"].read_text().splitlines()[-1])
        self.assertEqual(record["assessment"],"device_candidate")
        self.assertIn("Pineapple",record["category"])


if __name__ == "__main__":
    unittest.main()
