<div align="center">

<img src="../docs/logo.png" alt="Flock Noir" width="110">

# Flock Noir on Raspberry Pi

*The Raspberry Pi hardware target. Same web UI, same features, same logs as the XIAO
ESP32-S3 build. Works on any Raspberry Pi with Wi-Fi and a CSI camera; the reference
build is a **Pi Zero 2 W + Camera Module NoIR v2**.*

</div>

> Experimental. This is a research aid, not a definitive detector. Always visually confirm
> an actual camera. See the [main README](../README.md) for the full disclaimer.

---

## Why the Pi is a strong platform for this

- **The NoIR camera has no IR-cut filter**, so it sees 850 nm natively. That is exactly the
  limitation holding back a stock-lens XIAO build.
- The IMX219 sensor runs **640x480 at up to 90 fps**, more than double the XIAO's frame
  rate, so the 10 Hz / 20 ms pulse is sampled far better.
- Plenty of CPU and storage: recordings use the **hardware H.264 encoder**, logs go on the
  Pi's own microSD, and the whole thing runs as a normal Linux service.

## What you need

| Part | Notes |
|------|-------|
| Raspberry Pi with Wi-Fi + CSI connector | Zero 2 W (reference), 3, 4, 5, Zero W all work. |
| Raspberry Pi Camera Module **NoIR** (v2 or v3) | The "NoIR" is the one without the IR-cut filter. Standard v2/v3 also works but sees far less IR. |
| microSD card (8 GB+) with Raspberry Pi OS | **Bookworm (2023-10) or newer**, Lite is fine. Older Bullseye needs the legacy camera stack and is not supported here. |
| Passive piezo buzzer (optional) | Signal to **GPIO18**, other leg to **GND**. Passive, not active. |
| GPS module (optional) | NMEA over UART (GPIO14/15 = /dev/serial0) or USB (/dev/ttyUSB0). |
| MCP3008 ADC + IR photodiode (optional) | The Pi has no ADC; an MCP3008 on SPI adds the high-accuracy IR photodiode path. |
| Power | 5 V 2 A supply, or a USB power bank for the car. |

## Install

### Option A: flash the ready-made image (easiest)

1. Download `flocknoir-pi.img.xz` from the
   [Releases page](https://github.com/valleytechsolutions/Flock-Noir/releases).
2. Open **Raspberry Pi Imager**, choose your Pi, then **Use custom** and pick the file.
   (You can skip Imager's customization; the image is already set up.)
3. Flash, insert the card with the camera ribbon attached, and power up.
4. On your phone, join the Wi-Fi **Flock Noir** (password **flocknoir**). The Flock Noir
   page opens automatically; if not, browse to `http://192.168.4.1`.

The image boots straight into field (hotspot) mode with SSH on, SPI and the UART already
enabled, and a default login of **flock / flocknoir**. Change that password after the
first boot (`ssh flock@192.168.4.1`, then `passwd`). Details on what is inside the image
and how to put the Pi on your home Wi-Fi for updates are in [image/README.md](image/README.md).

### Option B: install on an existing Raspberry Pi OS

1. **Flash Raspberry Pi OS** (Bookworm or newer, Lite recommended) with Raspberry Pi
   Imager. In Imager's settings enable **SSH** and enter your **home Wi-Fi** so you can
   reach the Pi for setup. Insert the card and boot with the camera ribbon attached.
2. **SSH in** and confirm the camera is seen:
   ```bash
   rpicam-hello --list-cameras
   ```
   (On slightly older Bookworm images the command is `libcamera-hello --list-cameras`.)
3. **Clone and install**:
   ```bash
   git clone https://github.com/valleytechsolutions/Flock-Noir.git
   cd Flock-Noir
   sudo ./pi/install.sh
   ```
   This installs the apt/pip dependencies, creates `/var/lib/flocknoir` for logs and
   recordings, and installs + starts the `flocknoir` systemd service (so it runs on
   every boot).
4. **Enable the interfaces you use** (one time, then reboot): `sudo raspi-config` ->
   Interface Options -> **SPI: Yes** (for the MCP3008 IR receivers) and **Serial Port:**
   login shell No, hardware Yes (for a UART GPS). The camera needs nothing on Bookworm.
5. **Check it**: `systemctl status flocknoir` and `journalctl -u flocknoir -f`. While the
   Pi is still on your home Wi-Fi you can open the UI at `http://<pi-ip>/`.
6. **Turn on the field hotspot** (this is how you use it in the car):
   ```bash
   sudo ./pi/netmode.sh hotspot
   ```
   The Pi now broadcasts Wi-Fi **Flock Noir** (password **flocknoir**) at
   **192.168.4.1**. Join it from your phone and the Flock Noir page opens automatically
   (captive portal). To go back to your home Wi-Fi for updates: `sudo ./pi/netmode.sh client`.

**Single-radio note:** a Pi Zero 2 W has one Wi-Fi radio, so the hotspot and your home
Wi-Fi are exclusive. SSH keeps working over the hotspot: `ssh <user>@192.168.4.1`.

## Wiring

All connections use the Pi's 40-pin header. Numbers below are **physical pin numbers**
(the position on the header), with the BCM GPIO name in parentheses. Looking at the Pi
with the header at the top-right and the SD card slot facing away from you, pin 1 is the
top-left pin (nearest the SD card) and pins count 1-2 across, 3-4, and so on down.

```
        3V3  (1)  (2)  5V
  SDA GPIO2  (3)  (4)  5V
  SCL GPIO3  (5)  (6)  GND
      GPIO4  (7)  (8)  GPIO14 TXD  ---> GPS RX
        GND  (9)  (10) GPIO15 RXD  <--- GPS TX
     GPIO17 (11)  (12) GPIO18 PWM  ---> buzzer (+)
     GPIO27 (13)  (14) GND
     GPIO22 (15)  (16) GPIO23
        3V3 (17)  (18) GPIO24
 MOSI GPIO10 (19) (20) GND
 MISO GPIO9  (21) (22) GPIO25
 SCLK GPIO11 (23) (24) GPIO8 CE0   ---> MCP3008 CS
        GND (25)  (26) GPIO7 CE1
       ...  (27-40 unused by Flock Noir)
```

### Buzzer (passive piezo, 2-pin)

```
buzzer (+)  ------>  pin 12  (GPIO18, hardware PWM)
buzzer (-)  ------>  pin 6   (GND)          (optional ~100 ohm in series on the + leg)
```
Passive, not active: active buzzers make one fixed pitch and cannot play the RTTTL
tunes. If the legs are equal length, either orientation is fine. You will hear the boot
jingle on power-up when it is wired correctly. Change `BUZZER_PIN` in `config.py` to use
another GPIO.

### GPS (NMEA over the Pi UART)

```
GPS TX   ------>  pin 10  (GPIO15 RXD)
GPS RX   ------>  pin 8   (GPIO14 TXD)
GPS VCC  ------>  pin 1   (3V3)
GPS GND  ------>  pin 9   (GND)
```
Then free the UART from the login console: `sudo raspi-config` -> Interface Options ->
Serial Port -> login shell **No**, serial hardware **Yes**, reboot. The device is
`/dev/serial0` (the default `GPS_PORT`). A USB GPS needs no wiring: set `GPS_PORT` to
`/dev/ttyUSB0` or `/dev/ttyACM0`. Default baud is 9600 (ATGM336H, L76K); an LC29H is 115200.

### IR receivers (photodiodes) via an MCP3008 ADC

The Pi has no analog input, so the IR photodiode path needs an ADC. The **MCP3008**
(8-channel, 10-bit, SPI, about 3 dollars) is the standard choice and gpiozero supports
it directly. Wire the chip to SPI0:

```
MCP3008 pin  ->  Pi header
  16 VDD     ->  pin 1   (3V3)
  15 VREF    ->  pin 17  (3V3)
  14 AGND    ->  pin 25  (GND)
  13 CLK     ->  pin 23  (GPIO11 SCLK)
  12 DOUT    ->  pin 21  (GPIO9  MISO)
  11 DIN     ->  pin 19  (GPIO10 MOSI)
  10 CS/SHDN ->  pin 24  (GPIO8  CE0)
   9 DGND    ->  pin 20  (GND)
   1 CH0     <-  IR sensor front-end output   (IR_ADC_CHANNEL = 0)
   2 CH1     <-  (second sensor, e.g. rear)
   3 CH2     <-  (third sensor, e.g. roof)
```
Enable SPI once: `sudo raspi-config` -> Interface Options -> SPI -> Yes.

Each IR receiver is one of these two front ends (same circuits as the XIAO, see
[HARDWARE.md](../HARDWARE.md)), with its output going to an MCP3008 channel instead of
an ESP32 pin:

```
Option A - transimpedance amp (best):        Option B - phototransistor (simplest):

   BPW34 photodiode + MCP6002 op-amp,           3V3 --- collector [phototransistor]
   4.7 M feedback, 10 pF, from 3V3.                     emitter ---+---> MCP3008 CHn
   Output (~0.2 V dark, toward 3.3 V on IR)             [10k]     |
   ---> MCP3008 CHn                                     GND ------+
```
Put an IR-pass filter (mylar, or a strip of exposed and developed film negative) over
each diode to block visible light and cut false positives.

**Placement:** point one receiver forward through the windshield and, if you fit more,
one to the rear and one on the roof, the way the Noflock project does for coverage at
speed. Keep the camera lens and the diodes unobstructed. Flip the **IR photodiode**
toggle on in the Detector tab once wired; the firmware reads `IR_ADC_CHANNEL` (CH0) and
multi-channel reading is on the roadmap.

## Configuration

Everything is in [`pi/config.py`](config.py): camera size/fps/exposure/gain, detection
sensitivity (same knobs as the XIAO `config.h`), buzzer pin and tunes, GPS port/baud,
wardriver interface, IR ADC channel. After editing: `sudo systemctl restart flocknoir`.

## Using it

Same four tabs as the XIAO build: **Detector** (alert banner, live signal scope, IR sensor
panel, recent hits, CSV), **Camera** (live near-IR view, record), **Wardrive** (Wi-Fi to
WiGLE CSV), **Settings** (buzzer tones). A smoother live view is also available at
`http://192.168.4.1/stream.mjpg`.

Data lives in `/var/lib/flocknoir/`: `logs/flock_*.csv` (IR hits, same columns as the
XIAO), `wardrive/wigle_*.csv`, `videos/rec_*.mp4` (or `.h264` if ffmpeg is absent).

## Differences from the XIAO build

| | XIAO ESP32-S3 Sense | Raspberry Pi |
|---|---|---|
| Camera | OV2640, IR-cut filter on stock lens, ~40 fps | NoIR, no IR-cut filter, up to 90 fps |
| Recording | MJPEG AVI + WAV sidecar | Hardware H.264 mp4; audio muxed in if a USB mic + ffmpeg are present |
| IR photodiode | Built-in ADC (D1) | Needs an MCP3008 on SPI |
| Wi-Fi | AP + STA scan (single radio) | NetworkManager hotspot; scans pause while a phone is connected (single radio) |
| Settings | NVS | `/var/lib/flocknoir/settings.json` |

## Troubleshooting

- **Camera "NOT AVAILABLE"** in the log: reseat the ribbon (contacts toward the board),
  run `rpicam-hello --list-cameras`, and make sure `python3-picamera2` installed.
- **Low fps**: lower `CAM_FPS` or `CAM_SIZE` in `config.py`; a Zero 2 W is happiest around
  60 fps at 640x480.
- **No sound**: passive buzzer on GPIO18? Test a tone from the Settings tab.
- **GPS "NO DATA"**: nothing arriving on `GPS_PORT`; check wiring/baud and that the serial
  login shell is disabled (`raspi-config`).
- **Service logs**: `journalctl -u flocknoir -f`.
