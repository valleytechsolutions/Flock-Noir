# Optical ALPR evidence — XIAO 0.6.0

Flock Noir measures light changes and combines them with nearby ALPR radio
candidates. Its current **8–12 Hz** profile is a test configuration. This project
has no verified basis for claiming that every ALPR, or every Flock camera, emits
10 Hz / 20 ms flashes. Illumination and exposure depend on camera design and
configuration; see [Axis's plate-capture guide](https://whitepapers.axis.com/en-us/license-plate-capture).
A matching waveform is supporting evidence, never a camera identity check.

## What the sensors measure

**OPT101** integrates a photodiode and amplifier. Powered from a compatible 3.3 V
breakout, OUT feeds **D1 / GPIO2, ADC1**. The device responds to visible and
near-infrared light; it cannot measure wavelength. Sampling at ~1000 samples/s
permits millisecond pulse timing. The firmware removes a slowly changing
baseline, uses a noise-dependent threshold with hysteresis, and requires:

| Measurement | Current reference profile |
|---|---|
| Frequency | 8–12 Hz |
| Duty | 10–30% |
| Width | 8–35 ms |
| Consecutive valid intervals | At least 4 |
| Interval regularity | Within 15% of the preceding valid interval |
| Sampling gaps | Gaps above 5 ms reset the match |
| Freshness | Match clears after 300 ms without continuing valid pulses |

These constants are in `firmware/FlockNoir/config.h`. Profiles outside these
bounds can be missed, including continuous illumination. Calibrating a different
profile requires known-source measurements and new negative controls, not an
assumption that a new frequency identifies a manufacturer.

**OV2640** supplies spatial brightness evidence. It captures 640×480 JPEG at
fixed exposure/gain with two PSRAM buffers. A separate task extracts 80×60 block
brightness, bright-area fraction and centroid, retaining the full-quality JPEG
for preview. The camera searches for localized repeated brightness changes;
whole-frame flicker and very weak changes are rejected.

At the reference ~25 fps, frames are ~40 ms apart. A 20 ms pulse can fall between
frames. In particular a 10 Hz source may appear to blink at **5 Hz**. The detector
keeps this as an explicitly **aliased candidate**, halves its confidence score
and records the **observed** frequency. It does not invent an exact pulse width
or call the observed rate 10 Hz. The CSV field `camera_timing` distinguishes
`aliased_candidate` from `frame_limited_pattern`. Even the latter has limited
timing precision. Use OPT101 for the direct timing measurement.

## Combined evidence

Only **ALPR mode** runs both optical paths, together with Flock-You WiFi/OUI/probe
and BLE rules. Each source has its own timestamp; fresh evidence within 3 seconds
can produce `ir+camera+ble+wifi` or a subset in the ALPR CSV. Correlation works in
either arrival order. Shared vendor prefixes remain weak hints; a nearby radio
and a light source are not assumed to be the same object.

Pig Detector, Wardrive and General Scanner pause optical analysis. Their mode
switches clear stale optical results so a previous signal cannot trigger a new
mode's buzzer. Camera preview is a separate view, not an implicit mode change.

## Bench validation before field interpretation

1. Use the [wiring guide](HARDWARE.md). Select **ALPR** and enable OPT101 only
   after wiring. Cover/uncover the sensor: raw counts and waveform should change.
   ADC values alone cannot establish a physical connection.
2. Check the reported OPT101 sample rate (~1 kHz), camera analysis rate (~25 fps),
   clipping and sample-gap counters under actual lighting. Saturated OPT101
   output can be below ADC full-scale: a flattened waveform still needs attention.
3. Aim OPT101 and camera at a separate, measured **10 Hz / 20 ms light source**.
   OPT101 should report about 10 Hz, 20% duty and 20 ms width after four valid
   intervals. A 25 fps camera can legitimately report a 5 Hz alias candidate.
4. Negative controls: cover the sensors, constant light, 50/60 Hz light, 20 Hz
   pulses, very short / long pulse widths, irregular pulses and saturation.
   These must not produce an OPT101 timing match. A TV remote checks sensitivity;
   it is not a valid substitute for the reference waveform.
5. Inspect ALPR CSV source/method and pulse fields. Add a known radio candidate
   only when testing correlation; observe that a camera-alone event does not
   invent a MAC or radio tier. Missing SD/GPS must remain visible.
6. Switch to Pig Detector and Wardrive. OPT101 sample rate / camera analysis
   should pause, the prior alert should clear, and ALPR logging should stop.
   Restore ALPR and verify both optical paths resume without rewiring GPS.

Synthetic tests cover waveform timing and all phases of the 25 fps alias example.
Hardware smoke checks measure execution rates and mode isolation. Neither proves
range, sensitivity or false-positive rates against real ALPRs. Measure and record
actual target waveforms before making manufacturer-specific claims.

## Field setup

Use an OV2640 without its IR-cut filter. Fix focus and aim the camera and OPT101
at the same scene. Optical shade or a characterized filter can reduce ambient
light; neither identifies the source. Use the built-in fixed exposure/gain as a
starting point, then evaluate waveform headroom outdoors. Nearby light may come
from ordinary security cameras, traffic hardware or other emitters.

[Radio rules and CSV schema](docs/RADIO.md) · [Credits](ATTRIBUTIONS.md)
