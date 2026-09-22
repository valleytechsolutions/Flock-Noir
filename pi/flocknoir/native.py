"""ctypes interface to the same bounded pulse/packet parsers used by XIAO."""
import ctypes as ct
import json
import os
from pathlib import Path

_library = None


def library():
    global _library
    if _library is None:
        name = "native_core.dll" if os.name == "nt" else "native_core.so"
        lib = ct.CDLL(os.environ.get("FLOCKNOIR_NATIVE", str(Path(__file__).with_name(name))))
        for name, args, result in (
            ("fn_decode_wifi", [ct.c_char_p, ct.c_size_t, ct.c_void_p, ct.c_size_t], ct.c_int),
            ("fn_decode_ble", [ct.c_char_p, ct.c_size_t, ct.c_char_p, ct.c_int, ct.c_void_p, ct.c_size_t], ct.c_int),
            ("fn_pulse_new", [], ct.c_void_p),
            ("fn_decode_survey", [ct.c_char_p, ct.c_char_p, ct.c_void_p, ct.c_size_t], ct.c_int),
            ("fn_pulse_free", [ct.c_void_p], None),
            ("fn_pulse_reset", [ct.c_void_p], None),
            ("fn_pulse_feed", [ct.c_void_p, ct.c_uint16, ct.c_uint32], None),
            ("fn_pulse_configure", [ct.c_void_p, ct.POINTER(ct.c_float), ct.POINTER(ct.c_uint32)], None),
            ("fn_pulse_read", [ct.c_void_p, ct.POINTER(ct.c_float), ct.POINTER(ct.c_uint32)], None),
        ):
            fn = getattr(lib, name)
            fn.argtypes, fn.restype = args, result
        _library = lib
    return _library


def decode(data, address=None, public_address=False):
    data = bytes(data)
    if len(data) > 4096 or (address is not None and len(address) != 6):
        return {"valid": False}
    out = ct.create_string_buffer(8192)
    lib = library()
    if address is None:
        size = lib.fn_decode_wifi(data, len(data), out, len(out))
    else:
        size = lib.fn_decode_ble(data, len(data), bytes(address), public_address, out, len(out))
    return json.loads(out.value) if size >= 0 else {"valid": False}


def decode_survey(address, ssid):
    if len(address) != 6:
        return {"valid": False}
    out = ct.create_string_buffer(8192)
    size = library().fn_decode_survey(bytes(address), ssid.encode("ascii", "replace")[:128], out, len(out))
    return json.loads(out.value) if size >= 0 else {"valid": False}


class Pulse:
    def __init__(self, floats=None, integers=None):
        self.lib = library()
        self.handle = self.lib.fn_pulse_new()
        if not self.handle:
            raise MemoryError("Pulse detector allocation failed")
        self.floats = (ct.c_float * 6)()
        self.integers = (ct.c_uint32 * 4)()
        if floats is not None and integers is not None:
            self.lib.fn_pulse_configure(self.handle, (ct.c_float * 9)(*floats),
                                        (ct.c_uint32 * 3)(*integers))

    def reset(self):
        self.lib.fn_pulse_reset(self.handle)

    def feed(self, raw, micros):
        self.lib.fn_pulse_feed(self.handle, max(0, min(4095, int(raw))), int(micros) & 0xffffffff)

    def result(self):
        self.lib.fn_pulse_read(self.handle, self.floats, self.integers)
        baseline, noise, amp, freq, duty, width = self.floats
        valid, gaps, matched, clipped = self.integers
        return dict(baseline=int(baseline), noise=noise, amp=int(amp), freqHz=freq,
                    dutyCycle=duty, pulseMs=width, validCount=valid, gaps=gaps,
                    detected=bool(matched), clipped=bool(clipped))

    def close(self):
        if self.handle:
            self.lib.fn_pulse_free(self.handle)
            self.handle = None

    def __del__(self):
        if getattr(self, "handle", None):
            self.close()
