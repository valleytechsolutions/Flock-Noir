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


def main():
    for d in (C.DATA_DIR, C.LOG_DIR, C.WARDRIVE_DIR, C.REC_DIR):
        os.makedirs(d, exist_ok=True)

    print("=== Flock Noir (Raspberry Pi) v0.3 ===", flush=True)
    state = {"muted": False, "start": time.time()}

    gps = GPS()
    log = HitLogger(gps)
    buz = Buzzer()
    wd = Wardriver(gps)
    ir = IrSensor()

    def on_camera_detect(d, cx, cy):
        log.hit("camera", d.freqHz, d.dutyCycle, d.confidence, cx, cy, d.blobFrac, d.levelPP)
        if not state["muted"]:
            buz.play_alert()

    cam = Camera(on_camera_detect)
    rec = Recorder(cam)
    print("[CAM] %s" % ("ok" if cam.ok else "NOT AVAILABLE (is the camera connected / picamera2 installed?)"),
          flush=True)
    print("[GPS] port=%s baud=%d" % (C.GPS_PORT, C.GPS_BAUD), flush=True)
    print("[LOG] %s" % log.path, flush=True)

    # IR photodiode alerts share the same holdoff as the camera
    def ir_loop():
        while True:
            time.sleep(0.1)
            if not ir.enabled:
                continue
            r = ir.result()
            now = time.monotonic()
            if r["detected"] and (now - cam.last_alert) * 1000 >= C.ALERT_HOLDOFF_MS:
                cam.last_alert = now
                conf = 1.0 if r["validCount"] >= 8 else r["validCount"] / 8.0
                log.hit("ir", r["freqHz"], r["dutyCycle"], conf, 0, 0, 0.0, r["amp"])
                if not state["muted"]:
                    buz.play_alert()
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
                          recorder=rec, logger=log, state=state)
    app = create_app(ctx)
    print("[WEB] http://%s:%d/" % (C.AP_IP, C.WEB_PORT), flush=True)
    app.run(host="0.0.0.0", port=C.WEB_PORT, threaded=True, use_reloader=False)


if __name__ == "__main__":
    main()
