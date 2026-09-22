# =============================================================================
#  Flock Noir - Raspberry Pi target - config.py
#  All settings for the Pi build in one place.
#  Reference target: Pi Zero 2 W + Camera Module NoIR v2; hardware validation pending.
# =============================================================================
import os

# ---- paths ------------------------------------------------------------------
ROOT_DIR   = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # repo root
WEB_DIR    = os.path.join(ROOT_DIR, "web")            # shared UI (index.html, logo.png)
DATA_DIR   = os.environ.get("FLOCKNOIR_DATA", "/var/lib/flocknoir")
LOG_DIR    = os.path.join(DATA_DIR, "logs")            # IR detection CSVs
WARDRIVE_DIR = os.path.join(DATA_DIR, "wardrive")      # WiGLE CSVs
REC_DIR    = os.path.join(DATA_DIR, "videos")          # recordings
SETTINGS_FILE = os.path.join(DATA_DIR, "settings.json")
RADIO_DIR = os.path.join(DATA_DIR, "radio")

# ---- web --------------------------------------------------------------------
WEB_PORT   = int(os.environ.get("FLOCKNOIR_PORT", "80"))
AP_IP      = "192.168.4.1"      # hotspot address (set by pi/netmode.sh)

# ---- camera (Camera Module NoIR v2 / IMX219, or any libcamera CSI camera) ----
# The NoIR has NO IR-cut filter, so it sees 850 nm natively. We run a small,
# fast stream with auto-exposure/gain/white-balance OFF so the pulse is not
# corrected away (same principle as the XIAO build).
CAM_SIZE        = (640, 480)   # IMX219 supports up to ~90 fps at 640x480
CAM_FPS         = 60           # target frame rate (raise toward 90 if CPU allows)
CAM_EXPOSURE_US = 4000         # fixed exposure (microseconds); must be < 1/fps
CAM_GAIN        = 4.0          # fixed analogue gain
CAM_JPEG_QUALITY = 80

# ---- detection sensitivity (mirrors firmware/FlockNoir/config.h) -------------
# Loose "approximate" defaults: fires when a signal is close to the ALPR pattern.
# False positives are expected; confidence/duty are logged so you can filter.
SAT_THRESHOLD      = 120
MIN_AMPLITUDE      = 6
TARGET_PERIOD_MS   = 100.0
PERIOD_TOL_MS      = 45.0
DUTY_MIN           = 0.03
DUTY_MAX           = 0.40      # a real strobe is short-on (~0.2); random noise reads ~0.5
MIN_GOOD_CYCLES    = 3
DETECT_CONFIDENCE  = 0.30
PERIOD_SCORE_MIN   = 0.35      # loose: a 20 ms pulse can fall between frames at low fps
JITTER_MAX         = 0.15      # real strobes repeat at the same interval (<=0.09); noise >=0.19
BLOB_MAX_FRACTION  = 0.90
SAMPLE_BUFFER      = 512       # frames in the sliding window (~8 s at 60 fps)
ANALYZE_EVERY_MS   = 300
ALERT_HOLDOFF_MS   = 4000
RECENT_ALERTS      = 12

# ---- buzzer (PASSIVE piezo, required for RTTTL tunes) ------------------------
BUZZER_ENABLED  = True
BUZZER_PIN      = 18           # BCM GPIO18 (hardware-PWM capable). Signal -> GPIO18, other leg -> GND
BUZZER_STARTUP_TUNE = "Mario:d=4,o=5,b=200:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g,8p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b"
BUZZER_MAX_TONES = 5
DEVICE_ALERT_RTTTL = "Mario:d=4,o=5,b=200:16e6,16e6,32p,8e6,16c6,8e6,8g6"
DEFAULT_TONES = [
    ("Power Rangers", "MMPR:d=16,o=7,b=400:c#8,p,c#8,p,b,c#8,p,e8,p,c#8"),
    ("ALPR Alarm",    "Alarm:d=8,o=6,b=180:c,p,c,p,c7,p,c7,p,g,p,g"),
    ("Triple Chirp",  "Chirp:d=32,o=7,b=200:c,p,c,p,c"),
]

# ---- GPS (NMEA over serial; optional) ---------------------------------------
# Pi UART (GPIO14 TX / GPIO15 RX) = /dev/serial0. A USB GPS shows up as
# /dev/ttyUSB0 or /dev/ttyACM0. Set GPS_PORT = None to disable.
GPS_PORT = "/dev/serial0"
GPS_BAUD = 9600                # ATGM336H / L76K = 9600; Quectel LC29H = 115200
GPS_MAX_AGE_S = 3.0

# ---- Wi-Fi wardriver (optional, off by default) ------------------------------
# NOTE: a Pi Zero 2 W has ONE radio. Scanning while hosting the hotspot will
# interrupt clients, so scans are skipped while a phone is connected. For
# uninterrupted wardriving use a second USB Wi-Fi adapter and set WD_IFACE.
WARDRIVE_DEFAULT_ON = False
WD_IFACE        = "wlan0"
AP_IFACE        = "wlan0"
WARDRIVE_SCAN_S = 4
WIGLE_DEVICE    = "FlockNoir-Pi"

# ---- IR photodiode sensor (optional; needs an ADC - the Pi has none) ---------
# Use an MCP3008 on SPI (gpiozero supports it directly). Off by default.
IR_ENABLED_DEFAULT = False
IR_ADC_CHANNEL     = 0
IR_SPI_BUS, IR_SPI_DEVICE = 0, 0  # SPI0 CE0: BCM11/9/10/8 (SCLK/MISO/MOSI/CS)
IR_SAMPLE_HZ       = 1000
IR_MIN_HZ, IR_MAX_HZ = 8.0, 12.0
IR_REQUIRED_INTERVALS = 4
IR_ACTIVE_WINDOW_MS   = 300
IR_DUTY_MIN, IR_DUTY_MAX = 0.10, 0.30
IR_PULSE_MIN_MS, IR_PULSE_MAX_MS = 8, 35
IR_MAX_SAMPLE_GAP_US = 5000
IR_THR_IDLE   = 120            # in 12-bit counts (MCP3008 is 10-bit; scaled)
IR_THR_LOCKED = 70

# ---- passive radio capture --------------------------------------------------
# Set a dedicated monitor-capable USB adapter, e.g. wlan1, on a different
# physical radio from AP_IFACE. The hotspot interface is never repurposed.
# Empty disables WiFi packet capture; BLE still works independently.
RADIO_MONITOR_IFACE = os.environ.get("FLOCKNOIR_MONITOR_IFACE", "")
RADIO_HCI_INDEX = int(os.environ.get("FLOCKNOIR_HCI_INDEX", "0"))
RADIO_BLE_DEFAULT = True
RADIO_CHANNELS = tuple(range(1, 12))
RADIO_DWELL_S = 0.35
RADIO_CAPTURE_LIMIT = 16 * 1024 * 1024
RADIO_DEVICE_LIMIT = 128

# ---- recording ---------------------------------------------------------------
REC_BITRATE = 2_000_000
