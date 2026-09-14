# Changelog

All notable changes to the Flock Noir. This project is **experimental**;
version numbers are milestones, not stability guarantees.

## v0.3
- **Raspberry Pi hardware target** (`pi/`): a Python port for any Pi with Wi-Fi and a CSI
  camera (reference: Pi Zero 2 W + Camera Module NoIR v2, which has no IR-cut filter and
  runs up to 90 fps). Same web UI and API, same log formats; picamera2 capture with fixed
  exposure, hardware H.264 recording, gpiozero RTTTL buzzer, serial NMEA GPS, `iw`-based
  wardriving, optional MCP3008 IR photodiode path, systemd service, and an nmcli hotspot
  with captive portal. One-shot `pi/install.sh`.
- **Ready-to-flash Raspberry Pi image**: a GitHub Actions workflow builds the official
  Raspberry Pi OS Lite (64-bit) with Flock Noir pre-installed, the hotspot pre-configured,
  SSH on, and SPI/UART enabled, and attaches `flocknoir-pi.img.xz` to each tagged release
  (or run it manually from the Actions tab).
- **Shared web UI**: `web/index.html` is now the single source of truth; the XIAO header
  `web_ui.h` is generated from it by `tools/html2header.py`.
- **Detector noise fix (both targets)**: the loosened "approximate" mode could fire on pure
  sensor noise. Simulation across 37-90 fps showed two metrics that separate a real strobe
  from noise at every frame rate: **duty** (strobe ~0.2, noise ~0.5) and **interval
  jitter** (strobe <= 0.09, noise >= 0.19). The decision now gates on `DUTY_MAX` and a new
  `JITTER_MAX`, with `PERIOD_SCORE_MIN` kept loose because a 20 ms pulse can fall between
  frames at ~40 fps. Verified: 147/147 synthetic cases classified correctly.
- **Live camera view** in the web UI (grayscale NIR) via on-demand JPEG frames, with the
  brand logo as a corner watermark.
- **Recording** to SD: MJPEG **AVI** video, optional onboard-mic **WAV** sidecar (PDM mic
  over I2S). Record controls + recordings list/download in the UI.
- **Wi-Fi wardriver** (WiGLE-format CSV, kept in a separate log from IR detections),
  modeled on Piglet. Scanning auto-pauses while a device is connected to the UI so the
  page stays responsive; wardriving is **off by default**.
- **Captive portal** (DNS + catch-all redirect) so phones - especially iPhone - open the
  UI automatically on connect.
- **microSD fixed to the Sense's SPI wiring** (`SD.begin(21)`) instead of SD_MMC.
- Boot jingle set to the **Super Mario Bros.** theme; separate configurable alert tone.
- Branded, dark, four-tab web UI (Detector / Camera / Wardrive / Settings) with the
  embedded, unaltered logo.
- **Approximate ("high sensitivity") detection:** loosened thresholds and raised fixed
  exposure/gain so a weak IR signal (for example through a stock IR-cut lens) still
  triggers. False positives are expected; the confidence and duty are logged so you can
  filter and verify. A light regularity gate keeps random noise from beeping non-stop.
- **GPS wiring profiles** in `config.h`: select the Seeed L76K "GNSS for XIAO" sandwich
  board or a discrete external module (for example an ATGM336H, or an LC29H at 115200).
- Default "Power Rangers" alert tone is now a Mighty Morphin communicator RTTTL; built-in
  tone defaults re-seed automatically when their version changes.

## v0.2
- Branded web UI, `@valleytechsolutions` identity.
- **Passive-buzzer RTTTL** engine with an NVS-persisted tone library + Settings tab.
- Embedded logo pipeline (`logo2header.py`).

## v0.1
- Initial time-domain **IR camera-flash detector** (period + duty + compactness scoring).
- GPS tagging (NMEA/UART), CSV logging to SD, SoftAP web alert UI.

## Unreleased / planned
- 850 nm photodiode front-end for exact-timing confirmation + sensor fusion.
- Rolling-shutter band analysis.
- GPX/KML export; BLE/OUI detection.
