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
//  GPS  (Quectel LC29H -> ESP32-S3 hardware UART)
//  The LC29H speaks NMEA at 115200 baud by default.
//  Pick two FREE header pins. GPS TX -> ESP RX, GPS RX -> ESP TX.
//  D6=GPIO43 (TX0) and D7=GPIO44 (RX0) are free on the S3 because the USB-CDC
//  port carries the serial console. Change if you wire elsewhere.
// -----------------------------------------------------------------------------
#define GPS_UART_NUM   1          // use Serial1 (UART1)
#define GPS_RX_PIN     44         // ESP receives here  <- LC29H TX
#define GPS_TX_PIN     43         // ESP transmits here -> LC29H RX
#define GPS_BAUD       115200

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

// -----------------------------------------------------------------------------
//  microSD  (SPI, on the XIAO Sense expansion board)
//  Per the Seeed wiki, the Sense microSD is wired to the SPI bus with chip
//  select on GPIO21. The SPI bus itself uses the default header SPI pins:
//    SCK  = GPIO7  (D8)   MISO = GPIO8 (D9)   MOSI = GPIO9 (D10)
//  These do NOT overlap the camera pins, so SD + camera coexist.
//  Ref: wiki.seeedstudio.com/xiao_esp32s3_sense_filesystem  ->  SD.begin(21)
// -----------------------------------------------------------------------------
#define SD_CS_PIN      21
// If you ever wire the SPI bus to non-default pins, set them here (-1 = default):
#define SD_SCK_PIN     -1
#define SD_MISO_PIN    -1
#define SD_MOSI_PIN    -1

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

// Manual exposure/gain (AEC/AGC/AWB are DISABLED in code). Lower exposure keeps
// the frame dark so a bright IR emitter stands out as a saturated blob.
#define CAM_AEC_VALUE      120       // 0..1200 raw exposure; tune in field
#define CAM_AGC_GAIN       0         // 0..30 fixed gain; keep low
#define CAM_BRIGHTNESS     -1        // -2..2
#define CAM_PIXEL_STRIDE   1         // scan every Nth pixel (1=all, 2=faster)

// -----------------------------------------------------------------------------
//  Detection algorithm tunables
// -----------------------------------------------------------------------------
// A pixel at/above this 8-bit value counts as part of the (saturated) IR blob.
#define SAT_THRESHOLD      230
// Minimum peak-to-peak brightness swing in the tracked level for a "signal".
#define MIN_AMPLITUDE      25
// Target flash characteristics.
#define TARGET_FREQ_HZ     10.0f
#define TARGET_PERIOD_MS   100.0f
#define PERIOD_TOL_MS      18.0f     // accept 82..118 ms between rising edges
#define DUTY_MIN           0.10f     // accept 10%..35% duty
#define DUTY_MAX           0.35f
// How many good cycles inside the window before we declare a detection.
#define MIN_GOOD_CYCLES    8
// Confidence needed to raise/log an alert (0..1).
#define DETECT_CONFIDENCE  0.6f
// Blob must be reasonably compact: reject if blob covers more than this
// fraction of scanned pixels (that is whole-frame flicker, not a camera).
#define BLOB_MAX_FRACTION  0.35f

// Sliding analysis window.
#define SAMPLE_BUFFER      256       // ~5-8 s of frames depending on fps
#define ANALYZE_EVERY_MS   400       // run the analyzer this often

// Re-arm: suppress duplicate log rows for the same source for this long.
#define ALERT_HOLDOFF_MS   4000

// -----------------------------------------------------------------------------
//  Logging
// -----------------------------------------------------------------------------
#define CSV_DIR            "/logs"          // IR-detection CSVs live here
#define CSV_HEADER  "iso_utc,unix_ms,lat,lon,alt_m,sats,hdop,freq_hz,duty,confidence,blob_x,blob_y,blob_frac,level_pp"
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
