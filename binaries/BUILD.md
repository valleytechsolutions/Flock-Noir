# Flock Noir 0.4.4 - XIAO ESP32-S3 Sense

Device-specific alert sounds and one detection CSV for OPT101, camera, BLE and
WiFi. Settings assigns a preset, custom RTTTL slot or Silent independently to
each device/evidence category. ALPR candidates use an original retro blaster,
Axon a siren, Ring/Meta questioning tones, and Flipper/Pineapple/drone distinct
sounds. Existing custom tones survive the upgrade. The header byline is gone;
the footer credits Your Pal Kal. The four-tab visual design is preserved.

CSV rows record `source`, `detection_method` (including `ir+ble`), signature
rule, tier, assessment, MAC/RSSI, optical flags and fresh GPS coordinates when
available. The full 34-prefix Flock-You OUI union and WiFi/BLE rules remain.
Field scans now offer priority 1/6/11 or all 1–11, with 350 ms dwell. BLE and
the independent optical paths continue in both modes. Field mode turns off
the hotspot; hold BOOT for 1.5 seconds to restore it.

JPEG brightness decoding is faster without changing VGA image quality.
Invalid ATGM336H dates before lock are rejected instead of logged as UTC.

Join WiFi **Flock Noir**, password **flocknoir**, then open **http://192.168.4.1**.
Factory image: `FlockNoir-merged-0x0.bin`, flash at **0x0**.
SHA-256: see `FlockNoir-merged-0x0.bin.sha256`.

Built with PlatformIO Core 6.1.19, pioarduino platform 55.03.39,
Arduino ESP32 3.3.9 and TinyGPSPlus 1.0.3. No new dependencies.
Hardware: 8 MB flash, OPI PSRAM, ATGM336H on D7/D6 at 9600 baud,
OPT101 OUT on D1/GPIO2, OV2640 without IR-cut filter, Sense SD on CS GPIO21.

Validation on September 23, 2026:

- Firmware compiles without compiler warnings/errors.
- Host C++ pulse/radio tests pass, covering all 34 OUI address roles, valid and
  invalid pulse trains, clipping/gaps, rollover, device classification, alert
  queuing and malformed packets. JPEG fixture/truncation/mutation tests pass.
- 25 Pi tests pass, including category sounds, settings persistence, custom
  library preservation, mute/preview behavior, correlated CSV and Ring alerts.
- Browser checks pass at desktop/phone sizes, including sound assignments,
  previews, deletion/remapping of custom slots, save/reload and scan settings.
- The connected XIAO flash was hash-verified. A 30-second run measured camera
  analysis at 24.9–25.2 fps after decoder optimization (about 18 fps before),
  640x480 frames, no frame errors, ADC at 1000 samples/sec, active WiFi/BLE,
  SD mounted and fresh valid GPS sentences with zero checksum failures.
  No satellite fix was available indoors. No additional radio queue drops
  occurred during the check; cumulative startup drops were nonzero.
- ALPR, Axon and Meta preview commands were accepted over USB. Physical
  audibility and tone recognition were not measured through USB.

OPT101 detection remains off until the module is wired and enabled. ADC
activity alone does not prove a sensor is attached. The 8–12 Hz timing profile
is a starting point, not a universal ALPR identifier. Known optical sources,
field range/accuracy, sustained SD/HTTP load and physical Pi behavior still
need hardware validation. A 25 fps camera can miss 20 ms flashes; OPT101 provides
finer timing. Nearby light and radio can belong to different devices.

To reproduce and package:

```bash
python tools/html2header.py
python -m platformio run -e xiao_esp32s3_sense
python tools/package_firmware.py
```

To check a connected board (use its actual serial port):

```bash
python tests/hardware_smoke.py --port COM44 --require-gps-data --require-sd
```

The factory image includes bootloader, partition table, boot-app metadata and
application. It does not format SD. Existing tone slots, GPS pins and baud
remain unchanged. Reboot starts Dashboard mode.

Research and inspiration: [Colonel Panic](https://colonelpanic.tech/) and
[OUI Spy Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue).
