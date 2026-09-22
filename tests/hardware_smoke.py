"""Read-only post-flash checks on a connected XIAO. Requires pyserial (via PlatformIO)."""
import argparse
import json
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--seconds", type=int, default=30)
    parser.add_argument("--version", default="0.4.1")
    parser.add_argument("--require-gps-data", action="store_true")
    parser.add_argument("--require-sd", action="store_true")
    args = parser.parse_args()
    if args.seconds < 15:
        parser.error("--seconds must be at least 15")
    health, radio, versions, faults = [], [], [], set()
    with serial.Serial(args.port, 115200, timeout=0.2, write_timeout=2) as port:
        port.reset_input_buffer()
        started = time.monotonic()
        next_command, command_index, pending = 0.5, 0, b""
        commands = (b"CMD:VERSION\n", b"CMD:HEALTH\n", b"CMD:STATUS\n")
        while time.monotonic() - started < args.seconds:
            elapsed = time.monotonic() - started
            if elapsed >= next_command:
                port.write(commands[command_index % len(commands)])
                command_index += 1
                next_command = elapsed + 2
            pending += port.read(max(1, port.in_waiting))
            while b"\n" in pending:
                line, pending = pending.split(b"\n", 1)
                line = line.decode("utf-8", errors="replace").strip()
                for marker in ("Guru Meditation", "Backtrace:", "Rebooting",
                               "FB-SIZE", "FB-OVF", "EV-VSYNC-OVF",
                               "Failed to get the frame", "ADC initialization failed"):
                    if marker in line:
                        faults.add(marker)
                try:
                    item = json.loads(line)
                except ValueError:
                    continue
                if not isinstance(item, dict):
                    continue
                if "firmware" in item:
                    versions.append(item)
                elif "irSampleHz" in item:
                    health.append(item)
                elif "bleReady" in item:
                    radio.append(item)
            if len(pending) > 65536:
                raise RuntimeError("Unterminated serial output exceeded 64 KiB")
    errors = list(faults)
    if not versions or any(v.get("version") != args.version for v in versions):
        errors.append("Expected firmware version was not confirmed")
    if len(health) < 2 or len(radio) < 2:
        errors.append("Too few health/radio responses")
    else:
        if health[-1]["uptime"] - health[0]["uptime"] < 5:
            errors.append("Uptime did not advance normally")
        for status in health:
            if not status.get("cameraReady") or status["fps"] <= 0:
                errors.append("Camera is not delivering frames")
            if not status["irPresent"] or not 800 <= status["irSampleHz"] <= 1200:
                errors.append("ADC sampler is not running near 1 kHz")
            if args.require_sd and not status["sd"]:
                errors.append("SD card unavailable")
        if args.require_gps_data and health[-1]["gpsGood"] <= health[0]["gpsGood"]:
            errors.append("No new valid GPS sentences")
        for status in radio:
            if not all(status[k] for k in ("wifiReady", "bleReady", "bleScanning")):
                errors.append("WiFi/BLE scanning is not ready (enable BLE before this check)")
        if radio[-1]["logErrors"] != radio[0]["logErrors"]:
            errors.append("Radio log errors increased")
    # Summaries omit coordinates, nearby device identities and capture contents.
    summary = {"version": versions[-1].get("version") if versions else None,
               "healthResponses": len(health), "radioResponses": len(radio),
               "errors": sorted(set(errors))}
    if health:
        keys = ("uptime", "fps", "cameraReady", "sd", "gpsChars", "gpsGood", "gpsFail",
                "fix", "sats", "irEn", "irPresent", "irSampleHz", "irRaw", "irGaps")
        summary["health"] = {key: health[-1].get(key) for key in keys}
        summary["fpsRange"] = [min(h["fps"] for h in health), max(h["fps"] for h in health)]
    if radio:
        keys = ("mode", "wifiReady", "bleReady", "bleScanning", "packets", "dropped",
                "freeHeap", "minFreeHeap", "logErrors")
        summary["radio"] = {key: radio[-1].get(key) for key in keys}
    print(json.dumps(summary, indent=2))
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
