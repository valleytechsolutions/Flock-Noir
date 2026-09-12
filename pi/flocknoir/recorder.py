"""Video recording with the Pi's hardware H.264 encoder (picamera2).

Records the main stream while detection keeps running. If ffmpeg is present
we write .mp4 (and can mux audio from a USB microphone); otherwise a raw .h264
file that VLC plays directly.
"""
import os
import shutil
import threading
import time

import config as C

try:
    from picamera2.encoders import H264Encoder
    from picamera2.outputs import FileOutput, FfmpegOutput
    _HAVE = True
except Exception:
    _HAVE = False


class Recorder:
    def __init__(self, camera):
        self._cam = camera
        self._lock = threading.Lock()
        self.active = False
        self.audio = False
        self._start = 0.0
        self.path = ""
        os.makedirs(C.REC_DIR, exist_ok=True)

    def start(self, with_audio=False):
        with self._lock:
            if self.active or not _HAVE or not self._cam.picam2:
                return False
            base = os.path.join(C.REC_DIR, "rec_%d" % int(time.time()))
            enc = H264Encoder(bitrate=C.REC_BITRATE)
            if shutil.which("ffmpeg"):
                self.path = base + ".mp4"
                out = FfmpegOutput(self.path, audio=bool(with_audio))
                self.audio = bool(with_audio)
            else:
                self.path = base + ".h264"
                out = FileOutput(self.path)
                self.audio = False
            try:
                self._cam.picam2.start_encoder(enc, out)
            except Exception as e:
                print("[REC] start failed:", e, flush=True)
                return False
            self._start = time.time()
            self.active = True
            print("[REC] start", self.path, "(+audio)" if self.audio else "", flush=True)
            return True

    def stop(self):
        with self._lock:
            if not self.active:
                return
            try:
                self._cam.picam2.stop_encoder()
            except Exception as e:
                print("[REC] stop error:", e, flush=True)
            self.active = False
            print("[REC] stop", flush=True)

    def seconds(self):
        return int(time.time() - self._start) if self.active else 0

    def frames(self):
        return int(self.seconds() * self._cam.fps) if self.active else 0

    @staticmethod
    def listing():
        out = []
        try:
            for n in sorted(os.listdir(C.REC_DIR)):
                p = os.path.join(C.REC_DIR, n)
                if os.path.isfile(p):
                    out.append({"name": n, "size": os.path.getsize(p)})
        except OSError:
            pass
        return out
