<div align="center">

<img src="docs/logo.png" alt="Valleytech Custom Solutions" width="110">

# Hardware - Parts, Wiring & Assembly

*Bill of materials and build guide for the **Flock Noir**.*
See the main **[README](README.md)** for what the project is and its important
**experimental disclaimer** (this tool is a research aid, **not** a definitive detector -
always visually confirm an actual camera).

</div>

---

## Two things that trip everyone up

1. **The buzzer must be a *passive* piezo.** Active buzzers have a built-in oscillator and
   play only one fixed pitch - they **cannot** play the RTTTL tunes. Get a *passive* one.
2. **The IR-cut filter is the gatekeeper.** A stock camera lens blocks most 850 nm light, so
   IR detection range is short and finicky. An **IR-filter-removed lens** dramatically
   improves it (see [optional parts](#optional--recommended)).

---

## Bill of materials

### Required

| # | Part | Qty | Where to buy | Notes |
|---|------|-----|--------------|-------|
| 1 | **Seeed Studio XIAO ESP32-S3 Sense** | 1 | [seeedstudio.com](https://www.seeedstudio.com/XIAO-ESP32S3-Sense-p-5639.html) - [pre-soldered](https://www.seeedstudio.com/Seeed-Studio-XIAO-ESP32S3-Sense-Pre-Soldered-p-6335.html) | The whole brain. Ships with the **OV2640 camera**, **microSD slot**, **PDM mic**, PSRAM, and a **2.4 GHz antenna**. Get the *Sense* version - the plain XIAO S3 has no camera/SD. |
| 2 | **microSD card** | 1 | any | **Format FAT32.** 4-32 GB is plenty. Holds the detection CSV, wardrive CSV, and recordings. |
| 3 | **GNSS / GPS module (NMEA, UART)** | 1 | **[Seeed L76K GNSS for XIAO](https://www.seeedstudio.com/L76K-GNSS-Module-for-Seeed-Studio-XIAO-p-5864.html)** | Any NMEA GNSS over UART works. The reference build used a **Quectel LC29H** (default **115200** baud). The Seeed **L76K** defaults to **9600** baud - if you use it, set `GPS_BAUD 9600` in `config.h`. |
| 4 | **Passive piezo buzzer** | 1 | **[Seeed Grove Passive Buzzer](https://www.seeedstudio.com/Grove-Passive-Buzzer-p-4525.html)** or any 2-pin passive piezo | *Passive*, not active (see warning above). 3-5 V. |
| 5 | **2.4 GHz antenna** | 1 | *included with the XIAO ESP32-S3* | Snap onto the IPEX connector - needed for a stable Wi-Fi AP. |
| 6 | Hook-up wire + soldering iron | - | any | For the GPS and buzzer connections. |

### Optional / recommended

| Part | Where | Why |
|------|-------|-----|
| **OV2640 lens with IR-cut filter removed** (or a "no-IR-filter" OV2640 camera) | hobby electronics suppliers | Big boost to IR detection range/reliability. You can also carefully remove the tiny filter from a spare lens. |
| **LiPo battery (3.7 V)** | [Seeed batteries](https://www.seeedstudio.com/battery-c-262.html) | Portable, cable-free operation; the XIAO has an onboard charger. |
| **3D-printed case** | your own / community | Protect it for field/car use. |
| **850 nm photodiode + 850 nm band-pass filter** | electronics suppliers | For the planned high-accuracy timing front-end (see README roadmap). |

> **Cost:** the required electronics come to roughly **$25-40** depending on the GPS module.

---

## Pin map

Everything below is fixed by the Sense board **except** the GPS and buzzer, which use free
header pins. None of these overlap, so the camera + SD keep working.

| Signal | GPIO | Header pad | Owner |
|--------|------|-----------|-------|
| Camera OV2640 (14 pins) | 10,11,12,13,14,15,16,17,18,38,39,40,47,48 | ribbon | Sense board (fixed) |
| microSD - CS | **21** | - | Sense board (`SD.begin(21)`) |
| microSD - SCK / MISO / MOSI | 7 / 8 / 9 | D8 / D9 / D10 | Sense board (default SPI) |
| PDM microphone (recording) | 42 (clk) / 41 (data) | - | Sense board (fixed) |
| **GPS RX** (<- module **TX**) | **44** | **D7** | `config.h` |
| **GPS TX** (-> module **RX**) | **43** | **D6** | `config.h` |
| **Buzzer signal** | **1** | **D0** | `config.h` |
| *free for future photodiode* | 2,3,4,5,6 | D1-D5 | - |

---

## Wiring

### GPS (NMEA GNSS over UART)

```
GPS module            XIAO ESP32-S3 Sense
-----------           -------------------
  TX      ----------->  D7  (GPIO44)   [ESP RX]
  RX      -----------  D6  (GPIO43)   [ESP TX]
  VCC     ----------->  3V3
  GND     ----------->  GND
```
*If your module uses different pads, just change `GPS_RX_PIN` / `GPS_TX_PIN` / `GPS_BAUD` in
`config.h`.*

### Buzzer (passive piezo, 2-pin)

```
buzzer leg 1 (+)  ------->  D0  (GPIO1)     (optional ~100 ohm in series to soften volume)
buzzer leg 2 (-)  ------->  GND
```
*Passive piezos aren't polarized - if the legs are equal length, either orientation is fine.*
On power-up the board plays a **boot jingle**, which confirms the buzzer works.

---

## Assembly

1. **Attach the antenna** to the XIAO's IPEX connector and seat the camera ribbon.
2. **Format the microSD** as FAT32 and insert it into the Sense board's slot.
3. **Solder the GPS** (4 wires: TX->D7, RX->D6, 3V3, GND).
4. **Solder the buzzer** (signal->D0, other leg->GND).
5. *(Optional)* swap in the IR-filter-removed lens; *(optional)* connect a LiPo battery.
6. **Flash** the firmware - prebuilt image or from source, see the
   [README install section](README.md#install--flash).
7. Power on: you'll hear the boot jingle, then a Wi-Fi AP appears. Connect and the UI opens.

---

## First-light test (no camera target needed)

- **Buzzer:** hear the boot jingle? Passive buzzer wired correctly. In **Settings ->  Test**,
  a tune that *changes pitch* confirms it's passive.
- **SD:** the UI's microSD tile should read **ready**.
- **GPS:** take it outside; the **GPS Fix** tile flips to **LOCK** once it sees satellites
  (cold start can take a few minutes).
- **IR pipeline:** point a **TV/AC remote** at the lens and press buttons - remotes pulse IR
  (at a different rate), a quick way to confirm the camera + detector chain is alive. Real
  detection still requires the correct signature **and your visual confirmation.**

---

## IR photodiode sensor (high-accuracy detection)

The camera samples at ~40 fps, which is too slow to prove a 20 ms pulse. An analog **850 nm
photodiode** sampled at ~1 kHz captures the exact 10 Hz / 20 % pulse cleanly. This is the
method proven by the open-source **[Noflock/Flock-IR-Detection](https://github.com/Noflock/Flock-IR-Detection)**
project (reliable at highway speed), and Flock Noir implements the same approach. The
firmware samples an ADC pin on a dedicated 1 kHz task, tracks an ambient baseline, and
validates rising-edge intervals against the 5-15 Hz band.

**ADC pin:** the photodiode signal goes to **D1 / GPIO2** (an ADC1 channel, which works with
Wi-Fi on). Set `IR_SENSOR_PIN` in `config.h` to move it. Enable the sensor from the
**Detector** tab (IR photodiode toggle) once it is wired.

### Option A - transimpedance amplifier (best, what Noflock uses)

```
                    4.7 Mohm
              +-----/\/\/-----+
              |               |
              |     10 pF     |
              +----||---------+
              |               |
 photodiode   |   |\          |
 (BPW34,      |   | \         |
  cathode +)  +---|- \        |
      |           |   >-------+------> D1 / GPIO2 (ADC)
      |       +---|+ /
     GND      |   | /
              |   |/  MCP6002 (rail-to-rail, 3.3V)
             3V3
```
- Photodiode: **BPW34** (or BPW34NA, IR-enhanced), cathode to the op-amp input.
- Op-amp: **MCP6002** (dual, cheap, rail-to-rail). Feedback **4.7 Mohm** + **10 pF**.
- Output sits near ~0.2 V in the dark and swings toward 3.3 V on a strong IR pulse.
- Put an **IR-pass filter** over the diode (mylar, or a strip of exposed/developed film
  negative) to block visible light and cut false positives.

### Option B - phototransistor (simplest, no op-amp)

```
 3V3 ---- collector [phototransistor] emitter ----+---- D1 / GPIO2 (ADC)
                                                   |
                                                 [10k] load resistor
                                                   |
                                                  GND
```
- More IR light -> more current -> higher voltage at D1. Cheaper and fewer parts, but less
  sensitive and slower than the transimpedance design.

### Multi-sensor

Noflock uses three sensors (rear + roof) for coverage at speed. The firmware currently reads
one ADC pin; adding more channels (D2/GPIO3, D3/GPIO4 - also ADC1) is a small change and is
on the roadmap.

*Credit: the photodiode circuit and the edge/period detection approach follow
[Noflock/Flock-IR-Detection](https://github.com/Noflock/Flock-IR-Detection) and the broader
Flock-IR-detection community.*

---

## Raspberry Pi wiring (the `pi/` target)

The Raspberry Pi build uses the same sensors on the Pi's 40-pin header:

| Signal | Pi header pin | BCM |
|--------|---------------|-----|
| Buzzer (+) | 12 | GPIO18 (PWM) |
| Buzzer (-) | 6 | GND |
| GPS TX -> Pi | 10 | GPIO15 RXD |
| GPS RX <- Pi | 8 | GPIO14 TXD |
| MCP3008 CLK / DOUT / DIN / CS | 23 / 21 / 19 / 24 | GPIO11 / 9 / 10 / 8 (SPI0) |
| IR receiver front-end out | MCP3008 CH0 (CH1, CH2 for more) | - |

The Pi has no analog input, so the IR photodiode front ends above feed an **MCP3008**
ADC on SPI. The full pin-by-pin guide, header diagram, and placement notes are in
**[pi/README.md](pi/README.md#wiring)**.
