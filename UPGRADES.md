<div align="center">

<img src="docs/logo.png" alt="Flock Noir" width="110">

# Upgrades and Add-ons

*Low-cost parts to make Flock Noir more capable. Prices are rough ballparks in USD.*
See [HARDWARE.md](HARDWARE.md) for the required build.

</div>

---

## Tier 1: make the IR detection actually work

These directly improve the core job (seeing the ALPR IR flash). Do these first.

| Part | Approx cost | Why it matters |
|------|-------------|----------------|
| **OV2640 lens with the IR-cut filter removed** (or a spare lens you de-filter) | $3 - 8 | The single biggest win. A stock lens blocks most 850 nm light, so the camera barely sees the signal. Removing the IR-cut filter lets the sensor actually see 850 nm. |
| **850 nm IR photodiode or phototransistor** (dark / IR-pass lens type) | ~$0.30 each, or a 100-pack kit for ~$8 | A dedicated fast IR channel. Wired to a free ADC pin (D1-D5) and sampled at 1-2 kHz, it provides finer timing than the camera. The current reference build uses an OPT101 module; see HARDWARE.md. |
| **IR band filter for the photodiode** | $0 - 40 | Cheap option: a black-epoxy IR photodiode already blocks most visible light. Precise option: a narrow 850 nm bandpass optical filter (~$15-40) for the cleanest signal and fewest false positives. |

The firmware is structured so a photodiode ADC path can run alongside the camera and
record their evidence separately. The XIAO radio integration adds temporal correlation, not positive device identification.

## Tier 2: field usability

| Part | Approx cost | Why |
|------|-------------|-----|
| **LiPo battery, 3.7 V, 500-1000 mAh** | $5 - 8 | Cable-free operation. The XIAO has an onboard charger; just plug the cell into the battery pads. |
| **0.96" SSD1306 I2C OLED** | $3 - 5 | Standalone status (fix, detections, AP) without needing a phone. Wires to the I2C pins (D4/D5). |
| **WS2812 / NeoPixel status LED** | ~$0.30 | Color-coded state at a glance: green scanning, amber acquiring GPS, red alert. |
| **Momentary push button** | ~$0.10 | Manual "mark this spot" / arm / cycle modes in the field. |
| **Slide power switch** | ~$0.20 | Clean on/off for battery use. |
| **Printed or project enclosure** | $0 - 10 | Protect it for car / pocket use; a windowed slot keeps the lens clear. |

## Tier 3: GPS and timing

| Part | Approx cost | Why |
|------|-------------|-----|
| **Active GPS antenna** (if your module has a u.FL port) | $3 - 8 | Much faster, more reliable fix, especially inside a vehicle. |
| **DS3231 RTC module** | $2 - 4 | Accurate timestamps on logs even before a GPS fix (I2C, D4/D5). |
| **Larger 2.4 GHz u.FL antenna** | $2 - 5 | The XIAO S3 has the connector; a bigger antenna extends the web UI / wardrive range. |

## Rough budget

- Minimum "make detection real" (de-filtered lens + a photodiode): about **$5 - 15**.
- Nice portable build (battery + OLED + LED + case): another **$15 - 25**.

---

*Prices vary by supplier and quantity; buy from a source you trust and check datasheets
(some parts sold as "phototransistors" are actually IR LEDs).*
