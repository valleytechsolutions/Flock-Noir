# Radio and IR detection in Flock Noir 0.4.4

The XIAO ESP32-S3 Sense build combines OPT101 pulse sampling, OV2640 camera
analysis, passive WiFi observations, BLE advertisements, GPS and SD logging.
The original four-tab interface remains the control panel. Pi 0.4.4 shares the
native pulse/radio parsers and APIs, with MCP3008 input, legacy BLE HCI scanning,
and a separate monitor-capable USB WiFi adapter for passive packets. See
[Pi Zero 2 W setup](../pi/README.md#radio-and-opt101-update-044).

Radio rules run automatically, even with OPT101 off and WiGLE disabled. The
Detector tab now shows radio-only alerts and an ALPR/device table with event-log
download; the existing Wardrive controls still choose channel/mode/captures.
The detections tile includes radio event records as well as optical records.
These are event counts, not unique cameras. Stronger ALPR/Flock rules take
priority in the radio banner; a match expires there after eight seconds without
fresh matching evidence. Logs and the device table retain history.

The OUI set is the same complete 34-prefix union as Unified Blue's Flock-You;
regression tests check every prefix as transmitter, receiver and management
BSSID. OUI-only hits are `possible_camera`, stronger probe/advertisement rules
are `camera_signature_match`, and a qualifying radio rule plus camera or OPT101
evidence within three seconds is `corroborated_camera_candidate`. Logs retain
both optical flags, the rule, tier and fresh GPS position when available. GPS
locates the observation; it does not increase identity confidence. These
assessments do not establish that nearby radio and light share a source. No
match means unknown, never proof that a device is not a camera.

## Device sounds and CSV

Settings provides a persistent sound assignment for each of these categories:

| Candidate / evidence | Default sound |
|---|---|
| ALPR OPT101, BLE, WiFi, camera pulse or radio + optical | Original retro blaster phrase |
| Axon | Alternating two-tone siren |
| Ring | Question chime |
| Meta glasses | Confused descending phrase |
| Flipper Zero | Digital warble |
| WiFi Pineapple | Network warning |
| Drone | Radar phrase |
| Other watchlist | Triple chirp |

Each ALPR method has its own assignment, including combined evidence. Any
category can use any preset, a saved custom RTTTL slot, or Silent. Existing
custom tones survive the upgrade. Deleting an assigned slot switches that
assignment to Triple chirp; later slot references shift with their tones.
Test previews the chosen sound before saving. A passive piezo is required;
a fixed-pitch active buzzer cannot reproduce melodies.

New encounters, stronger signatures, optical corroboration and reappearance
after 60 seconds can request a sound. A bounded queue retains one pending sound
per category, prioritizes ALPR, waits for the current melody to finish and
expires requests after 30 seconds. Each category has a four-second cooldown.
Mute/buzzer-off clears pending sounds; detection and logging continue. Weak
OUI/shared-service hints remain candidates even if they make a sound.

The Detector CSV download (`/api/log`, `/logs/flock_*.csv`) now contains optical
**and radio** rows. `source` identifies the originating path; `detection_method`
is `ir`, `camera`, `ble`, `wifi`, or a combination such as `ir+ble`.
`radio_method` preserves the exact rule (`penguin_serial`, `wildcard_ie`,
`oui_addr2`, etc.). `category`, `assessment`, `radio_tier`, MAC, RSSI and the
optical evidence flags are also included. Radio-only rows leave frequency and
duty empty because a packet does not measure a flash. No GPS fix means no valid
position. SD must be mounted for XIAO CSV persistence; storage errors appear in
the health panel. Existing CSV files are left untouched; each boot starts a new
session. Radio JSONL and optional packet captures are still available.

Corroboration works in either arrival order: an optical row can refer to a
recent qualifying radio match; a subsequent radio observation can log an
optical upgrade even inside the normal duplicate window. This does not merge
MACs, prove a shared physical source or label an IR timing match as confirmed
ALPR identity.

`CMD:SETTINGS` reads sound assignments over USB. `CMD:TEST_ALERT` previews the
ALPR BLE assignment; `CMD:TEST_SOUND:axon` (or another category ID) previews
that assignment without logging a detection or modifying settings.

## XIAO operating modes

| Mode | WiFi | BLE | IR / camera | Dashboard |
|---|---|---|---|---|
| Dashboard | Promiscuous reception on the AP channel; passive WiGLE surveys when no client is connected | Passive legacy advertisements, approximately 10% scan window | Both enabled paths continue | Available |
| Field | Priority 1/6/11 or all 1–11, 350 ms per channel; WiGLE logs observed beacons when enabled | Same passive scan | Both enabled paths continue | Hotspot off |

Enable the OPT101 toggle in Detector and WiGLE toggle in Wardrive, then apply
Field mode in Wardrive. Hold the onboard BOOT button for 1.5 seconds to return to
the dashboard. Reboot always returns to the dashboard. Capture and field mode
are deliberately not persisted; BLE preference, watchlist, target, dashboard
channel and field scan plan are saved. A mode change takes effect about 1.2 seconds after Apply.

There is one shared 2.4 GHz radio. WiFi channel hopping and BLE scanning divide
airtime; these are not simultaneous receivers on every channel. The hotspot
transmits beacons in Dashboard mode, so that mode is not radio-silent. Field mode
does not start an AP, associate with networks, send WiFi probes or BLE scan
requests. It receives management/data frames and BLE advertisements. No 5/6 GHz,
cellular reception, encrypted payload decryption, or connected BLE traffic.

On Pi, field mode hops the dedicated USB monitor adapter and leaves the
onboard hotspot running. Dashboard mode fixes the monitor channel. Without a
monitor adapter, onboard WiFi surveys still report OUI/SSID candidates when
wardriving is enabled and no hotspot clients are present. Survey evidence is
explicitly labeled and is never synthesized into captured packets. Pi radio
queues hold 256 observations and its live table holds at most 128 devices.

## Features and provenance

Inspired by [Colonel Panic's Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue)
at commit `0d67d145a5444b70a53061bb4ab00f0765cad245`, inspected September 22, 2026.
Flock Noir implements its own parsers, scheduler, UI integration and storage.
The inspected Unified Blue repository has no top-level license granting reuse
of the complete application. Its application source is not vendored here.
Public protocol identifiers and research findings are credited below and in
[ATTRIBUTIONS.md](../ATTRIBUTIONS.md).

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

### Watchlist syntax

One rule per line, up to 1024 bytes total. Names are case-insensitive substrings;
hex values have no `0x` prefix. These supplement the built-in candidate rules.

```text
oui:B4:1E:52
mac:AA:BB:CC:DD:EE:FF
name:Penguin
cid:034D
svc:FC81
```

## IR evidence

OPT101 on D1/ADC1 is sampled by a dedicated FreeRTOS task, targeting 1 kHz.
Current defaults accept 8-12 Hz, 10-30% duty, 8-35 ms width and four consecutive
valid intervals, with at most 15% period change between accepted cycles.
Invalid intervals, long pulses, ADC clipping and sampling gaps over 5 ms reset
the train. The live panel reports actual rate, noise estimate, raw ADC, baseline,
pulse width and gaps. These defaults are tunable starting points, not a
universal ALPR signature or a field-calibrated classifier.

The noise estimate uses quiet-sample differences. Hysteresis and an adaptive
threshold help reject noise; optical filtering and gain/headroom remain
necessary outdoors. OPT101 sees visible and near-IR light and cannot measure
wavelength. Its amplifier can saturate below the ADC full-scale rail, so lack
of an ADC-clipping warning does not prove that the optical signal is usable.

- `camera_pattern`: the camera's spatial/temporal heuristic matched.
- `ir_timing_match`: OPT101 pulse timing passed the configured gates.
- `optical_radio_nearby` (optical CSV) / `ir_radio_nearby` (radio JSON): an optical match and a qualifying Flock radio candidate occurred
  within the 3-second correlation window. This is temporal co-occurrence,
  not proof of a common source, confirmed identity or location of a camera.

Camera and OPT101 use independent event logging. An eight-entry queue retains
OPT101 events while the foreground loop is busy. Its overflow counter is in
`/api/status`. Slow downloads/recording can still delay logging, camera frames
and radio queue draining. Monitor the counters during hardware testing.

### Camera view

The XIAO uses a VGA (640x480) sensor frame instead of enlarging a 160x120 image.
The sensor produces JPEG directly; a background task reads its 8x8 luminance
block averages into an 80x60 analysis grid without full image decoding.
Small/distant lights can be diluted by block averaging; OPT101 remains the
primary pulse-timing input. This avoids the corruption seen with high-rate raw
VGA while the radios were active. The browser uses normal image interpolation.
Exposure/gain remain fixed for
pulse detection, so the view remains a near-IR diagnostic rather than an
auto-exposed general-purpose camera. An IR-cut-free lens and fixed white balance
can produce unusual colors in daylight. Actual preview/recording rate depends on
SD/network load; sensor capture and actual analysis rate are reported separately.
The Detector FPS tile reports analysis, not merely camera capture.
Timing gaps over 50 ms reset the camera pulse window instead of classifying
aliased flicker. OPT101 sampling remains independent.

`python tools/camera_snapshot.py --port COM44 --output snapshot.jpg` retrieves
one bounded diagnostic JPEG over USB. Use the actual serial port. This is a
snapshot utility, not a high-rate video transport.

## Data and resource limits

| Data | Path | Behavior |
|---|---|---|
| IR / camera events | `/logs/flock_*.csv` | Separate source and evidence columns |
| WiGLE networks | `/wardrive/wigle_*.csv` | GPS fix required by default; passive field captures mark unknown encryption explicitly |
| Radio candidates | `/radio/events_*.jsonl` | GPS validity, RSSI, method, tier, IR/camera evidence and drone telemetry |
| WiFi capture | `/radio/wifi_*.pcap` | Optional; 16 MiB session limit |
| BLE capture | `/radio/ble_*.jsonl` | Optional; 16 MiB session limit |

Random session filenames prevent normal reboot collisions. Files from previous
boots remain on SD. The Saved sessions button lists up to 100 radio files for
download; larger collections can be read with a card reader. No automatic SD
formatting or deletion. Power loss can leave the last record incomplete.

The radio queue holds 32 observations and the live table 64 devices. The
foreground drains up to eight observations per loop. Overflow is counted and
does not block the WiFi/BLE callback. Repeated candidate events are limited to
one per MAC/protocol per ten seconds, except rule-strength or optical-corroboration upgrades.
New attention-worthy candidates can sound an alert, including weak Flock hints. Global mute suppresses sounds,
not logging. Capture limits and SD failures are exposed in the UI.

GPS fixes older than three seconds are rejected. Events delayed over three seconds
are logged without observer coordinates rather than attaching a later location.
Detection CSV includes observed `uptime_ms` and `logged_uptime_ms`; its UTC field is
logging time. This makes a delayed queue drain visible. `uptime_ms` is time since
boot, not Unix time. Missing JSON coordinates are `null`. The observer's GPS
position must not be interpreted as the target's location. Drone positions are
decoded broadcasts, separately labeled, and can be spoofed.

USB commands at 115200 baud: `CMD:STATUS`, `CMD:HEALTH`, `CMD:VERSION`, `CMD:DUMP_LIVE`,
`CMD:CLEAR_LIVE`, `CMD:TEST_ALERT`, `CMD:FRAME`.
`CMD:HEALTH` returns the same camera, GPS, SD and ADC diagnostics
as `/api/status`, for checking a flashed board without changing WiFi networks.
Clear Live affects RAM only. Output uses a bounded write per
foreground iteration. Historical SD files are available through the dashboard.

## Validation before relying on a build

Host tests exercise pulse acceptance/rejection, gaps, rollover, BLE composite
rules, random addresses, bounds and malformed packet input. The firmware must
also compile for the actual XIAO Sense target. Neither substitutes for hardware
validation: run a known 10 Hz / 20 ms optical source with WiGLE, BLE, recording
and captures enabled; verify measured rate and event counts. Test 50/60 Hz,
sunlight, a TV remote, radio-only candidates, loss of GPS, an absent/full SD,
and BOOT recovery. Record false positives and missed events before changing
thresholds. No detection range or highway-speed guarantee has been established.

## Detection tuning checks (0.4.4)

The OPT101 path keeps the 1 kHz sampler, 8–12 Hz / 10–30% duty / 8–35 ms
profile and four consecutive interval requirement. No threshold was loosened
to manufacture confirmations. Synthetic tests exercise valid trains, noise,
wrong rates, long/short pulses, clipping, dropped samples and timer rollover.
The sensor must be wired and enabled to supply optical evidence. A 1 kHz ADC
counter by itself proves only that the sampler runs. See the
[TI OPT101 specification](https://www.ti.com/product/OPT101) for its broad optical
response and amplifier limits.

VGA camera analysis uses fixed exposure/gain/white balance and block-average
brightness. Its JPEG decoder now consumes long Huffman prefixes once and uses
speed optimization on ESP32, preserving the image and parser bounds. Camera
frame sampling can still miss 20 ms flashes; OPT101 is the finer timing sensor.
Use a known pulsed optical source and actual field targets to calibrate optical
aim, ambient-light headroom and detection range. No universal ALPR frequency
has been established by this work.

The passive radio rules are checked against
[Unified Blue’s Flock-You description](https://github.com/colonelpanichacks/oui-spy-unified-blue#mode-3-flock-you--promiscuous-wifi-edition):
34 OUI prefixes, transmitter/receiver paths, wildcard probes and the IE
fingerprint, plus passive BLE accessory signatures. The XIAO still shares
one radio between WiFi and BLE; coverage is intermittent while hopping.
