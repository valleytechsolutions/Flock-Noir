# XIAO 0.6.0 — scan modes, radio and optical evidence

This guide applies to **Seeed XIAO ESP32-S3 Sense** with OV2640. Raspberry Pi stays
on its separate 0.5.1 release and [Pi guide](../pi/README.md).

## Exclusive modes

The scan profile is the single persisted mode. Selecting ALPR, Scanner, Pig
Detector or Wardrive changes actual firmware work, not just the displayed table.
Camera and Settings leave the chosen mode active. A failed settings write leaves
the previous mode running. On a successful change, pending automatic tones, live
device rows, optical history and fusion evidence are cleared. Packet and camera
queues carry generation numbers so a previous mode's queued samples cannot be
processed as new detections. Optical sensor enable and tone assignments persist.

| Mode / API value | Optical analysis | WiFi | BLE | Automatic outputs |
|---|---|---|---|---|
| ALPR / `alpr` | OPT101 + camera | ALPR/Flock OUI and probe rules | ALPR/Flock rules | ALPR tones, ALPR CSV, radio events |
| Scanner / `general` | Paused | General signatures, watchlist, Remote ID, optional captures | General signatures, watchlist, Remote ID, optional captures | Per-device tones and radio events; ALPR radio candidates also enter ALPR CSV |
| Pig Detector / `axon` | Paused | Detection off | Axon-only matching | Fresh-evidence siren and Axon radio events |
| Wardrive / `wardrive` | Paused | Passive AP surveys / beacons | Advertisements, including unknown vendors | WiGLE CSV only; no automatic detection alerts |

OPT101's saved enable switch is independent of the runtime pause. Camera DMA can
continue acquiring frames, but optical analysis stops outside ALPR. Requested
preview remains available; it is not a scanner. Recording and microphone capture
are stopped and blocked in Pig Detector and Wardrive.

## Pig Detector

Passive BLE discovery runs indefinitely with duplicate advertisements enabled.
Only Axon candidate advertisements enter its queue. It does not scan WiFi,
classify other device categories, run target-tracking tones or populate ALPR CSV.

Requested BLE timing is **90/100 ms** window/interval with the hotspot, or
**100/100 ms** with field coverage and WiFi switched off. These are configured
windows, not a claim of lossless reception. BLE advertisements may be missed
because of interference, address rotation, antenna placement or device state.
Normal ALPR, General and Wardrive discovery requests **100/200 ms** to share the
radio with WiFi. Scan faults retry on a nonblocking timer.

Axon signatures use SIG company **034D**, member service **FC81**, Axon names and
public-address OUI **00:25:DF**. Company+service in one advertisement is stronger
than either alone. Shared vendor identifiers do not establish a body-camera
model. The banner therefore reads **POSSIBLE AXON BODY CAMERA NEARBY**. Absence of
advertisements is not absence of a camera: [Axon's own Body 4 guide](https://www.axon.com/help/axon-body-4/cameras-and-sensors/body/body-nearby-activation.htm)
describes conditional and time-limited beacons.

A fresh match requests the **Axon** sound. One global reminder timer prevents
advertisement floods or multiple MACs from flooding the buzzer. Default repeat:
**10 seconds**, editable **5–60 seconds** in this tab. No reminder occurs if the
latest supporting advertisement is more than **3 seconds** old. Mute, disabled
buzzer and a silent Axon sound are respected. Settings can select another sound
or a custom RTTTL slot.

## Radio coverage and recovery

**Dashboard**: hotspot on `192.168.4.1`, default channel 1. Packet reception stays
on its channel while clients are connected. Wardrive can perform passive surveys
when no hotspot client is present. **Field**: hotspot off; ALPR priority 1/6/11,
Wardrive 1–11, General configured plan; dwell 350 ms. Pig Detector instead turns
WiFi off entirely. BLE continues in all four modes. WiFi and Bluetooth cannot
receive all channels at once on this shared radio.

Hold **BOOT 1.5 seconds** to restore the dashboard. Restarting also restores the
hotspot while preserving the selected scan mode. Changing coverage never changes
the scan profile. XIAO has no Bluetooth Classic, 5/6 GHz WiFi or sub-GHz receiver.

## ALPR focus and evidence fusion

OPT101 (~1 kHz), camera (~25 fps), BLE and WiFi share a bounded 3-second evidence
window. Matches arriving in either order can add support. Radio evidence is
tracked by strength independently: a weak vendor hint cannot hide or refresh a
stronger old signature. MAC identities remain separate in the radio log; proximity
of optical and radio observations does not prove a shared emitter.

`no_recent_evidence` means no current match, not no camera. Optical evidence alone
is `possible_camera`; sufficiently specific nearby radio evidence adds
`corroborated_camera_candidate`. The four-source case reports
`multiple_sources_nearby`. These are assessments, not calibrated probabilities.

**Frequency:** 8–12 Hz / 10–30% duty / 8–35 ms is an experimental reference profile,
not a manufacturer-verified universal ALPR rate. OPT101 validates four consecutive
intervals, regularity, adaptive amplitude threshold, clipping and sampling gaps.
Camera duty and pulse timing are exposure/frame limited. A narrow 10 Hz source
sampled at 25 fps can appear as 5 Hz; camera aliases are retained as weaker
candidates with observed frequency and `camera_timing=aliased_candidate`. They do
not count as an OPT101 timing measurement. [Detection and bench validation](../DETECTION.md).

## Separate files and data quality

| Data | File | Contents |
|---|---|---|
| ALPR evidence | `/alpr/alpr_*.csv` | 31 columns; optical / ALPR radio only |
| Survey | `/wardrive/wigle_*.csv` | WiGLE 1.4, `WIFI` and `BLE` types |
| Radio events | `/radio/events_*.jsonl` | Category, rule, tier, MAC, protocol, RSSI, uptime, GPS, mode and evidence |
| General packet capture | `/radio/wifi_*.pcap`, `/radio/ble_*.jsonl` | Optional raw observations, 16 MiB cap per capture |

`GET /api/alpr.csv` and the legacy `/api/log` return the ALPR file. Its
`detection_method` is `ir`, `camera`, `ble`, `wifi` or a combined value such as
`ir+camera+ble+wifi`. `source` is the event that caused the row. Pulse fields on
radio-only events remain blank; they are not guessed from a MAC. Additional
columns retain OPT101 pulse width / sample rate, camera sample rate / timing
ambiguity and scan mode. Optical evidence can log without GPS, with an uptime
stamp and unavailable coordinates. Non-ALPR devices never enter the ALPR CSV.

Wardrive deliberately does not classify or alert. It observes WiFi APs and BLE
advertisers, including unknown vendors. Rows require a current GPS fix, UTC,
altitude and HDOP. `AccuracyMeters` is **HDOP × 5 m estimated accuracy**, recorded
in the pre-header; it is not measured receiver uncertainty. BLE channel is **0
(unknown)** because legacy NimBLE reports do not identify the advertising channel.
WiFi security uses parsed RSN/WPA evidence; a privacy bit alone stays UNKNOWN.

SSID / names are quoted, quotes doubled, and line breaks replaced by spaces for
WiGLE import. Repeated protocol/address observations log at most once per 15
seconds, allowing measurements at new locations. Failed writes and missing GPS
are not remembered as successfully logged. The dashboard exposes WiFi/BLE row
counts, SD errors and observations skipped for missing GPS. WiGLE upload itself
has not been exercised with a real field session.

## Protocol provenance and retained General features

Inspired by [Colonel Panic](https://colonelpanic.tech/) and
[OUI Spy Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue).
Please support his work. Flock Noir independently implements the parsers,
scheduler, storage and UI; upstream application code/UI is not vendored.
[Complete attributions](../ATTRIBUTIONS.md).

| Unified Blue area | Flock Noir implementation |
|---|---|
| Detector | BLE vendor signatures and configurable MAC, OUI, name, manufacturer ID and 16-bit service watchlists |
| Foxhunter | Selected MAC RSSI, last-seen age and nonblocking proximity tones; no meter/distance estimate |
| Flock-You | Transmitter, receiver and management-BSSID prefix matching; wildcard probe and strict IE signature matching; Flock accessory BLE signals |
| PCAP | Optional SD WiFi PCAP, raw 802.11 linktype 105, 768-byte snap length; bounded receive queue and visible drop count |
| Sky Spy | Remote ID over WiFi vendor beacons, NAN service-discovery action frames and legacy BLE service data; ID, location, operator ID/location, description and message-type mask |
| BLE Sniff | Live advertisement table and raw advertisement JSONL including address type, event type and RSSI |

BLE capture is JSONL, not a reconstruction of over-the-air BLE PCAP. WiFi PCAP
has no radiotap header and uses boot-relative timestamps. Extended/coded-PHY BLE
Remote ID is not implemented. Remote ID authentication messages are recognized
and retained in raw captures, but their signatures are not verified. This is
feature integration for this hardware, not binary or API compatibility with
Unified Blue's Flask dashboard, boot selector or session CRC format.

### Signature interpretation

- The 34 Flock candidate WiFi prefixes come from Unified Blue's documented union
  of NitekryDPaul / DeFlockJoplin field research and its camera-firmware research.
  Most are shared chip/vendor prefixes. `82:6B:F2` is a research prefix with the
  locally administered bit set, not a registered vendor OUI.
- Receiver (`addr1`) and management BSSID (`addr3`) matches are tier 1. Their
  frame RSSI measures the transmitter, not the receiver named in that address.
  These matches do not drive the target proximity tone or IR correlation.
- Transmitter-prefix matches are tier 2. Wildcard probes from a listed prefix
  are tier 3. Exact ordered IE sequence plus wildcard and listed prefix is tier 4.
  The strict fingerprint is `2,12,127,221:506f9a16030103,45,191,221:0050f208000000`;
  malformed IEs are rejected, not repaired heuristically.
- BLE Flock accessory UUID: `e8ccbb38-9532-46a8-9fe5-1814df172e6f`. Penguin,
  FS Ext Battery and Flock names are candidates. Xuntong company `09C8` and
  Raven's reported `3100`-`3500` service range are weak tier-1 hints; they do not
  independently corroborate IR. Exact ten-digit serials, `DfuTarg`, and Nordic
  DFU UUID `00001530-1212-efde-1523-785feabcd123` are also tier-1 Flock-You hints.
  Nordic DFU and serial formats are shared by unrelated devices; they never
  independently corroborate IR. Exact Penguin serial names use `penguin_serial`.
  WiFi names include `FS Ext Battery`; Flock Noir's own default SSID is excluded.
- Axon: company `034D`, service `FC81`, public OUI `00:25:DF`.
- Meta glasses: company `0D53` AND service `FD5F` in the same advertisement, or
  a Ray-Ban / Wayfarer / Oakley Meta name. Either numeric signature alone does
  not trigger that preset. Advertised names can be spoofed.
- Flipper Zero: official serial-profile advertised services `3080`-`3083`
  (base `3080` plus hardware color) with appearance `8600` are tier 3; service
  alone is tier 1; `Flipper` / `Flipper ...` names are tier 2. No generic ST OUI
  rule is used. Bluetooth-off, connected/nonadvertising and renamed/custom
  profiles can be missed. This does not detect sub-GHz, NFC or RFID activity.
- WiFi Pineapple: beacon/probe-response SSIDs `Pineapple_` plus four hex digits,
  `Pineapple_Management`, `Pineapple`, or `WiFi Pineapple` are tier-2 candidates.
  A client's probe request for that name is not classified as a Pineapple AP.
  Renamed/hidden devices can evade these rules; a generic `Open` SSID does not
  match. These rules cannot prove PineAP activity or identify every Pineapple.
- Public-address vendor presets include Ring, Axon, Flock, DJI, Parrot and
  Skydio. Vendor matching is disabled for random BLE addresses. A manually
  supplied watchlist can still match such an address, but it may rotate.

Tiers describe the matched rule, not a calibrated probability of identification.
An unrelated transmitter can satisfy a vendor-prefix or probe fingerprint rule.


Biscuit uses advertised name `Biscuit`; the additional documented service UUID
adds support only alongside the name. Its service alone is shared with a generic
Arduino BLE example. Pineapple Pager has no verified unique model fingerprint;
recognizable Pineapple-family network names produce family candidates only.

## Controls and diagnostics

- `POST /api/profile`: `profile=alpr|general|axon|wardrive`.
- `POST /api/coverage`: `mode=dashboard|field`, preserving the scan profile.
- `POST /api/axon`: `repeatSeconds=5..60`.
- `POST /api/wardrive`: legacy `en=1` selects Wardrive; `en=0` selects General.
- `GET /api/status`: sensors, mode, readiness, logs, waveform, GPS and evidence.
- `GET /api/radio`: BLE requested window/interval, scan state, drops, heap and settings.
- USB: `CMD:PROFILE:<mode>`, `CMD:COVERAGE:<coverage>`, `CMD:HEALTH`, `CMD:STATUS`.

In General, additional watchlist lines accept `oui:B4:1E:52`,
`mac:AA:BB:CC:DD:EE:FF`, `name:Penguin`, `cid:034D`, `svc:FC81`; maximum 1024 bytes.
Names are case-insensitive substrings. Watchlist and RSSI tracking do not operate
in the dedicated ALPR, Pig Detector or Wardrive profiles.

[Wiring and BOM](../HARDWARE.md) · [Build / install](../README.md#install-and-flash-xiao-esp32-s3-sense)
