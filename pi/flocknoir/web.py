"""Flask web app - serves the shared UI and the same /api endpoints as the XIAO
firmware, so web/index.html works unchanged on both hardware targets."""
import os
import shutil
import time
from pathlib import Path
import re

from flask import (Flask, Response, jsonify, redirect, request, send_file,
                   send_from_directory, abort)

import config as C
from . import __version__


def create_app(ctx):
    app = Flask(__name__, static_folder=None)
    cam, gps, buz, wd, ir, rec, log, state = (ctx.camera, ctx.gps, ctx.buzzer, ctx.wardriver,
                                              ctx.irsensor, ctx.recorder, ctx.logger, ctx.state)

    @app.get("/")
    def root():
        return send_from_directory(C.WEB_DIR, "index.html")

    @app.get("/api/radio")
    def radio_capabilities():
        return jsonify(ctx.radio.status())

    @app.post("/api/radio")
    def radio_settings():
        try:
            ctx.radio.configure(request.form.get("mode", ""), request.form.get("ble") == "1",
                                request.form.get("capture") == "1", request.form.get("watch", ""),
                                request.form.get("target", ""), request.form.get("channel", "0"))
            return jsonify(ok=True)
        except (ValueError, OSError) as exc:
            return jsonify(ok=False, error=str(exc)), 400

    @app.get("/api/radio/devices")
    def radio_devices():
        return jsonify(ctx.radio.rows())

    @app.post("/api/radio/clear")
    def radio_clear():
        ctx.radio.clear_live()
        return jsonify(ok=True)

    def radio_download(name):
        if not re.fullmatch(r"(?:events_|wifi_|ble_)[A-Za-z0-9_-]+\.(?:jsonl|pcap)", name):
            abort(400)
        path = Path(C.RADIO_DIR)/name
        if path.resolve().parent != Path(C.RADIO_DIR).resolve() or not path.is_file():
            abort(404)
        return send_file(path, as_attachment=True, download_name=name)

    @app.get("/api/radio/file")
    def radio_file():
        return radio_download(request.args.get("name", ""))

    @app.get("/api/radio/log")
    def radio_log():
        return radio_download(ctx.radio.paths["log"].name)

    @app.get("/api/radio/pcap")
    def radio_pcap():
        return radio_download(ctx.radio.paths["pcap"].name)

    @app.get("/api/radio/ble")
    def radio_ble():
        return radio_download(ctx.radio.paths["ble"].name)

    @app.get("/api/radio/files")
    def radio_files():
        directory = Path(C.RADIO_DIR)
        files = []
        try:
            for path in sorted(directory.iterdir(), key=lambda p: p.name, reverse=True):
                if path.is_file() and path.suffix in (".pcap", ".jsonl") and not path.is_symlink():
                    files.append(dict(name=path.name, size=path.stat().st_size))
                if len(files) >= 100:
                    break
        except OSError:
            pass
        return jsonify(files)

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
            "sd": log.ready, "logged": log.count, "logErrors": log.errors,
            "version": __version__, "cameraReady": cam.ok,
            "radioAlert": ctx.radio.alert(), "radioEvents": ctx.radio.events,
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
            "irRaw": irr["raw"], "irBaseline": irr["baseline"], "irClipped": irr["clipped"],
            "irPulseMs": irr["pulseMs"], "irSampleHz": irr["sampleHz"], "irGaps": irr["gaps"],
            "irNoise": irr["noise"], "irDroppedEvents": irr["droppedEvents"],
            "radioNearby": ctx.radio.recent_alpr(), "irError": ir.error,
            "muted": state["muted"], "uptime": int(time.monotonic() - state["start"]),
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
