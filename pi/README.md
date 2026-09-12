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
4. **Check it**: `systemctl status flocknoir` and `journalctl -u flocknoir -f`. While the
   Pi is still on your home Wi-Fi you can open the UI at `http://<pi-ip>/`.
5. **Turn on the field hotspot** (this is how you use it in the car):
   ```bash
   sudo ./pi/netmode.sh hotspot
   ```
   The Pi now broadcasts Wi-Fi **Flock Noir** (password **flocknoir**) at
   **192.168.4.1**. Join it from your phone and the Flock Noir page opens automatically
   (captive portal). To go back to your home Wi-Fi for updates: `sudo ./pi/netmode.sh client`.

**Single-radio note:** a Pi Zero 2 W has one Wi-Fi radio, so the hotspot and your home
Wi-Fi are exclusive. SSH keeps working over the hotspot: `ssh <user>@192.168.4.1`.

## Wiring

```
Passive piezo buzzer:   (+) -> GPIO18 (physical pin 12)     (-) -> GND (pin 6/9/14)
GPS module (UART):      GPS TX -> GPIO15 RXD (pin 10)      GPS RX -> GPIO14 TXD (pin 8)
                        VCC -> 3V3 (pin 1)                  GND -> GND
                        (then: sudo raspi-config -> Interface Options -> Serial Port:
                         login shell NO, serial hardware YES; or set GPS_PORT to a USB port)
MCP3008 (optional):     SPI0 (pins 19/21/23/24), photodiode front end into CH0
                        (enable SPI: sudo raspi-config -> Interface Options -> SPI)
```
The photodiode front-end circuit is in [HARDWARE.md](../HARDWARE.md) (transimpedance
BPW34 + MCP6002, or a simple phototransistor).

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
