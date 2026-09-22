# Flock Noir 0.4.1 - XIAO ESP32-S3 Sense

This patch fixes a Bluetooth startup crash by reserving BLE memory before
Arduino initialization. It also initializes the ADC before configuring its
attenuation, enables direct camera DMA into PSRAM, services radio packets
between frames, and buffers GPS input during peripheral startup.

Join WiFi **Flock Noir**, password **flocknoir**, then open **http://192.168.4.1**.

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

On the connected XIAO ESP32-S3 Sense, flash hashes verified and a 30-second
hardware check passed with camera frames at 25.0-25.1 fps, ADC sampling at
1000 Hz, WiFi/BLE scanning active, zero dropped radio packets, SD mounted, and
new valid ATGM336H messages with zero checksum failures. GPS had no satellite
fix during the check. OPT101 was not wired, so its detection toggle stayed off;
ADC activity does not establish optical detection or sensor connection.

Optical range, field accuracy, recording/capture load, sustained SD throughput
and known-source pulse validation remain untested on hardware.

To reproduce and package:

```bash
python tools/html2header.py
python -m platformio run -e xiao_esp32s3_sense
python tools/package_firmware.py
```

To check a connected board after uploading (close any serial monitor first):

```bash
python tests/hardware_smoke.py --port COM44 --require-gps-data --require-sd
```

Use your board's actual port. Omit either peripheral requirement when absent.
The script reads diagnostics; it does not enable the unwired OPT101 detector.

The factory image includes bootloader, partition table, boot-app metadata and
application. It changes the old flash partition layout. It does not format SD.
The OPT101 toggle must be enabled after wiring; radio starts in dashboard mode.
