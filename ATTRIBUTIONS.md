# Credits and upstream research

Flock Noir is made by Valleytech Custom Solutions, @valleytechsolutions, Your Pal Kal.

## Colonel Panic and OUI Spy

Please support **Colonel Panic**, whose OUI Spy ecosystem and surveillance
detection research informed Flock Noir's radio capabilities:

- [Colonel Panic's website](https://colonelpanic.tech/)
- [OUI Spy](https://github.com/colonelpanichacks/oui-spy)
- [OUI Spy Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue)
- [OUI Spy Unified Blue browser flasher](https://colonelpanichacks.github.io/oui-spy-unified-blue/)
- [Flock-You research and companion tools](https://github.com/colonelpanichacks/flock-you)
- [Colonel Panic on Tindie](https://www.tindie.com/stores/colonel_panic/)

Reference snapshot: Unified Blue commit
`0d67d145a5444b70a53061bb4ab00f0765cad245`, inspected September 22, 2026.
Its detector, foxhunter, promiscuous Flock-You, PCAP, Sky Spy and BLE-sniff
features informed the independent implementation described in [RADIO.md](docs/RADIO.md).
The signature facts and research contributions are credited; Unified Blue's
application code and UI are not copied. No endorsement or affiliation is implied.
Use its flasher for OUI Spy hardware/software, not for this Flock Noir build.

## Research and protocol contributors

- NitekryDPaul and DeFlockJoplin: community WiFi prefix and probe-IE research,
  credited in Unified Blue's Flock-You implementation.
- [Will Greenberg / wgreenberg](https://github.com/wgreenberg/flock-you): BLE
  manufacturer-ID research acknowledged by Unified Blue.
- [Noflock / Flock-IR-Detection](https://github.com/Noflock/Flock-IR-Detection):
  photodiode pulse detection research and motivation for a dedicated analog path.
- [Open Drone ID](https://github.com/opendroneid/opendroneid-core-c): public
  protocol layouts used to implement independent bounds-checked decoding. Its
  Apache-2.0 reference library credits Intel and the Open Drone ID contributors;
  that library is not bundled as a Flock Noir dependency.
- [Texas Instruments OPT101 documentation](https://www.ti.com/product/OPT101):
  photodiode/amplifier specifications and pin wiring.
- [Seeed Studio](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/):
  XIAO ESP32-S3 Sense hardware documentation.
- [BlueZ](https://github.com/bluez/bluez): Linux Bluetooth tooling and public HCI
  definitions used by the Pi transport. No BlueZ application source is copied.
- [Radiotap](https://www.radiotap.org/): packet metadata format for the Pi monitor
  adapter transport, implemented independently for the fields used here.

Additional public signature references used in 0.4.2:

- [Unified Blue Flock-You research](https://github.com/colonelpanichacks/oui-spy-unified-blue/blob/0d67d145a5444b70a53061bb4ab00f0765cad245/src/raw/flockyou_promiscious.cpp):
  Penguin/bare serials, FS Ext Battery, Xuntong, Raven and Nordic DFU hints.
- [Flipper Devices serial profile](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/targets/f7/ble_glue/profiles/serial_profile.c)
  and [advertised name](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/targets/f7/furi_hal/furi_hal_version.c):
  public advertised service/appearance values and name format. Implemented
  independently; no Flipper firmware source is bundled.
- [Hak5 setup documentation](https://github.com/hak5/hak5-docs/blob/master/setup/connecting-to-the-wifi-pineapple-over-wifi.md):
  Pineapple setup SSID convention. Other named-SSID rules are weak hints only.

Pi 0.4.2 compiles Flock Noir's own shared `pulse_detector.h` and
`radio_protocol.h`; these use the same research credits as the XIAO target.

## Existing project credits

- [@hamspiced / Midwest Gadgets - Piglet](https://github.com/hamspiced/piglet):
  wardriving workflow and WiGLE-format inspiration.
- [justcallmekoko / ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder):
  ESP32 wireless-tooling inspiration.
- [Mikal Hart / TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus): NMEA parsing.
- [Espressif Arduino ESP32](https://github.com/espressif/arduino-esp32) and the
  ESP-IDF / Apache NimBLE contributors: device drivers and Bluetooth stack.
- [ITU-T T.81 / JPEG](https://www.w3.org/Graphics/JPEG/itu-t81.pdf): baseline JPEG
  syntax and DC block-average relationship used by the independently implemented
  camera luminance reader. No third-party decoder source is bundled.
- Mario opening motif: *Super Mario Bros.*, music by Koji Kondo (Nintendo).

Flock Noir's original code remains under [LICENSE](LICENSE). Dependencies retain
their own licenses. Credits do not replace any upstream license requirements.
