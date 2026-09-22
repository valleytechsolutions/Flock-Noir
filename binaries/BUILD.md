# Flock Noir 0.4.3 - XIAO ESP32-S3 Sense

Flock-You's complete 34-prefix OUI set now feeds the main Detector view, with
serial/DFU hints, Axon, Meta glasses, Flipper Zero and WiFi Pineapple candidates.
New encounters play a Mario phrase. Logs distinguish possible cameras, stronger
signatures and nearby optical corroboration, while suppressing duplicate sounds.

The camera now supplies a clean 640x480 JPEG preview with normal browser scaling.
A separate task extracts block-average brightness directly from JPEG for fast
analysis. This avoids enlarging a 160x120 image and avoids raw VGA corruption
under radio load. The original four-tab dashboard design is preserved.

Join WiFi **Flock Noir**, password **flocknoir**, then open **http://192.168.4.1**.

Factory image: `FlockNoir-merged-0x0.bin`, flash at **0x0**.
SHA-256: see `FlockNoir-merged-0x0.bin.sha256`.

Built with PlatformIO Core 6.1.19, pioarduino platform 55.03.39,
Arduino ESP32 3.3.9 and TinyGPSPlus 1.0.3. Hardware configuration:
8 MB flash, OPI PSRAM, ATGM336H on D7/D6 at 9600 baud, OPT101 OUT on
D1/GPIO2, OV2640 without IR-cut filter, Sense SD on CS GPIO21.

Validation on September 22, 2026:

- Target firmware compiles successfully without compiler warnings/errors.
- Host C++ tests cover every Flock OUI address role, added device signatures,
  alert timing, camera compactness/staleness, pulse validation, malformed radio
  packets and Remote ID. JPEG tests compare luminance with independent reference
  fixtures, including subsampling, optimized Huffman tables, restart markers,
  every truncated prefix and a mutation corpus.
- Twenty Pi parser/API tests cover logging, camera assessments, immediate
  optical upgrades, Mario notification dispatch, mute and duplicate suppression.
- Browser tests cover desktop/mobile layout, escaped device names,
  settings submission, IR/radio banner and Pi capability fallback.

On the connected XIAO ESP32-S3 Sense, flash hashes verified and a 30-second
hardware check passed with VGA capture around 24-25 fps and actual analysis at
22.9-25.0 fps, zero frame errors, ADC sampling at 1000 Hz, WiFi/BLE scanning
active, no additional dropped radio packets during the check, SD mounted, and
new valid ATGM336H messages with zero checksum failures. GPS had no satellite
fix during the check. OPT101 was not wired, so its detection toggle stayed off;
ADC activity does not establish optical detection or sensor connection.
The cumulative radio drop counter was 743 before the measurement and stayed
unchanged; this is not a claim of lossless reception from startup or in the field.
A USB JPEG snapshot was visually checked for corruption. The Mario test command
was accepted by the board; physical audibility cannot be measured over USB.

Optical range, field identification accuracy, live HTTP/recording load, sustained
SD throughput and known-source pulse validation remain untested on hardware.
The 80x60 block-average analysis can dilute small lights; 25 fps cannot prove a
20 ms pulse. Wire OPT101 for finer timing. Neither timing nor shared radio
signatures establish a camera's identity or prove radio/light share a source.

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

The factory image includes bootloader, the current partition table, boot-app
metadata and application. It does not format SD.
The OPT101 toggle must be enabled after wiring; radio starts in dashboard mode.

Research and inspiration: [Colonel Panic](https://colonelpanic.tech/) and
[OUI Spy Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue).
