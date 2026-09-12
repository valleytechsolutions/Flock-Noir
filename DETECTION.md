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
   20 ms / 80 ms waveform cleanly and unambiguously. Community projects (see references) show
   this reliably detects Flock captures from a stopped car up to highway speed.

## Flock Noir's two-detector design

Flock Noir runs both and treats them as independent detectors that share one alert/log stream:

**Camera path** (`detector.cpp`): grayscale frames at fixed exposure -> per-frame brightness of
the brightest blob -> time-domain edge/period/duty scoring -> confidence. Good for spatial
"which object", and it works with no extra hardware. The live **Signal Scope** in the UI shows
this stream so you can see whether the sensor responds at all (a flat line while aimed at a
camera means the IR-cut lens is blocking 850 nm).

**IR photodiode path** (`irsensor.cpp`): a dedicated 1 kHz ADC task with an exponential-moving-
average ambient baseline, AC (pulse) extraction, hysteresis edge detection with a refractory
gap, and validation that rising-edge intervals fall in the **5-15 Hz** band for several
consecutive pulses before asserting a detection. This mirrors the proven
[Noflock/Flock-IR-Detection](https://github.com/Noflock/Flock-IR-Detection) algorithm and is
the high-accuracy path. See [HARDWARE.md](HARDWARE.md) for the circuit.

Every hit is written to the CSV with a **source** column (`camera` or `ir`) plus its frequency,
duty, and confidence, so real hits can be told from noise after the fact.

## Honing it in - practical tuning

- **Remove the IR-cut filter** (or use a de-filtered lens). Nothing else matters as much for
  the camera path.
- **Match the band, not just 10 Hz.** We accept ~5-15 Hz because real-world rate varies and the
  motion-triggered burst is short; requiring several valid intervals in that band rejects
  random flicker.
- **Expect and filter false positives.** Sunlight off a modulated surface, other IR
  illuminators, and mains-lit scenes can all flicker. The duty check (~20 %), the compactness
  gate (camera), and the multi-interval requirement (photodiode) cut these down, and the logged
  confidence lets you filter later. **Always visually confirm a camera.**

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
