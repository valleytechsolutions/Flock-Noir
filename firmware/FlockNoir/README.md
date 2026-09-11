# Firmware - FlockNoir (Arduino sketch)

This folder is the Arduino sketch for the **Flock Noir**. For the project
overview, the **experimental disclaimer**, the parts list, and wiring, see the repository
root: **[../../README.md](../../README.md)** and **[../../HARDWARE.md](../../HARDWARE.md)**.

>  **Experimental.** This detects an IR flash *pattern consistent with* some ALPR cameras -
> it is **not** a definitive detector. **Always visually confirm** an actual camera.

## Build

- **esp32** core **v3.x** + **TinyGPSPlus** (everything else ships with the core).
- Board **XIAO_ESP32S3**, **PSRAM: OPI**, **USB CDC On Boot: Enabled**, 8 MB partition.

```bash
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3:PSRAM=opi,PartitionScheme=default_8MB .
# add --upload -p <PORT> to flash
```

## Files

| File | Role |
|------|------|
| `FlockNoir.ino` | Main loop: camera -> detector -> GPS/CSV -> buzzer -> wardriver -> recorder -> web |
| `config.h` | **All pins & tunables** (Wi-Fi, GPS, buzzer, SD, detection thresholds, wardrive, recorder) |
| `detector.h/.cpp` | Time-domain IR pattern detector (period + duty + compactness -> confidence) |
| `buzzer.h/.cpp` | Non-blocking RTTTL player + NVS-persisted tone library |
| `wardriver.h/.cpp` | Async Wi-Fi scan -> WiGLE CSV (separate log); NVS on/off |
| `recorder.h/.cpp` | MJPEG-AVI writer (+ optional PDM-mic WAV sidecar) |
| `web_ui.h` | The web app (HTML/CSS/JS), served from flash |
| `logo.h` | Embedded logo (generated - do not hand-edit) |
| `assets/logo.png` | Source logo |
| `tools/logo2header.py` | `assets/logo.png` -> `logo.h` converter |

## Custom logo

Replace `assets/logo.png`, then:

```bash
python tools/logo2header.py
```

Rebuild. The image is embedded unaltered and served at `/logo.png` (an SD-card `/logo.png`
overrides it, no recompile needed).

## Tuning (in `config.h`)

| Symptom | Try |
|---|---|
| Nothing detects / view too bright | Lower `CAM_AEC_VALUE`; keep `CAM_AGC_GAIN` = 0 |
| Blob never saturates | Lower `SAT_THRESHOLD`, or use an IR-filter-removed lens |
| Too many false positives | Raise `DETECT_CONFIDENCE` / `MIN_GOOD_CYCLES`; tighten `PERIOD_TOL_MS`, `DUTY_MIN/MAX` |
| Misses real cameras | Loosen `PERIOD_TOL_MS`; widen `DUTY_MIN/MAX`; lower `MIN_AMPLITUDE` |
| Using the Seeed L76K GPS | Set `GPS_BAUD 9600` (LC29H uses 115200) |
| Camera glitches when buzzer plays | Change `BUZZER_LEDC_CHANNEL` |
