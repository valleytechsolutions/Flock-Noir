# Flock Noir 0.5.1 - Raspberry Pi Zero 2 W

Installable Raspberry Pi OS Lite arm64 image with Flock Noir, the shared XIAO/Pi
pulse and radio parsers, unchanged dashboard design, BLE scanning, WiFi candidate
detection, watchlists, Remote ID, evidence logs and capture downloads.

This release adds the ALPR focus profile, a dedicated General Scanner tab,
four-source evidence ages and combined CSV methods such as `ir+camera+ble+wifi`.
The native C++ assessment is shared with XIAO. ALPR focus enables BLE and priority
field scanning, limiting automatic sounds to camera candidates while retaining
all device observations. General alerts restores per-device sounds; existing
custom RTTTL settings survive. The UI keeps its dark/green design.

Biscuit name/service candidates are supported with an editable sound. Its UUID
alone is shared with an Arduino example and is not a Biscuit identification.
Pineapple Pager remains a recognizable Pineapple-family AP-name candidate,
not a unique Pager-model identification. Existing Flock-You OUI/probe/BLE,
Axon, Ring, Meta, Flipper, Pineapple and drone rules remain.

The browser web flasher is for XIAO only; use the Pi image below for Zero 2 W.

Flash `flocknoir-pi.img.xz` with Raspberry Pi Imager's **Use custom** option.
Verify the adjacent SHA-256 checksum. Join **Flock Noir**, password **flocknoir**,
and open **http://192.168.4.1**. SSH login is **flock / flocknoir**; change it
with `passwd` after first boot.

The OPT101 detector starts off. Wire OPT101 OUT to MCP3008 CH0, with the module,
ADC VDD and ADC VREF powered from 3.3 V, then enable the detector. Pi has no
built-in analog input. GPS remains at 9600 baud on `/dev/serial0`.

Onboard WiFi provides surveys and the hotspot. Passive WiFi packet capture
requires a separate monitor-capable USB adapter configured in `pi/config.py`.
Pi field mode hops that adapter while preserving dashboard access.

Automated parser/API tests also run against the ARM library during image
provisioning. Physical Pi camera/radio/optical testing has not been performed
in this session. Detection evidence does not establish a camera's identity.

See [Pi wiring and setup](https://github.com/valleytechsolutions/Flock-Noir/blob/main/pi/README.md).
Research and feature inspiration: [Colonel Panic](https://colonelpanic.tech/) and
[OUI Spy Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue).
