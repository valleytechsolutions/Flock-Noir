"""Flock Noir - Raspberry Pi target - entry point.

Run from the pi/ directory:   python -m flocknoir.main
(installed as the 'flocknoir' systemd service by pi/install.sh)
"""
import os
import threading
import time
from types import SimpleNamespace

import config as C
from .buzzer import Buzzer
from .camera import Camera
from .gps import GPS
from .irsensor import IrSensor
from .logger import HitLogger
from .recorder import Recorder
from .wardriver import Wardriver
from .web import create_app
from .radio import Radio
from . import __version__


def main():
    for d in (C.DATA_DIR, C.LOG_DIR, C.WARDRIVE_DIR, C.REC_DIR):
        os.makedirs(d, exist_ok=True)

    print("=== Flock Noir (Raspberry Pi) v%s ===" % __version__, flush=True)
    state = {"muted": False, "start": time.monotonic()}

    gps = GPS()
    log = HitLogger(gps)
    buz = Buzzer()
    wd = Wardriver(gps)
    ir = IrSensor()
    radio = None

    def on_camera_detect(d, cx, cy):
        if radio: radio.optical("camera", time.monotonic())
        nearby = radio.nearby_alpr(time.monotonic()) if radio else None
        log.hit("camera", d.freqHz, d.dutyCycle, d.confidence, cx, cy, d.blobFrac, d.levelPP, nearby=nearby, fusion=radio.fusion() if radio else None)
        if not state["muted"]:
            buz.request_alert("alpr_combined" if nearby else "camera")

    cam = Camera(on_camera_detect)
    rec = Recorder(cam)
    radio = Radio(gps, ir, cam, buz, wd, state, logger=log)
    wd.on_scan = radio.observe_survey
    print("[CAM] %s" % ("ok" if cam.ok else "NOT AVAILABLE (is the camera connected / picamera2 installed?)"),
          flush=True)
    print("[GPS] port=%s baud=%d" % (C.GPS_PORT, C.GPS_BAUD), flush=True)
    print("[LOG] %s" % log.path, flush=True)

    # IR events are latched by the sampling thread, independent of camera alerts.
    def ir_loop():
        while True:
            r = ir.pop_event()
            if r is None:
                continue
            conf = min(1., r["validCount"]/8.)
            evidence = "ir_radio_nearby" if radio.recent_alpr(r["timestamp"]) else "ir_timing_match"
            radio.optical("ir", r["timestamp"])
            nearby = radio.nearby_alpr(r["timestamp"])
            log.hit("ir", r["freqHz"], r["dutyCycle"], conf, level_pp=r["amp"],
                    observed=r["timestamp"], evidence=evidence, nearby=nearby, fusion=radio.fusion(r["timestamp"]))
            if not state["muted"]:
                buz.request_alert("alpr_combined" if nearby else "alpr_ir")
    threading.Thread(target=ir_loop, daemon=True).start()

    # GPS health heartbeat (serial console), every 5 s
    def gps_heartbeat():
        while True:
            time.sleep(5)
            h = gps.health(); f = gps.fix()
            print("[GPS] chars=%d good=%d fail=%d sats=%d fix=%d" %
                  (h["chars"], h["good"], h["fail"], f.get("sats", 0), 1 if f.get("valid") else 0),
                  flush=True)
    threading.Thread(target=gps_heartbeat, daemon=True).start()

    if buz.enabled and C.BUZZER_STARTUP_TUNE:
        buz.play(C.BUZZER_STARTUP_TUNE)

    ctx = SimpleNamespace(camera=cam, gps=gps, buzzer=buz, wardriver=wd, irsensor=ir,
                          recorder=rec, logger=log, state=state, radio=radio)
    app = create_app(ctx)
    print("[WEB] http://%s:%d/" % (C.AP_IP, C.WEB_PORT), flush=True)
    try:
        app.run(host="0.0.0.0", port=C.WEB_PORT, threaded=True, use_reloader=False)
    finally:
        radio.stop()
        ir.stop()
        wd.stop()
        rec.stop()
        buz.stop()


if __name__ == "__main__":
    main()
