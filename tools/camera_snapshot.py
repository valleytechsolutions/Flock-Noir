"""Fetch one bounded diagnostic JPEG over USB without changing camera settings."""
import argparse
import json
from pathlib import Path
import time
import serial

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--port", required=True)
p.add_argument("--output", required=True)
args = p.parse_args()
with serial.Serial(args.port, 115200, timeout=.25, write_timeout=2) as port:
    port.reset_input_buffer()
    port.write(b"CMD:FRAME\n")
    data, wanted = bytearray(), None
    deadline = time.monotonic()+40
    pending=b""
    saved=False
    while time.monotonic()<deadline:
        while b"\n" not in pending and time.monotonic()<deadline:
            pending+=port.read(max(1,port.in_waiting))
            if len(pending)>65536:
                raise RuntimeError("Unterminated USB response exceeded 64 KiB")
        if b"\n" not in pending:
            break
        line,pending=pending.split(b"\n",1)
        try:
            row=json.loads(line)
        except (ValueError,UnicodeDecodeError):
            continue
        if "error" in row:
            time.sleep(.5)
            port.write(b"CMD:FRAME\n")
        if "frame_bytes" in row:
            wanted=row["frame_bytes"]
            data.clear()
            if not 0<wanted<=131072:
                raise RuntimeError("Invalid snapshot size")
        if "frame_hex" in row:
            if row["frame_offset"]!=len(data):
                raise RuntimeError(f"Missing snapshot chunk: wanted {len(data)}, got {row['frame_offset']}")
            data.extend(bytes.fromhex(row["frame_hex"]))
            if wanted and len(data)==wanted:
                if data[:2]!=b"\xff\xd8" or data[-2:]!=b"\xff\xd9":
                    raise RuntimeError("Invalid JPEG envelope")
                Path(args.output).write_bytes(data)
                print("Saved",len(data),"JPEG bytes to",args.output)
                saved=True
                break
    if not saved:
        raise RuntimeError("Snapshot timed out; no file written")
