# Flock Noir 0.4 - XIAO ESP32-S3 Sense

Factory image: `FlockNoir-merged-0x0.bin`, flash at **0x0**.
SHA-256: see `FlockNoir-merged-0x0.bin.sha256`.

Built with PlatformIO Core 6.1.19, pioarduino platform 55.03.39,
Arduino ESP32 3.3.9 and TinyGPSPlus 1.0.3. Hardware configuration:
8 MB flash, OPI PSRAM, ATGM336H on D7/D6 at 9600 baud, OPT101 OUT on
D1/GPIO2, OV2640 without IR-cut filter, Sense SD on CS GPIO21.

Validation on September 22, 2026:

- Target firmware compiles successfully without compiler warnings/errors.
- Host C++ tests cover camera compactness/staleness, pulse validation,
  sampling gaps, timer rollover, BLE signature rules, malformed packets,
  Remote ID message packs, NAN transport and coordinate decoding.
- Browser tests cover desktop/mobile layout, escaped device names,
  settings submission, IR/radio banner and Pi capability fallback.

This is software validation. Optical range, field accuracy, radio coexistence,
SD write throughput and physical wiring have not been bench-tested in this
session. Follow the hardware validation checklist before depending on results.

To reproduce and package:

```bash
python tools/html2header.py
python -m platformio run -e xiao_esp32s3_sense
python tools/package_firmware.py
```

The factory image includes bootloader, partition table, boot-app metadata and
application. It changes the old flash partition layout. It does not format SD.
The OPT101 toggle must be enabled after wiring; radio starts in dashboard mode.
