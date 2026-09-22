<div align="center">

<img src="docs/logo.png" alt="Flock Noir" width="110">

# How ALPR IR works, and how Flock Noir detects it

*Research notes and the detection design. This is a research aid, not a definitive
detector - see the disclaimer in the [README](README.md).*

</div>

---

## How ALPR cameras use infrared

Automatic License Plate Reader (ALPR) cameras, including Flock Safety's, read plates day and
night. License plates are **retroreflective**: they bounce light straight back at its source.
So an ALPR camera puts a ring of **infrared (IR) LEDs around its lens** and flashes them; the
plate lights up brightly for the camera even in the dark, while the flash stays invisible to
the human eye.

The key facts that make this detectable:

- **Wavelength: ~850 nm** near-infrared. This is just past visible red. Standard camera
  sensors can see it, but consumer lenses ship with an **IR-cut filter** that blocks it.
- **It is pulsed, not continuous.** Reported timing is about **10 Hz, 20 ms on / 80 ms off
  (~20 % duty cycle)**.
- **It is motion-triggered.** The illuminator fires a **burst** when a vehicle or pedestrian
  passes - observers have measured **as many as ~40 pulses** during a single nighttime plate
  capture, then it goes quiet.

That pulsed 850 nm burst is a distinctive, machine-detectable signature. It is what Flock Noir
looks for.

## Why this is hard, and the sensor hierarchy

The catch is **time resolution**. To confirm a 20 ms pulse you want to sample well above
~100 Hz. Two sensors, two very different capabilities:

1. **Camera (OV2640) - weak for this.** At ~30-50 fps you get only ~4 samples per 100 ms
   cycle, so a 20 ms pulse lands in about one frame. Worse, a **stock lens blocks 850 nm**,
   and auto-exposure fights you. The camera can flag "there is ~10 Hz flicker over there" and
   tell you *where* it is, but it cannot prove the exact pulse shape.
2. **Analog photodiode - the right tool.** An 850 nm photodiode sampled at ~1 kHz captures the
   20 ms / 80 ms waveform with much finer timing resolution than the camera.
   Detection range and performance at speed need measurement on the actual build.

## Flock Noir's two-detector design

Flock Noir runs both and treats them as independent detectors that share one alert/log stream:

**Camera path** (`detector.cpp`): grayscale frames at fixed exposure -> per-frame brightness of
the brightest blob -> time-domain edge/period/duty scoring -> confidence. Good for spatial
"which object", and it works with no extra hardware. The live **Signal Scope** in the UI shows
this stream so you can see whether the sensor responds at all (a flat line while aimed at a
camera means the IR-cut lens is blocking 850 nm).

**OPT101 path** (XIAO 0.4): a dedicated ADC1 task targets 1 kHz and reports the
actual sample rate. It removes a slow ambient baseline, measures rising/falling
edges, and requires four consecutive cycles in the configured 8-12 Hz profile,
10-30% duty, 8-35 ms pulse width and no more than 15% period change. Invalid
cycles, clipping and sample gaps over 5 ms reset the train. Noise-adaptive
thresholds supplement the fixed threshold floor. These are initial tuning
values, not an ALPR identification standard.

Radio observations and optical events are independently logged. A nearby radio
candidate can add temporal evidence; neither an OUI nor pulse timing proves a
Flock camera. See [the radio/evidence guide](docs/RADIO.md) for operating modes,
signatures, capture limits and hardware validation. The Pi detector currently
retains its older implementation.

## Honing it in - practical tuning

- **Remove the IR-cut filter** (or use a de-filtered lens). Nothing else matters as much for
  the camera path.
- **Tune against measured sources.** The XIAO OPT101 default is 8-12 Hz; the
  motion-triggered burst is short; requiring several valid intervals in that band rejects
  random flicker.
- **Expect and filter false positives.** Sunlight off a modulated surface, other IR
  illuminators, and mains-lit scenes can all flicker. The duty check (~20 %), the compactness
  gate (camera), and the multi-interval requirement (photodiode) cut these down, and the logged
  confidence lets you filter later. **Always visually confirm a camera.**
- **What actually separates a strobe from noise.** Simulating the camera path from 37 to 90
  fps showed that "fraction of intervals near 100 ms" is a weak test at low frame rates (a
  20 ms pulse sometimes falls between frames, so even a real strobe only scores ~0.6). Two
  metrics separate cleanly at every frame rate: **duty cycle** (a strobe is short-on, ~0.2;
  symmetric noise crossing the threshold reads ~0.5) and **interval jitter** (a strobe repeats
  like a metronome, <= 0.09; noise that lands in the tolerance band is ragged, >= 0.19). Both
  detectors gate on these. If you retune, keep those two gates.

## References

- Flock IR pulse characteristics and passive photodiode detection:
  [Noflock/Flock-IR-Detection](https://github.com/Noflock/Flock-IR-Detection),
  [Unlisted-yea815/Flock-IR-Detection](https://github.com/Unlisted-yea815/Flock-IR-Detection)
- Teardown / IR illuminator observations:
  [Dissection of a Flock Safety Camera - CEHRP](https://www.cehrp.org/dissection-of-flock-safety-camera/)
- Mapping and avoiding ALPR cameras: [DeFlock](https://deflock.me),
  [State of Surveillance guide](https://stateofsurveillance.org/guides/basic/find-and-avoid-flock-cameras/)
- Signature-detection inspiration: Colonel Panic's
  [OUI Spy](https://github.com/colonelpanichacks/oui-spy)

*Not affiliated with Flock Safety. "Flock" is used generically for this category of ALPR camera.*
