<div align="center">

<img src="../docs/logo.png" alt="Flock Noir" width="110">

# Flock Noir on Raspberry Pi

*Flock Noir 0.4.3 for **Pi Zero 2 W + Camera Module NoIR v2**, sharing the XIAO
web design, native OPT101 pulse validator and radio signature parsers. Pi camera,
SPI and radio operation still require physical validation on your hardware.*

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
| Raspberry Pi Zero 2 W | Reference for this arm64 image. Other Pi models have not been validated; the original Zero W cannot run this 64-bit image. |
| Raspberry Pi Camera Module **NoIR** (v2 or v3) | The "NoIR" is the one without the IR-cut filter. Standard v2/v3 also works but sees far less IR. |
| microSD card (8 GB+) with Raspberry Pi OS | **Bookworm (2023-10) or newer**, Lite is fine. Older Bullseye needs the legacy camera stack and is not supported here. |
| Passive piezo buzzer (optional) | Signal to **GPIO18**, other leg to **GND**. Passive, not active. |
| GPS module (optional) | NMEA over UART (GPIO14/15 = /dev/serial0) or USB (/dev/ttyUSB0). |
| MCP3008 ADC + IR photodiode (optional) | The Pi has no ADC; an MCP3008 on SPI adds the high-accuracy IR photodiode path. |
| OPT101 module (optional) | Power from 3V3; OUT goes to MCP3008 CH0, never directly to a Pi GPIO. |
| Monitor-capable USB Wi-Fi adapter (for packet capture) | Separate physical radio from the onboard hotspot; set `RADIO_MONITOR_IFACE`. An OTG adapter may be needed on Zero 2 W. |
| Power | 5 V 2 A supply, or a USB power bank for the car. |

## Install

### Option A: flash the ready-made image (easiest)

1. Download `flocknoir-pi.img.xz` from the
   [Releases page](https://github.com/valleytechsolutions/Flock-Noir/releases).
2. Choose the **Pi 0.4.3** release and verify its checksum.
   Open **Raspberry Pi Imager**, choose your Pi, then **Use custom** and pick the file.
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

**OPT101 module wiring (recommended):** module VCC/V+ -> Pi 3V3 (physical pin 1),
GND -> Pi GND (physical pin 6), OUT -> MCP3008 CH0 (ADC chip pin 1).
MCP3008 VDD and VREF must both be 3.3 V as shown above. The OPT101 already
contains its photodiode and amplifier. Match your module's terminal labels;
breakout terminal order varies. Leave **IR photodiode** off until connected.
Use an IR-pass optical filter and shield the sensor from direct sunlight.

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
| Camera | OV2640; VGA capture/analysis about 25 fps on 0.4.3; preview is slower | NoIR; configured for 60 fps, hardware rate must be measured |
| Recording | MJPEG AVI + WAV sidecar | Hardware H.264 mp4; audio muxed in if a USB mic + ffmpeg are present |
| IR photodiode | Built-in ADC (D1) | Needs an MCP3008 on SPI |
| Wi-Fi | AP + STA scan (single radio) | NetworkManager hotspot; scans pause while a phone is connected (single radio) |
| Settings | NVS | `/var/lib/flocknoir/settings.json` |

## Troubleshooting

- **Camera "NOT AVAILABLE"** in the log: reseat the ribbon (contacts toward the board),
  run `rpicam-hello --list-cameras`, and make sure `python3-picamera2` installed.
- **Low fps**: lower `CAM_FPS` or `CAM_SIZE` in `config.py`. The 60 fps setting is a
  target; measure actual frame rate and ADC gaps with your capture/recording load.
- **No sound**: passive buzzer on GPIO18? Test a tone from the Settings tab.
- **GPS "NO DATA"**: nothing arriving on `GPS_PORT`; check wiring/baud and that the serial
  login shell is disabled (`raspi-config`).
- **Service logs**: `journalctl -u flocknoir -f`.

## Radio and OPT101 update (0.4.3)

The Pi now uses the same native C++ pulse and radio parsers as XIAO, compiled by
the installer. It adds strict pulse-width/duty/regularity checks, clipping and
sampling-gap rejection, separate latched IR events, fresh GPS checks, WiFi/BLE
watchlists, target RSSI tones, drone Remote ID and bounded capture files. The
four-tab design stays the same. Full signature provenance and limitations are
in [RADIO.md](../docs/RADIO.md); please support
[Colonel Panic](https://colonelpanic.tech/) and
[OUI Spy Unified Blue](https://github.com/colonelpanichacks/oui-spy-unified-blue).

Version 0.4.3 surfaces radio-only detections on the Detector tab, includes the
additional Flock-You serial/DFU hints, Flipper BLE and Pineapple SSID candidates,
and plays Mario for new device encounters, including Axon and Meta. Repeated
packets stay quiet, mute is respected, and logs retain the evidence tier.
These radio rules do not require the IR toggle or a GPS fix to detect/log.

### Pi Zero 2 W radio setup

- Onboard WiFi: passive AP surveys when wardriving is enabled and the hotspot
  has no clients. OUI/SSID matches are labeled `survey_oui` / `survey_ssid`.
  Survey results are never written as fabricated packets in PCAP captures.
- Onboard Bluetooth: legacy BLE advertising through BlueZ/HCI (`hci0`), enabled
  by default. Extended/coded-PHY advertising is not implemented. Controller
  rejection, absence and disconnects are reported and retried.
- Packet capture: attach a USB adapter with Linux monitor-mode support, run
  `iw dev`, and set `RADIO_MONITOR_IFACE = "wlan1"` (use your actual interface)
  in `pi/config.py`. Keep `AP_IFACE` and `WD_IFACE` on `wlan0`, then restart
  `flocknoir`. The configured USB interface is made unmanaged by NetworkManager
  and switched to monitor mode. The application refuses to use the hotspot's
  physical radio for this purpose. A stock onboard-only setup cannot provide
  the promiscuous WiFi packet features.

On Pi, **Field** hops the separate monitor adapter across channels 1-11;
**Dashboard** fixes that adapter to the selected channel. The hotspot stays on
in both modes. Change back using the web UI; Pi has no XIAO BOOT-button action.
To return a dedicated adapter to ordinary networking, stop the service and run
`sudo ip link set wlan1 down`, `sudo iw dev wlan1 set type managed`,
`sudo ip link set wlan1 up`, then `sudo nmcli device set wlan1 managed yes`.

Captures and candidate logs live in `/var/lib/flocknoir/radio`, accessible from
the Wardrive tab. WiFi PCAP uses raw 802.11 link type 105 with FCS removed; BLE
captures are JSONL advertisement bytes. Each capture file stops at 16 MiB.
Monotonic timestamps describe observation time; UTC is logging time. IR/radio
co-occurrence within three seconds is supporting evidence, not camera identity.

### Update an existing Pi install

```bash
cd /opt/flocknoir
git pull --ff-only
sudo ./pi/install.sh
sudo systemctl restart flocknoir
```

Use your clone directory if it differs. The installer rebuilds the shared parser
for your Pi architecture and preserves saved settings and logs. A downloaded
appliance image may not retain Git metadata; in that case clone a fresh copy
on a network-connected Pi and run its installer to switch the service to it.

### Validation status

Automated tests cover native pulse acceptance/rejection, malformed WiFi/BLE and
HCI/radiotap input, Remote ID, stale GPS, evidence logs, capture bounds, watchlist
settings and Flask downloads. Image provisioning repeats these tests against
the ARM-built library. No Pi is attached in the development session: camera,
Bluetooth, monitor-adapter compatibility and optical range remain unverified
on physical Pi hardware. Linux is not hard real time; inspect actual ADC sample
rate/gaps and validate a known optical pulse source under recording/capture load.
