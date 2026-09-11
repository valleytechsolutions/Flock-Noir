# Changelog

All notable changes to the Flock Noir. This project is **experimental**;
version numbers are milestones, not stability guarantees.

## v0.3
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
