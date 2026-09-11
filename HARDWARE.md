<div align="center">

<img src="docs/logo.png" alt="Valleytech Custom Solutions" width="110">

# Hardware - Parts, Wiring & Assembly

*Bill of materials and build guide for the **Flock Noir**.*
See the main **[README](README.md)** for what the project is and its important
**experimental disclaimer** (this tool is a research aid, **not** a definitive detector -
always visually confirm an actual camera).

</div>

---

##  Two things that trip everyone up

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
