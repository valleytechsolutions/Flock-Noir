# Flock Noir 0.5.0 - XIAO ESP32-S3 Sense

[Open the web flasher](https://valleytechsolutions.github.io/Flock-Noir/)
using desktop Chrome or Edge and a USB data cable. On an update leave Erase
device unchecked to retain settings. The installer writes separate parts that
exclude NVS. The downloadable merged image is for flashing at 0x0 and includes
NVS padding; it can reset settings. Its SHA-256 is in the adjacent checksum file.

This release adds an ALPR focus profile, a dedicated General Scanner tab,
four-source evidence ages and combined IR/camera/BLE/WiFi CSV methods. Focus
enables BLE and priority field channels and suppresses non-ALPR sounds while
retaining all device observations. General alerts restores per-device sounds.
The existing dark/green visual style, VGA camera preview, GPS wiring and custom
tone settings are preserved. Switching tabs alone does not change the profile.

Biscuit recognition uses its documented BLE name and optional advertised
service; the common Arduino example UUID alone never identifies Biscuit.
Pineapple Pager is covered only as a Pineapple-family AP-name candidate;
this does not uniquely identify a Pager or detect silent/renamed devices.
Flock-You's 34-prefix OUI union, probe/BLE rules, Axon, Ring, Meta, Flipper,
Pineapple and drone signatures remain enabled. Each device class has a sound
assignment in Settings, including Biscuit.

Join WiFi **Flock Noir**, password **flocknoir**, then open **http://192.168.4.1**.
Field mode turns off the XIAO hotspot; hold BOOT for 1.5 seconds to return.
Select Focus ALPR in the ALPR tab or Use general alerts in Scanner.
General alerts is the default for compatibility with existing installations.

Hardware: Seeed XIAO ESP32-S3 Sense only, 8 MB flash and OPI PSRAM,
OV2640 without IR-cut filter, Sense SD, ATGM336H GPS on D7/D6 at 9600 baud.
OPT101 module OUT → D1/GPIO2, VCC → 3V3, GND → GND. Enable it after wiring
and check the live response to light. First-install default is off; upgrades
preserve the saved enable setting. ESP32-CAM uses different pins and is not
supported by this image. Raspberry Pi has a separate release.

Validation on September 23, 2026:

- PlatformIO build passed without compiler warnings/errors; Core 6.1.19,
  pioarduino 55.03.39, Arduino ESP32 3.3.9 and TinyGPSPlus 1.0.3.
- Host C++ tests passed: optical pulse rules, all 34 OUI roles, malformed
  packets, device classifications, source fusion, expiry and timestamp rollover.
- 28 Pi tests passed, including both fusion arrival orders, combined CSV,
  profile persistence, sound filtering without stopping logs and Biscuit rules.
- Browser desktop/mobile checks passed for tabs, ALPR filtering, general
  filters, profile controls, settings/sound persistence and camera preview.
- Flasher loaded the real pinned ESP Web Tools 10.4.0 module in Edge.
  Unsupported-browser and metadata-failure cases keep installation disabled.
  Staging tests verified image ranges exclude NVS and reject stale/corrupt images.
  USB flashing through the browser chooser has not been exercised.
- Connected XIAO flashed via PlatformIO; esptool verified the written hash.
  A 30-second hardware check measured 24.9–25.0 camera analysis fps at
  640×480, zero decode errors, ADC at 1000 Hz, SD ready, WiFi/BLE active,
  zero added queue drops, and valid GPS sentences with zero checksum errors.
  Startup radio drops were nonzero; no satellite fix was available indoors.
- ALPR/General profile switching and the new fusion status were verified via
  USB, restoring the previous General profile. The attached board's saved
  OPT101 enable setting was on and was preserved; wiring is not verified by
  ADC activity. Physical Pi hardware was not attached.

Nearby optical and radio signals can come from different devices. The 3-second
fusion window and 8–12 Hz pulse profile provide supporting evidence, not proof
of camera identity or absence. OPT101 measures intensity/timing, not wavelength.
Known optical sources, field accuracy/range and sustained full-load operation
still need physical validation. A 25 fps camera can miss narrow pulses.

Reproduce:

```bash
python tools/html2header.py
python -m platformio run -e xiao_esp32s3_sense
python tools/package_firmware.py
python tools/build_flasher.py
```

Thanks to [Colonel Panic](https://colonelpanic.tech/) and
[OUI Spy Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue).
Full source provenance is in ATTRIBUTIONS.md.
