// =============================================================================
//  Flock Noir  -  config.h
//  All hardware pins, thresholds and tunables live here.
//  Board: Seeed Studio XIAO ESP32-S3 Sense (OV2640 + microSD on the sense board)
// =============================================================================
#pragma once

// -----------------------------------------------------------------------------
//  WiFi SoftAP  (device makes its own network -- no internet needed in field)
// -----------------------------------------------------------------------------
#define AP_SSID        "Flock Noir"
#define AP_PASSWORD    "flocknoir"      // >= 8 chars, or "" for open AP
#define WEB_PORT       80

// -----------------------------------------------------------------------------
//  GPS  (NMEA GNSS over a hardware UART)
//  Pick your GPS wiring PROFILE below. All modules talk NMEA over UART; the
//  profiles just set the pins and default baud.  GPS TX -> ESP RX, GPS RX -> ESP TX.
//
//    XIAO_L76K : the Seeed "L76K GNSS for XIAO" board sandwiched on the XIAO.
//                Uses the standard XIAO UART pins D7/D6 (GPIO44/43) at 9600.
//                (Confirmed from Seeed's example: RXPin=D7, TXPin=D6, 9600.)
//    EXTERNAL  : a discrete module wired to the header pins yourself
//                (e.g. an ATGM336H, default 9600 baud; a Quectel LC29H is 115200).
//
//  D6/D7 are free on the S3 because the USB-CDC port carries the serial console.
// -----------------------------------------------------------------------------
#define GPS_PROFILE_XIAO_L76K  1
#define GPS_PROFILE_EXTERNAL   2

#define GPS_PROFILE   GPS_PROFILE_EXTERNAL    // <-- SET YOUR GPS HERE

#define GPS_UART_NUM   1          // Serial1 (UART1)
#if   GPS_PROFILE == GPS_PROFILE_XIAO_L76K
  #define GPS_RX_PIN   44         // D7  <- L76K TX
  #define GPS_TX_PIN   43         // D6  -> L76K RX
  #define GPS_BAUD     9600
#elif GPS_PROFILE == GPS_PROFILE_EXTERNAL
  #define GPS_RX_PIN   44         // D7  <- module TX   (change to your wiring)
  #define GPS_TX_PIN   43         // D6  -> module RX   (change to your wiring)
  #define GPS_BAUD     9600       // ATGM336H = 9600; set 115200 for a Quectel LC29H
#else
  #error "Set GPS_PROFILE to GPS_PROFILE_XIAO_L76K or GPS_PROFILE_EXTERNAL"
#endif

// -----------------------------------------------------------------------------
//  Buzzer  (PASSIVE piezo -- required for RTTTL melodies; active buzzers
//  only make one fixed pitch and will NOT play tunes).
//  Signal -> BUZZER_PIN (through ~100 ohm), other leg -> GND.
//  D0 = GPIO1 is a safe free header pin (see README wiring).
//  We drive it with a dedicated LEDC channel that does NOT collide with the
//  camera's XCLK (camera uses LEDC channel/timer 0). If you ever see the camera
//  image glitch when the buzzer sounds, change BUZZER_LEDC_CHANNEL.
// -----------------------------------------------------------------------------
#define BUZZER_ENABLE_PIN     // comment out to build with no buzzer support
#define BUZZER_PIN         1          // D0 = GPIO1
#define BUZZER_LEDC_CHANNEL 5        // keep away from camera's channel 0
#define BUZZER_MAX_TONES   5         // configurable tone slots
#define BUZZER_STARTUP_BEEP 1        // 1 = chirp once on boot to prove wiring
// Bump this when the built-in default tones change, to re-seed them into NVS on
// devices that already saved the old set. (Resets tones to defaults.)
#define BUZZER_DEFAULTS_VERSION 2

// -----------------------------------------------------------------------------
//  microSD  (SPI, on the XIAO Sense expansion board)
//  Per the Seeed wiki, the Sense microSD is wired to the SPI bus with chip
//  select on GPIO21. The SPI bus itself uses the default header SPI pins:
//    SCK  = GPIO7  (D8)   MISO = GPIO8 (D9)   MOSI = GPIO9 (D10)
//  These do NOT overlap the camera pins, so SD + camera coexist.
//  Ref: wiki.seeedstudio.com/xiao_esp32s3_sense_filesystem  ->  SD.begin(21)
// -----------------------------------------------------------------------------
#define SD_CS_PIN      21
// Explicit Sense SPI pin map:
#define SD_SCK_PIN     7
#define SD_MISO_PIN    8
#define SD_MOSI_PIN    9

// -----------------------------------------------------------------------------
//  Camera pins  -  Seeed XIAO ESP32-S3 Sense (OV2640)
//  These are the documented XIAO ESP32-S3 camera pins.
// -----------------------------------------------------------------------------
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   10
#define SIOD_GPIO_NUM   40
#define SIOC_GPIO_NUM   39
#define Y9_GPIO_NUM     48
#define Y8_GPIO_NUM     11
#define Y7_GPIO_NUM     12
#define Y6_GPIO_NUM     14
#define Y5_GPIO_NUM     16
#define Y4_GPIO_NUM     18
#define Y3_GPIO_NUM     17
#define Y2_GPIO_NUM     15
#define VSYNC_GPIO_NUM  38
#define HREF_GPIO_NUM   47
#define PCLK_GPIO_NUM   13

// -----------------------------------------------------------------------------
//  Camera / sensor tuning  (the most important knobs for detection)
//  We run GRAYSCALE at a small frame size to maximize frame rate, and we FORCE
//  a fixed exposure/gain so the pulsing IR source is not auto-corrected away.
// -----------------------------------------------------------------------------
#define CAM_XCLK_HZ        20000000
// Frame size: FRAMESIZE_QQVGA (160x120) is a good speed/detail balance.
// FRAMESIZE_96X96 pushes fps higher if you need more temporal resolution.
#define CAM_FRAMESIZE      FRAMESIZE_QQVGA
#define CAM_FB_COUNT       2

// Manual exposure/gain (AEC/AGC/AWB are DISABLED in code).
// The reference build uses an IR-cut-free OV2640. These lower manual values
// reduce overexposure; calibrate for actual lighting rather than auto-exposure.
#define CAM_AEC_VALUE      150       // 0..1200, starting point for IR-cut-free OV2640
#define CAM_AGC_GAIN       2         // 0..30 fixed gain; tune under actual lighting
#define CAM_BRIGHTNESS     0         // -2..2
#define CAM_PIXEL_STRIDE   1         // scan every Nth pixel (1=all, 2=faster)

// -----------------------------------------------------------------------------
//  Detection SENSITIVITY
//  These defaults are deliberately LOOSE ("approximate" mode): they fire when a
//  signal is *close* to the ALPR IR pattern, so you WILL get false positives --
//  that is the trade for not missing a real one. The confidence and duty are
//  written to the CSV so you can filter/verify later. To make it STRICTER again
//  (fewer false positives), move each value back toward the "(strict)" note.
//    ALWAYS visually confirm an actual camera before trusting a hit.
// -----------------------------------------------------------------------------
// A pixel at/above this 8-bit value counts toward the IR "blob" (compactness).
#define SAT_THRESHOLD      120       // (strict ~230) lower = counts dimmer spots
// Minimum peak-to-peak brightness swing before we bother analyzing.
#define MIN_AMPLITUDE      6         // (strict ~25) lower = catch faint flicker
// Target flash characteristics (Flock-style ~10 Hz, ~20% duty).
#define TARGET_FREQ_HZ     10.0f
#define TARGET_PERIOD_MS   100.0f
#define PERIOD_TOL_MS      45.0f     // (strict ~18) accept ~6.5..18 Hz between edges
#define DUTY_MIN           0.03f     // (strict ~0.10)
#define DUTY_MAX           0.40f     // a real strobe is short-on (~0.2); random noise reads ~0.5
// How many matching cycles inside the window before we call it.
#define MIN_GOOD_CYCLES    3         // (strict ~8) fewer = fires on a brief match
// Confidence needed to raise/log/beep an alert (0..1).
#define DETECT_CONFIDENCE  0.30f     // (strict ~0.6) lower = more (and looser) hits
// Minimum fraction of rising-edge intervals that must land near the target
// period. Kept loose because at ~40 fps a 20 ms pulse can fall between frames
// and a real strobe only scores ~0.6 here.
#define PERIOD_SCORE_MIN   0.35f
// Regularity gate: spread of the matching intervals (std/mean). A real strobe
// repeats at the same interval (measures <= 0.09); sensor noise that happens
// to land in the tolerance band is ragged (>= 0.19). This is what stops the
// detector from beeping on noise with no camera present.
#define JITTER_MAX         0.15f
// Reject only if the bright area fills MORE than this fraction of the frame
// (that is whole-frame flicker, not a compact source). Loose here = permissive.
#define BLOB_MAX_FRACTION  0.90f     // (strict ~0.35)

// Sliding analysis window.
#define SAMPLE_BUFFER      256       // ~5-8 s of frames depending on fps
#define ANALYZE_EVERY_MS   300       // run the analyzer this often

// -----------------------------------------------------------------------------
//  OPT101 analog pulse input. Independent ADC1 task, measured pulse width/duty,
//  consecutive intervals, adaptive noise threshold and sampling-gap rejection.
//  The defaults are a timing profile, not a unique Flock identifier.
//  Wiring and headroom checks: HARDWARE.md. Behaviour: docs/RADIO.md.
// -----------------------------------------------------------------------------
#define IR_SENSOR_PIN         2       // D1 / GPIO2 (ADC1_CH1)
#define IR_SAMPLE_HZ          1000    // ADC samples per second
#define IR_RING               256     // decimated scope ring (for the UI)
#define IR_MIN_HZ             8.0f     // configurable timing profile, not device identity
#define IR_MAX_HZ             12.0f
#define IR_REQUIRED_INTERVALS 4       // consecutive valid intervals -> detection
#define IR_ACTIVE_WINDOW_MS   300     // hold a detection this long after last pulse
#define IR_THR_IDLE           120     // AC threshold (12-bit counts) to arm an edge
#define IR_THR_LOCKED         70      // falling-edge hysteresis floor
#define IR_DEFAULT_ENABLED    0       // 0 = off until you wire the sensor
#define IR_DUTY_MIN           0.10f
#define IR_DUTY_MAX           0.30f
#define IR_PULSE_MIN_MS       8.0f
#define IR_PULSE_MAX_MS       35.0f
#define IR_MAX_SAMPLE_GAP_US  5000
static_assert(IR_SENSOR_PIN >= 2 && IR_SENSOR_PIN <= 6,
              "Use a free XIAO Sense ADC1 header pin D1-D5 for OPT101");
static_assert(IR_SENSOR_PIN != BUZZER_PIN && IR_SENSOR_PIN != GPS_RX_PIN && IR_SENSOR_PIN != GPS_TX_PIN,
              "OPT101 pin conflicts with another peripheral");

// Re-arm: suppress duplicate log rows / beeps for the same source for this long.
#define ALERT_HOLDOFF_MS   4000

// -----------------------------------------------------------------------------
//  Logging
// -----------------------------------------------------------------------------
#define CSV_DIR            "/logs"          // IR-detection CSVs live here
#define CSV_HEADER  "iso_utc,uptime_ms,source,lat,lon,alt_m,sats,hdop,freq_hz,duty,confidence,blob_x,blob_y,blob_frac,level_pp,evidence,logged_uptime_ms"
#define RECENT_ALERTS      12        // how many recent alerts the web UI keeps

// -----------------------------------------------------------------------------
//  Wardriver  (WiFi 2.4 GHz -> WiGLE-compatible CSV, SEPARATE file from IR log)
//  Modeled on the "piglet" wardriver: WiFi-only, WiGLE CSV, GPS-tagged.
//  Runs the radio in AP+STA so the web UI stays up; note that each scan hops
//  channels, so a connected browser may hiccup briefly during a scan. The IR
//  camera detector is unaffected.
// -----------------------------------------------------------------------------
#define WARDRIVE_DEFAULT_ON   0              // start wardriving at boot? (off = UI stable)
#define WARDRIVE_SCAN_MS      3000           // min gap between WiFi scans (ms)
#define WARDRIVE_DEDUP_RING   512            // recent BSSIDs kept to skip dupes
#define WARDRIVE_DIR          "/wardrive"    // WiGLE CSVs live here
#define WARDRIVE_LOG_NEEDS_FIX 1             // only log APs when GPS has a fix
#define WIGLE_APP_RELEASE     "0.2"
#define WIGLE_DEVICE_NAME     "FlockNoir"

// -----------------------------------------------------------------------------
//  Recorder  (live view -> SD).  Video = MJPEG AVI (plays in VLC). Optional
//  audio = onboard PDM mic -> a matching WAV sidecar (same base name), so
//  "video + audio" is reliable without risky in-file A/V muxing.
//  The PDM mic on the XIAO Sense is CLK=GPIO42, DATA=GPIO41 (onboard, no wiring).
// -----------------------------------------------------------------------------
#define REC_DIR            "/videos"
#define REC_MAX_FRAMES     18000        // ~20 min @ 15fps; auto-stops at cap
#define REC_JPEG_QUALITY   80
#define MIC_CLK_PIN        42
#define MIC_DATA_PIN       41
#define MIC_SAMPLE_RATE    16000        // Hz, mono 16-bit

// Radio controls use no peripheral header pads. BOOT restores the dashboard.
#define RADIO_BOOT_PIN 0
#define RADIO_DWELL_MS 350
#define RADIO_CORRELATE_MS 3000
#define GPS_MAX_AGE_MS 3000
