# Radio and IR detection in Flock Noir 0.4

The XIAO ESP32-S3 Sense build combines OPT101 pulse sampling, OV2640 camera
analysis, passive WiFi observations, BLE advertisements, GPS and SD logging.
The original four-tab interface remains the control panel. The Pi build retains
its existing camera/ADC implementation; this radio integration is XIAO-specific.

## Operating modes

| Mode | WiFi | BLE | IR / camera | Dashboard |
|---|---|---|---|---|
| Dashboard | Promiscuous reception on the AP channel; passive WiGLE surveys when no client is connected | Passive legacy advertisements, approximately 10% scan window | Both enabled paths continue | Available |
| Field | Passive hopping through channels 1-11, 350 ms per channel; WiGLE logs observed beacons when enabled | Same passive scan | Both enabled paths continue | Hotspot off |

Enable the OPT101 toggle in Detector and WiGLE toggle in Wardrive, then apply
Field mode in Wardrive. Hold the onboard BOOT button for 1.5 seconds to return to
the dashboard. Reboot always returns to the dashboard. Capture and field mode
are deliberately not persisted; BLE preference, watchlist, target and dashboard
channel are saved. A mode change takes effect about 1.2 seconds after Apply.

There is one shared 2.4 GHz radio. WiFi channel hopping and BLE scanning divide
airtime; these are not simultaneous receivers on every channel. The hotspot
transmits beacons in Dashboard mode, so that mode is not radio-silent. Field mode
does not start an AP, associate with networks, send WiFi probes or BLE scan
requests. It receives management/data frames and BLE advertisements. No 5/6 GHz,
cellular reception, encrypted payload decryption, or connected BLE traffic.

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
  independently corroborate IR. Bare ten-digit serial numbers are not matched.
- Axon: company `034D`, service `FC81`, public OUI `00:25:DF`.
- Meta glasses: company `0D53` AND service `FD5F` in the same advertisement, or
  a Ray-Ban / Wayfarer / Oakley Meta name. Either numeric signature alone does
  not trigger that preset. Advertised names can be spoofed.
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
- `ir_radio_nearby`: an IR match and a qualifying Flock radio candidate occurred
  within the 3-second correlation window. This is temporal co-occurrence,
  not proof of a common source, confirmed identity or location of a camera.

Camera and OPT101 use independent event logging. An eight-entry queue retains
OPT101 events while the foreground loop is busy. Its overflow counter is in
`/api/status`. Slow downloads/recording can still delay logging, camera frames
and radio queue draining. Monitor the counters during hardware testing.

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
one per MAC/protocol per ten seconds, except rule-strength upgrades.
New tier-2-or-higher evidence can sound an alert. Global mute suppresses sounds,
not logging. Capture limits and SD failures are exposed in the UI.

GPS fixes older than three seconds are rejected. Events delayed over three seconds
are logged without observer coordinates rather than attaching a later location.
IR CSV includes observed `uptime_ms` and `logged_uptime_ms`; its UTC field is
logging time. This makes a delayed queue drain visible. `uptime_ms` is time since
boot, not Unix time. Missing JSON coordinates are `null`. The observer's GPS
position must not be interpreted as the target's location. Drone positions are
decoded broadcasts, separately labeled, and can be spoofed.

USB commands at 115200 baud: `CMD:STATUS`, `CMD:VERSION`, `CMD:DUMP_LIVE`,
`CMD:CLEAR_LIVE`. Clear Live affects RAM only. Output uses a bounded write per
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
