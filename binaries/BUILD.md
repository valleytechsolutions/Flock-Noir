# Flock Noir 0.6.0 — XIAO ESP32-S3 Sense

[Install with the web flasher](https://valleytechsolutions.github.io/Flock-Noir/)
using desktop Chrome / Edge and a USB data cable. Leave **Erase device unchecked**
when updating to preserve settings. The installer writes split parts excluding
NVS. The downloadable merged image writes at **0x0**, includes NVS padding and
can reset settings. The adjacent checksum file identifies the exact image.

## Changes

- **Four exclusive modes.** Choosing ALPR, Scanner, Pig Detector or Wardrive starts
  that scanner and stops the previous one. Camera / Settings preserve the mode.
  Switching clears pending alerts, live rows, optical history and fusion evidence;
  generation-stamped queued observations cannot leak across mode changes.
- **Pig Detector:** Axon-only BLE, no optical analysis / WiFi detection / wardrive.
  Requests 90/100 ms BLE receive window with dashboard, 100/100 ms with field
  coverage and WiFi off. Siren repeats every 10 seconds while evidence remains
  fresh, configurable 5–60 seconds. No reminders after 3 seconds without a match.
  Axon vendor evidence is a candidate, not confirmed body-camera identity.
- **ALPR:** OPT101 + OV2640 + dedicated Flock-You OUI/probe/BLE rules. Early packet
  filtering reduces irrelevant queue traffic. Existing 34-prefix union retained.
  Camera accounts for frame timing and explicitly flags a 5 Hz observation that
  could be a 10 Hz narrow-pulse alias. The measured frequency stays unchanged and
  alias confidence is reduced. Whole-frame flicker / timing gates are tighter.
- **Separate logs:** 31-column ALPR CSV in `/alpr/alpr_*.csv`, with source methods,
  pulse width / sampling rates, camera timing ambiguity and scan mode. Non-ALPR
  devices stay in radio JSONL, now tagged with `scan_mode`.
- **Wardrive only:** WiFi + BLE WiGLE rows, no device detection alerts or optics.
  Fresh GPS/time/HDOP/altitude required; estimated accuracy is HDOP × 5 m, not a
  fixed invented accuracy. Per-address repeat measurements every 15 seconds,
  bounded security-IE parsing, CSV escaping, write-error / missing-fix counters.
- Original dark green design, VGA JPEG quality, ATGM336H D7/D6 at 9600 baud,
  passive buzzer D0 and saved tone/sensor settings preserved. Updated BOM,
  wiring, installation guide, flasher and Colonel Panic / OUI Spy credits.

## Hardware and operation

**Seeed XIAO ESP32-S3 Sense + IR-cut-free OV2640**, 8 MB flash / OPI PSRAM.
OPT101: **OUT→D1/GPIO2, VCC→3V3, GND→GND**. Enable it in ALPR after wiring and
verify its response to light. First-install enable default is off; updates retain
it. GPS remains ATGM336H at 9600 baud. Passive piezo: D0/GPIO1 through ~100 Ω.
ESP32-CAM is not supported by this image. **No Raspberry Pi update is included.**

Join **Flock Noir** / **flocknoir**, open **http://192.168.4.1**, choose a scan tab.
Field scan turns the hotspot off. **Hold BOOT 1.5 seconds** to restore it without
changing the scan mode. Boot restores the hotspot with the saved mode.

## Validation — September 24, 2026

- PlatformIO build passed without compiler warnings/errors: pioarduino 55.03.39,
  Arduino ESP32 3.3.9, TinyGPSPlus 1.0.3; no additional runtime dependencies.
- Portable C++ tests passed for mode filtering, reminders / stale evidence,
  rollover, Axon identifiers and public/random address handling, Flock rules,
  fusion, malformed packets, pulse width/duty/gaps, phase-swept 25 fps aliases,
  WiGLE rows/security/deduplication and JPEG decoding.
- Desktop/mobile browser checks passed for actual mode requests, failed switch
  behavior, inactive sensor indicators, Pig Detector banner/reminder, coverage,
  settings/tone persistence and VGA preview recovery. Screenshot uses simulated
  evidence, not a field observation.
- Staging verifies checksums, board/version and flash ranges excluding NVS.
  Browser smoke checks load the real pinned ESP Web Tools 10.4.0 module and test
  unsupported browsers / failed metadata. Browser USB flashing was not exercised.
- Connected XIAO flashed via PlatformIO, esptool verified each written image.
  All four modes were exercised over USB; optics stopped in General, Pig Detector
  and Wardrive, then resumed in ALPR at approximately 25 fps and 1000 ADC samples/s.
  GPS messages continued, SD stayed ready and tone/sensor settings were preserved.
  Axon field coverage disabled WiFi and reported the 100/100 ms BLE window.
- A 30-second ALPR hardware check showed 640×480, 25.0–25.1 analysis fps, zero
  camera decode errors, 1000 Hz ADC, valid GPS messages and no new queue
  drops during the steady check. The latest restart showed one GPS checksum
  error; valid NMEA continued. Startup / mode-transition queue drops can occur.

The attached **OPT101 ADC input read 4095 (clipped)**. Sampling was verified;
usable optical pulses / physical sensor wiring were not. Clipped inputs are
rejected. GPS was receiving valid NMEA but had no indoor satellite fix, so no
real WiGLE field upload or geotagged drive was validated. Real Axon / ALPR targets,
field range and false-positive rates still require physical testing.

The **8–12 Hz, 10–30% duty, 8–35 ms** optical profile is experimental, not a
manufacturer-confirmed universal ALPR rate. OPT101 measures intensity/timing,
not wavelength. Nearby optical and radio observations can originate separately.

## Reproduce

```sh
python tools/html2header.py
python -m platformio run -e xiao_esp32s3_sense
python tools/package_firmware.py
python tools/build_flasher.py
```

Opt-in connected-board checks (leave ALPR / dashboard active):

```sh
python tests/mode_hardware_smoke.py --port COM44
python tests/hardware_smoke.py --port COM44 --seconds 30 --require-gps-data --require-sd
```

Thanks to [Colonel Panic](https://colonelpanic.tech/),
[OUI Spy Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue),
and the contributors credited in [ATTRIBUTIONS.md](../ATTRIBUTIONS.md).
