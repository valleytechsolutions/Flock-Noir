"""Tiny JSON settings store (one file, keyed sections) shared by the modules."""
import json
import os
import threading

import config as C

_lock = threading.Lock()


def load_section(name, default=None):
    try:
        with open(C.SETTINGS_FILE, encoding="utf-8") as f:
            return json.load(f).get(name, default)
    except (OSError, ValueError):
        return default


def save_section(name, value):
    with _lock:
        os.makedirs(C.DATA_DIR, exist_ok=True)
        data = {}
        try:
            with open(C.SETTINGS_FILE, encoding="utf-8") as f:
                data = json.load(f)
        except (OSError, ValueError):
            data = {}
        data[name] = value
        with open(C.SETTINGS_FILE, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=1)
