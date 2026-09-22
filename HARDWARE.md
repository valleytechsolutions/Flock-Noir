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
| 3 | **GNSS / GPS module (NMEA, UART)** | 1 | ATGM336H, or **[Seeed L76K GNSS for XIAO](https://www.seeedstudio.com/L76K-GNSS-Module-for-Seeed-Studio-XIAO-p-5864.html)** | The current reference build uses **ATGM336H at 9600 baud** on D7/D6. L76K also defaults to 9600. A Quectel LC29H typically needs `GPS_BAUD 115200` in `config.h`. |
| 4 | **Passive piezo buzzer** | 1 | **[Seeed Grove Passive Buzzer](https://www.seeedstudio.com/Grove-Passive-Buzzer-p-4525.html)** or any 2-pin passive piezo | *Passive*, not active (see warning above). 3-5 V. |
| 5 | **2.4 GHz antenna** | 1 | *included with the XIAO ESP32-S3* | Snap onto the IPEX connector - needed for a stable Wi-Fi AP. |
| 6 | Hook-up wire + soldering iron | - | any | For the GPS and buzzer connections. |

### Optional / recommended

| Part | Where | Why |
|------|-------|-----|
| **OV2640 lens with IR-cut filter removed** (or a "no-IR-filter" OV2640 camera) | hobby electronics suppliers | Big boost to IR detection range/reliability. You can also carefully remove the tiny filter from a spare lens. |
| **LiPo battery (3.7 V)** | [Seeed batteries](https://www.seeedstudio.com/battery-c-262.html) | Portable, cable-free operation; the XIAO has an onboard charger. |
| **3D-printed case** | your own / community | Protect it for field/car use. |
| **OPT101 analog module** | [TI specifications](https://www.ti.com/product/OPT101) / electronics suppliers | Dedicated pulse timing input on D1; use a 3.3 V-compatible breakout. |

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
| **OPT101 OUT** | **2** | **D1** | ADC1, works alongside WiFi |
| Free header pins | 3,4,5,6 | D2-D5 | Check boot-strapping before attaching loads |

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
   [README install section](README.md#install-and-flash-xiao-esp32-s3-sense).
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

## OPT101 photodiode module (current reference build)

![OPT101 and GPS wiring](docs/wiring-opt101.svg)

Disconnect USB and battery power before connecting wires. This build uses an
**OPT101 analog breakout** and an **OV2640 with its IR-cut filter removed**.
OPT101 already contains the photodiode and amplifier; no BPW34 or MCP6002 is
required. It responds to visible and near-IR light, not exclusively 850 nm.

| OPT101 breakout terminal | XIAO ESP32-S3 Sense |
|---|---|
| VCC / VS / V+ | **3V3** |
| GND | **GND** |
| OUT / VOUT / analog output | **D1 / GPIO2 (ADC1_CH1)** |

Terminal order varies by breakout. Confirm labels and its supply requirements;
this connection is for a breakout that operates from 3.3 V. A board marked for
5 V only needs its circuit checked before use. Do not put a 5 V output into the
XIAO ADC. Start with short wires; put supply decoupling at the sensor if the
breakout does not include it. TI recommends 0.01-0.1 uF supply bypassing.

### Bare OPT101 chip, standard single-supply circuit

These are **IC pin numbers viewed from above**, not breakout terminal positions.
Use the package's pin-1 marker and the [TI datasheet](https://www.ti.com/lit/ds/symlink/opt101.pdf).

| IC pin | Connection |
|---|---|
| 1, VS | 3V3 |
| 3, -V | GND |
| 8, Common | GND |
| 4, feedback | Join to pin 5 to use the internal 1 Mohm resistor |
| 5, output | D1 / GPIO2 |
| 2, negative input | Leave unconnected in this standard circuit |
| 6 and 7 | No connection |

### Bring-up

1. Power on and connect to **Flock Noir**, password **flocknoir**. Open `192.168.4.1`.
2. Enable **IR photodiode sensor** in Detector. Watch the raw count and waveform
   as you cover/uncover the OPT101. The firmware cannot prove a sensor is
   connected simply by reading an ADC baseline.
3. Check for headroom in actual outdoor lighting. OPT101 is not rail-to-rail;
   its amplifier may saturate well below the ADC maximum. Lower incoming light
   or use an appropriate optical filter if the waveform flattens under bright light.
4. A TV remote is useful for checking response, but is not a valid ALPR test
   source. Verify a known 10 Hz, 20 ms light pulse and reject other frequencies.
5. Enable WiGLE and radio scanning, then verify the sampling-rate and drop/gap
   diagnostics while logging. See [detection and radio validation](docs/RADIO.md).

The supplied camera defaults now use lower fixed exposure/gain (150 / 2) for
an IR-cut-free OV2640. Tune those under real lighting. Point the camera and
OPT101 at the same area to make cross-sensor evidence more useful. A suitable
850 nm bandpass filter can reduce visible-light interference; verify the actual
filter transmission specification. No optics can establish camera identity.

### GPIO conflicts to avoid

The OUI Spy reference board is not the Sense peripheral layout. Keep the passive
buzzer on **D0/GPIO1**. **GPIO21 is microSD CS**, not an available NeoPixel pin.
GPS stays on **D7/GPIO44 RX** and **D6/GPIO43 TX**. For your ATGM336H, the
firmware uses the external UART profile at **9600 baud**. Keep its existing power
wiring if working; bare GNSS modules and breakouts can have different supply ratings.

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
