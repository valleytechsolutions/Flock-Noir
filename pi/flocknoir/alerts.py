"""Device sound assignments. IDs mirror the XIAO alert_tones.h contract."""
import threading

PRESETS = [
    ("retro", "Retro blaster", "Blaster:d=16,o=6,b=180:c,g,c7,p,a#,g,8c7"),
    ("siren", "Two-tone siren", "Siren:d=8,o=6,b=150:c,g,c,g,c,g"),
    ("confused", "Confused", "Hmm:d=8,o=5,b=170:g,c6,f#,4d#"),
    ("ring", "Question chime", "Door:d=8,o=6,b=170:e,c,16p,f#,4d#"),
    ("flipper", "Digital warble", "Warble:d=16,o=6,b=170:c,f#,c7,f#,c,g"),
    ("pineapple", "Network warning", "Network:d=16,o=5,b=160:g,g,p,c6,c6,p,f#6"),
    ("drone", "Radar", "Radar:d=16,o=6,b=160:c,p,g,p,c7,p,g"),
    ("chirp", "Triple chirp", "Chirp:d=32,o=7,b=200:c,p,c,p,c"),
    ("silent", "Silent", ""),
]
KINDS = [
    ("alpr_combined", "ALPR radio + optical", "retro"),
    ("alpr_ir", "ALPR pulse / OPT101", "retro"),
    ("alpr_ble", "ALPR / Flock BLE", "retro"),
    ("alpr_wifi", "ALPR / Flock Wi-Fi", "retro"),
    ("camera", "Camera pulse candidate", "retro"),
    ("axon", "Axon candidate", "siren"),
    ("ring", "Ring candidate", "ring"),
    ("meta", "Meta glasses", "confused"),
    ("flipper", "Flipper Zero", "flipper"),
    ("pineapple", "Wi-Fi Pineapple", "pineapple"),
    ("drone", "Drone", "drone"),
    ("other", "Other watchlist devices", "chirp"),
]
DEFAULTS = {key: default for key, _, default in KINDS}


def valid(sound, count):
    return isinstance(sound, str) and (any(sound == p[0] for p in PRESETS) or
        (len(sound) == 6 and sound.startswith("slot:") and sound[-1].isdigit() and int(sound[-1]) < count))


def classify(row, protocol, ir=False, camera=False):
    if row["alpr"]:
        return "alpr_combined" if row["tier"] >= 2 and (ir or camera) else "alpr_"+protocol
    category = row["category"].lower()
    for key in ("axon", "ring", "meta", "flipper", "pineapple"):
        if key in category:
            return key
    return "drone" if any(k in category for k in ("drone", "dji", "parrot", "skydio")) else "other"


def detection_method(ir=False, camera=False, protocol=None):
    return "+".join(k for k, present in (("ir", ir), ("camera", camera),
                     ("ble", protocol == "ble"), ("wifi", protocol == "wifi")) if present) or "unknown"


class AlertQueue:
    def __init__(self):
        self.pending, self.last = {}, {}
        self.lock = threading.Lock()

    def request(self, kind, now):
        with self.lock:
            if kind in DEFAULTS and now-self.last.get(kind, -1e30) >= 4:
                self.pending.setdefault(kind, now)

    def clear(self):
        with self.lock:
            self.pending.clear()

    def take(self, now, muted, enabled, busy):
        with self.lock:
            if muted or not enabled:
                self.pending.clear()
            for kind in DEFAULTS:
                if kind not in self.pending:
                    continue
                if now-self.pending[kind] > 30:
                    del self.pending[kind]
                    continue
                if busy:
                    continue
                del self.pending[kind]
                self.last[kind] = now
                return kind
        return None
