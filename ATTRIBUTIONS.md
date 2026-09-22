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

## Existing project credits

- [@hamspiced / Midwest Gadgets - Piglet](https://github.com/hamspiced/piglet):
  wardriving workflow and WiGLE-format inspiration.
- [justcallmekoko / ESP32 Marauder](https://github.com/justcallmekoko/ESP32Marauder):
  ESP32 wireless-tooling inspiration.
- [Mikal Hart / TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus): NMEA parsing.
- [Espressif Arduino ESP32](https://github.com/espressif/arduino-esp32) and the
  ESP-IDF / Apache NimBLE contributors: device drivers and Bluetooth stack.

Flock Noir's original code remains under [LICENSE](LICENSE). Dependencies retain
their own licenses. Credits do not replace any upstream license requirements.
