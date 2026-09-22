"""Flask web app - serves the shared UI and the same /api endpoints as the XIAO
firmware, so web/index.html works unchanged on both hardware targets."""
import os
import shutil
import time

from flask import (Flask, Response, jsonify, redirect, request, send_file,
                   send_from_directory, abort)

import config as C


def create_app(ctx):
    app = Flask(__name__, static_folder=None)
    cam, gps, buz, wd, ir, rec, log, state = (ctx.camera, ctx.gps, ctx.buzzer, ctx.wardriver,
                                              ctx.irsensor, ctx.recorder, ctx.logger, ctx.state)

    @app.get("/")
    def root():
        return send_from_directory(C.WEB_DIR, "index.html")

    @app.get("/api/radio")
    def radio_capabilities():
        return jsonify(supported=False)

    @app.get("/logo.png")
    def logo():
        return send_from_directory(C.WEB_DIR, "logo.png")

    @app.get("/api/status")
    def status():
        d = cam.detector.last()
        fix = gps.fix()
        gh = gps.health()
        irr = ir.result()
        try:
            du = shutil.disk_usage(C.DATA_DIR)
            sd_total, sd_free = du.total >> 20, du.free >> 20
        except OSError:
            sd_total = sd_free = 0
        j = {
            "detected": d.detected, "freq": round(d.freqHz, 2), "duty": round(d.dutyCycle, 3),
            "confidence": round(d.confidence, 3), "fps": round(cam.fps, 1),
            "sd": True, "logged": log.count,
            "fix": bool(fix.get("valid")), "buzzer": buz.enabled,
            "wd": wd.enabled, "wdScan": wd.scanning, "wdLogged": wd.logged,
            "wdTotal": wd.last_total, "wdNew": wd.new_last,
            "rec": rec.active, "recAudio": rec.audio, "recSecs": rec.seconds(),
            "recFrames": rec.frames(),
            "sats": fix.get("sats", 0), "time": gps.iso_utc(),
            "gpsChars": gh["chars"], "gpsGood": gh["good"], "gpsFail": gh["fail"],
            "hdop": round(fix.get("hdop", 0.0), 1),
            "amp": d.levelPP, "wave": cam.detector.snapshot(64),
            "irEn": ir.enabled, "irDet": irr["detected"], "irPresent": irr["present"],
            "irFreq": round(irr["freqHz"], 1), "irDuty": round(irr["dutyCycle"], 3),
            "irAmp": irr["amp"], "irWave": ir.snapshot(64),
            "muted": state["muted"], "uptime": int(time.time() - state["start"]),
            "sdFree": sd_free, "sdTotal": sd_total,
            "recent": log.recent_list(),
        }
        if fix.get("valid"):
            j["lat"] = round(fix["lat"], 6)
            j["lon"] = round(fix["lon"], 6)
        return jsonify(j)

    @app.get("/api/frame.jpg")
    def frame():
        data = cam.jpeg()
        if not data:
            abort(503)
        return Response(data, mimetype="image/jpeg",
                        headers={"Cache-Control": "no-store"})

    @app.get("/stream.mjpg")
    def stream():
        def gen():
            while True:
                data = cam.jpeg()
                if data:
                    yield (b"--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
                           str(len(data)).encode() + b"\r\n\r\n" + data + b"\r\n")
                time.sleep(0.1)
        return Response(gen(), mimetype="multipart/x-mixed-replace; boundary=frame")

    @app.get("/api/log")
    def get_log():
        return send_file(log.path, as_attachment=True, download_name="flock_ir_log.csv")

    @app.get("/api/wardrive.csv")
    def get_wardrive():
        return send_file(wd.path, as_attachment=True, download_name="wigle_wardrive.csv")

    @app.post("/api/wardrive")
    def set_wardrive():
        wd.set_enabled(request.form.get("en") == "1")
        return jsonify(ok=True)

    @app.post("/api/irsensor")
    def set_ir():
        ir.set_enabled(request.form.get("en") == "1")
        return jsonify(ok=True)

    @app.post("/api/mute")
    def set_mute():
        state["muted"] = request.form.get("en") == "1"
        cam.muted = state["muted"]
        return jsonify(ok=True)

    @app.post("/api/rec")
    def rec_ctl():
        a = request.form.get("action")
        if a == "start":
            ok = rec.start(request.form.get("audio") == "1")
            return jsonify(ok=ok, audio=rec.audio), (200 if ok else 500)
        if a == "stop":
            rec.stop()
            return jsonify(ok=True)
        return jsonify(ok=False), 400

    @app.get("/api/recs")
    def recs():
        return jsonify(rec.listing())

    @app.get("/api/rec/get")
    def rec_get():
        f = request.args.get("f", "")
        if not f or "/" in f or ".." in f:
            abort(400)
        return send_from_directory(C.REC_DIR, f, as_attachment=True)

    @app.get("/api/settings")
    def get_settings():
        return jsonify(buz.to_json())

    @app.post("/api/settings")
    def set_settings():
        buz.enabled = request.form.get("enabled") == "1"
        try:
            count = max(0, min(C.BUZZER_MAX_TONES, int(request.form.get("count", "0"))))
        except ValueError:
            count = 0
        buz.tones = [(request.form.get("nm%d" % i, ""), request.form.get("rt%d" % i, ""))
                     for i in range(count)]
        try:
            buz.alert_idx = int(request.form.get("alertIdx", "0"))
        except ValueError:
            buz.alert_idx = 0
        if buz.alert_idx >= len(buz.tones):
            buz.alert_idx = 0
        buz.save()
        return jsonify(ok=True)

    @app.post("/api/test")
    def test_tone():
        r = request.form.get("rtttl", "")
        if r:
            buz.play(r)
        elif request.form.get("idx") is not None:
            try:
                i = int(request.form.get("idx"))
                if 0 <= i < len(buz.tones):
                    buz.play(buz.tones[i][1])
            except ValueError:
                pass
        return jsonify(ok=True)

    @app.errorhandler(404)
    def captive(_e):
        # captive-portal catch-all (OS "is there internet?" probes) -> the UI
        return redirect("http://%s/" % C.AP_IP, code=302)

    return app
