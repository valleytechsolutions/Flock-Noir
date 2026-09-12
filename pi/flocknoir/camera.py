"""Camera capture loop (picamera2) feeding the IR detector.

Two streams from one request: a small 'lores' luma plane for detection (cheap
numpy math at high fps) and the 'main' stream for the live view and recordings.
Auto-exposure, gain and white balance are forced OFF so a pulsing IR source is
not corrected away. The NoIR camera has no IR-cut filter, so 850 nm is visible.
"""
import io
import threading
import time

import numpy as np

import config as C
from .detector import Detector

try:
    from picamera2 import Picamera2
    _HAVE_CAM = True
except Exception:
    _HAVE_CAM = False

try:
    from PIL import Image
    _HAVE_PIL = True
except Exception:
    _HAVE_PIL = False

LORES = (320, 240)


class Camera:
    def __init__(self, on_detect):
        self._on_detect = on_detect          # callback(DetectionResult, cx, cy)
        self.picam2 = None
        self.detector = Detector(LORES[0] * LORES[1])
        self.fps = 0.0
        self.ok = False
        self._lock = threading.Lock()
        self._last_main = None               # latest main-stream luma (numpy)
        self._last_jpeg = b""
        self._jpeg_t = 0.0
        self.last_alert = 0.0
        self.muted = False
        self.last_cxy = (0, 0)
        if _HAVE_CAM:
            try:
                self._setup()
                self.ok = True
            except Exception as e:
                print("[CAM] init failed:", e, flush=True)
        if self.ok:
            threading.Thread(target=self._run, daemon=True).start()

    def _setup(self):
        self.picam2 = Picamera2()
        cfg = self.picam2.create_video_configuration(
            main={"size": C.CAM_SIZE, "format": "YUV420"},
            lores={"size": LORES, "format": "YUV420"},
            controls={"FrameRate": C.CAM_FPS,
                      "AeEnable": False, "AwbEnable": False,
                      "ExposureTime": C.CAM_EXPOSURE_US,
                      "AnalogueGain": C.CAM_GAIN})
        self.picam2.configure(cfg)
        self.picam2.start()
        time.sleep(0.3)

    def _run(self):
        h_lo = LORES[1]
        h_main = C.CAM_SIZE[1]
        xs = np.arange(LORES[0], dtype=np.float32)
        ys = np.arange(h_lo, dtype=np.float32)
        frames = 0
        t_win = time.monotonic()
        last_analyze = 0.0
        n = 0
        while True:
            try:
                req = self.picam2.capture_request()
            except Exception:
                time.sleep(0.05)
                continue
            try:
                lo = req.make_array("lores")
                y = lo[:h_lo, :]                       # luma plane
                level = int(y.max())
                mask = y >= C.SAT_THRESHOLD
                cnt = int(mask.sum())
                if cnt:
                    cx = int((mask.sum(axis=0) * xs).sum() / cnt)
                    cy = int((mask.sum(axis=1) * ys).sum() / cnt)
                    self.last_cxy = (cx, cy)
                self.detector.feed(level, cnt)
                n += 1
                if n % 3 == 0:                          # cache a main frame for the UI
                    main = req.make_array("main")
                    with self._lock:
                        self._last_main = main[:h_main, :].copy()
            finally:
                req.release()

            frames += 1
            now = time.monotonic()
            if now - t_win >= 1.0:
                self.fps = frames / (now - t_win)
                frames = 0
                t_win = now

            if (now - last_analyze) * 1000 >= C.ANALYZE_EVERY_MS:
                last_analyze = now
                d = self.detector.analyze()
                if d.detected and (now - self.last_alert) * 1000 >= C.ALERT_HOLDOFF_MS:
                    self.last_alert = now
                    self._on_detect(d, *self.last_cxy)

    # ---- live view ------------------------------------------------------------
    def jpeg(self):
        """Latest main-stream frame as a grayscale JPEG (cached ~10 fps)."""
        now = time.monotonic()
        if self._last_jpeg and now - self._jpeg_t < 0.08:
            return self._last_jpeg
        with self._lock:
            y = self._last_main
        if y is None or not _HAVE_PIL:
            return self._last_jpeg
        buf = io.BytesIO()
        Image.fromarray(y, "L").save(buf, "JPEG", quality=C.CAM_JPEG_QUALITY)
        self._last_jpeg = buf.getvalue()
        self._jpeg_t = now
        return self._last_jpeg
