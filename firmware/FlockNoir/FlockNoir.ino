// =============================================================================
//  FLOCK NOIR
//  Seeed XIAO ESP32-S3 Sense  -  IR-camera-flash "wardriving" logger
//
//  Scans the camera's NIR view for a pulsed IR illuminator (~10 Hz, ~20% duty,
//  850 nm -- the signature of many ALPR / "Flock" surveillance cameras). On a
//  confident detection it: (1) raises an alert on the web UI, (2) tags the
//  current GPS position (Quectel LC29H) and appends a row to a CSV on the SD
//  card.
//
//  Subsystems: OV2640 camera | LC29H GPS (NMEA/UART) | SD (SPI) | buzzer |
//              WiFi wardriver (WiGLE CSV) | SoftAP web UI
//
//  Arduino IDE board: "XIAO_ESP32S3"  (enable PSRAM: OPI PSRAM)
//  Libraries: esp32 core (>=3.x) + TinyGPSPlus
//  ---------------------------------------------------------------------------
//  v0.2 -- camera IR detector + branded UI + buzzer/RTTTL + WiFi wardriver.
//  Two SEPARATE logs on SD:  /logs/flock_*.csv  (IR ALPR hits)
//                            /wardrive/wigle_*.csv  (WiGLE WiFi wardrive)
//  A dedicated 850 nm photodiode on an ADC pin is the recommended next upgrade
//  for locking the exact 20 ms/80 ms IR timing (see README).
// =============================================================================
#include "esp_camera.h"
#include "img_converters.h"     // frame2jpg() for the live view
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>          // captive portal (iPhone can't reach a bare IP)
#include <FS.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGPS++.h>

#include "config.h"
#include "detector.h"
#include "buzzer.h"
#include "wardriver.h"
#include "recorder.h"
#include "web_ui.h"
#include "logo.h"

// ---------------------------------------------------------------------------
//  Globals
// ---------------------------------------------------------------------------
WebServer   server(WEB_PORT);
DNSServer   dnsServer;
const IPAddress AP_IP(192, 168, 4, 1);
TinyGPSPlus gps;
HardwareSerial GPSserial(GPS_UART_NUM);
Detector    detector;

bool     g_sdReady   = false;
uint32_t g_scanned   = 1;            // scanned pixels per frame
uint32_t g_logged    = 0;
uint32_t g_lastAlert = 0;
String   g_csvPath;

// fps estimate
uint32_t g_frames = 0, g_fpsWin = 0;
float    g_fps = 0;

struct RecentAlert { char t[24]; double lat, lon; float hz, duty, conf; };
RecentAlert g_recent[RECENT_ALERTS];
int         g_recentCount = 0, g_recentHead = 0;

// ---------------------------------------------------------------------------
//  Time / GPS helpers
// ---------------------------------------------------------------------------
void isoUtc(char *out, size_t n) {
  if (gps.date.isValid() && gps.time.isValid()) {
    snprintf(out, n, "%04d-%02d-%02dT%02d:%02d:%02dZ",
             gps.date.year(), gps.date.month(), gps.date.day(),
             gps.time.hour(), gps.time.minute(), gps.time.second());
  } else {
    snprintf(out, n, "nofix+%lus", (unsigned long)(millis() / 1000));
  }
}

// WiGLE "FirstSeen" wants "YYYY-MM-DD HH:MM:SS".
void wigleTime(char *out, size_t n) {
  if (gps.date.isValid() && gps.time.isValid()) {
    snprintf(out, n, "%04d-%02d-%02d %02d:%02d:%02d",
             gps.date.year(), gps.date.month(), gps.date.day(),
             gps.time.hour(), gps.time.minute(), gps.time.second());
  } else {
    snprintf(out, n, "1970-01-01 00:00:00");
  }
}

// ---------------------------------------------------------------------------
//  Camera
// ---------------------------------------------------------------------------
bool initCamera() {
  camera_config_t c = {};
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer   = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM;  c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM;  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;  c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM;  c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM;  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM; c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM; c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM;  c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = CAM_XCLK_HZ;
  c.frame_size   = CAM_FRAMESIZE;
  c.pixel_format = PIXFORMAT_GRAYSCALE;      // 1 byte/pixel -> fast temporal scan
  c.fb_location  = CAMERA_FB_IN_PSRAM;
  c.fb_count     = CAM_FB_COUNT;
  c.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;   // take every frame, don't drop

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("[CAM] init failed 0x%x\n", err);
    return false;
  }

  // CRITICAL: freeze exposure/gain/white-balance so the pulse is not
  // auto-corrected away. This is the biggest lever for detection quality.
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_whitebal(s, 0);
    s->set_awb_gain(s, 0);
    s->set_exposure_ctrl(s, 0);   // AEC off
    s->set_aec2(s, 0);
    s->set_gain_ctrl(s, 0);       // AGC off
    s->set_aec_value(s, CAM_AEC_VALUE);
    s->set_agc_gain(s, CAM_AGC_GAIN);
    s->set_brightness(s, CAM_BRIGHTNESS);
    s->set_gainceiling(s, GAINCEILING_2X);
    s->set_lenc(s, 1);            // lens correction on
  }
  return true;
}

// Scan a grayscale frame: track brightest pixel + centroid + blob size.
void scanFrame(camera_fb_t *fb, uint16_t &level, uint16_t &blobPx,
               uint16_t &cx, uint16_t &cy) {
  const uint8_t *p = fb->buf;
  const int W = fb->width, H = fb->height;
  const int step = CAM_PIXEL_STRIDE;
  uint16_t maxV = 0; uint32_t sumX = 0, sumY = 0, cnt = 0; uint32_t scanned = 0;

  for (int y = 0; y < H; y += step) {
    const uint8_t *row = p + (size_t)y * W;
    for (int x = 0; x < W; x += step) {
      uint8_t v = row[x];
      if (v > maxV) maxV = v;
      if (v >= SAT_THRESHOLD) { sumX += x; sumY += y; cnt++; }
      scanned++;
    }
  }
  g_scanned = scanned ? scanned : 1;
  level  = maxV;
  blobPx = (cnt > 0xFFFF) ? 0xFFFF : cnt;
  cx = cnt ? (uint16_t)(sumX / cnt) : 0;
  cy = cnt ? (uint16_t)(sumY / cnt) : 0;
}

// ---------------------------------------------------------------------------
//  SD / CSV
// ---------------------------------------------------------------------------
bool initSD() {
  // XIAO ESP32-S3 Sense wires the microSD to SPI with CS on GPIO21 (Seeed wiki).
  if (SD_SCK_PIN >= 0)                                  // optional custom SPI pins
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("[SD] mount failed (card in? formatted FAT32?)");
    return false;
  }
  if (!SD.exists(CSV_DIR)) SD.mkdir(CSV_DIR);

  char name[48];
  snprintf(name, sizeof(name), "%s/flock_%lu.csv", CSV_DIR,
           (unsigned long)(millis()));
  g_csvPath = name;

  File f = SD.open(g_csvPath, FILE_WRITE);
  if (!f) { Serial.println("[SD] cannot open csv"); return false; }
  f.println(CSV_HEADER);
  f.close();
  Serial.printf("[SD] IR log -> %s\n", g_csvPath.c_str());
  return true;
}

void logDetection(const DetectionResult &d, uint16_t bx, uint16_t by) {
  char iso[24]; isoUtc(iso, sizeof(iso));
  double lat = gps.location.isValid() ? gps.location.lat() : NAN;
  double lon = gps.location.isValid() ? gps.location.lng() : NAN;
  double alt = gps.altitude.isValid() ? gps.altitude.meters() : NAN;
  int    sats= gps.satellites.isValid()? gps.satellites.value() : 0;
  double hdop= gps.hdop.isValid() ? gps.hdop.hdop() : NAN;

  // remember for the web UI
  RecentAlert &ra = g_recent[g_recentHead];
  strncpy(ra.t, iso, sizeof(ra.t)); ra.t[sizeof(ra.t)-1]=0;
  ra.lat = lat; ra.lon = lon; ra.hz = d.freqHz; ra.duty = d.dutyCycle; ra.conf = d.confidence;
  g_recentHead = (g_recentHead + 1) % RECENT_ALERTS;
  if (g_recentCount < RECENT_ALERTS) g_recentCount++;

  g_logged++;

  if (g_sdReady) {
    File f = SD.open(g_csvPath, FILE_APPEND);
    if (f) {
      f.printf("%s,%llu,%.6f,%.6f,%.1f,%d,%.1f,%.2f,%.3f,%.3f,%u,%u,%.3f,%u\n",
               iso, (unsigned long long)millis(), lat, lon, alt, sats, hdop,
               d.freqHz, d.dutyCycle, d.confidence, bx, by, d.blobFrac, d.levelPP);
      f.close();
    }
  }
  Serial.printf("[ALERT] %s  %.6f,%.6f  %.1fHz duty=%.0f%% conf=%.2f\n",
                iso, lat, lon, d.freqHz, d.dutyCycle*100, d.confidence);
}

// ---------------------------------------------------------------------------
//  Web handlers
// ---------------------------------------------------------------------------
void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

// Captive-portal catch-all: any unknown URL (incl. the OS "is there internet?"
// probes like /hotspot-detect.html, /generate_204) -> bounce to the UI. This is
// what makes the iPhone open the page automatically on connect.
void handleCaptive() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void handleStatus() {
  DetectionResult d = detector.last();
  char iso[24]; isoUtc(iso, sizeof(iso));

  String j; j.reserve(1600);
  j += "{";
  j += "\"detected\":"   + String(d.detected ? "true":"false");
  j += ",\"freq\":"      + String(d.freqHz, 2);
  j += ",\"duty\":"      + String(d.dutyCycle, 3);
  j += ",\"confidence\":"+ String(d.confidence, 3);
  j += ",\"fps\":"       + String(g_fps, 1);
  j += ",\"sd\":"        + String(g_sdReady ? "true":"false");
  j += ",\"logged\":"    + String(g_logged);
  j += ",\"fix\":"       + String(gps.location.isValid() ? "true":"false");
  j += ",\"buzzer\":"    + String(buzzer.enabled() ? "true":"false");
  j += ",\"wd\":"        + String(wardriver.enabled() ? "true":"false");
  j += ",\"wdScan\":"    + String(wardriver.scanning() ? "true":"false");
  j += ",\"wdLogged\":"  + String(wardriver.logged());
  j += ",\"wdTotal\":"   + String(wardriver.lastTotal());
  j += ",\"wdNew\":"     + String(wardriver.newLast());
  j += ",\"rec\":"       + String(recorder.active() ? "true":"false");
  j += ",\"recAudio\":"  + String(recorder.audioOn() ? "true":"false");
  j += ",\"recSecs\":"   + String(recorder.seconds());
  j += ",\"recFrames\":" + String(recorder.frames());
  j += ",\"sats\":"      + String(gps.satellites.isValid()? gps.satellites.value():0);
  if (gps.location.isValid()) {
    j += ",\"lat\":" + String(gps.location.lat(), 6);
    j += ",\"lon\":" + String(gps.location.lng(), 6);
  }
  j += ",\"time\":\"" + String(iso) + "\"";
  j += ",\"recent\":[";
  for (int i = 0; i < g_recentCount; i++) {
    int idx = (g_recentHead - 1 - i + RECENT_ALERTS) % RECENT_ALERTS;
    RecentAlert &ra = g_recent[idx];
    if (i) j += ",";
    j += "{\"t\":\"" + String(ra.t) + "\",\"lat\":" + String(ra.lat,6) +
         ",\"lon\":" + String(ra.lon,6) + ",\"hz\":" + String(ra.hz,2) +
         ",\"duty\":" + String(ra.duty,3) + ",\"conf\":" + String(ra.conf,3) + "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
}

void handleLog() {
  if (!g_sdReady) { server.send(404, "text/plain", "no SD card"); return; }
  File f = SD.open(g_csvPath, FILE_READ);
  if (!f) { server.send(404, "text/plain", "no log"); return; }
  server.sendHeader("Content-Disposition", "attachment; filename=flock_ir_log.csv");
  server.streamFile(f, "text/csv");
  f.close();
}

// Download the WiGLE wardrive CSV.
void handleWardriveCsv() {
  if (!g_sdReady) { server.send(404, "text/plain", "no SD card"); return; }
  File f = SD.open(wardriver.csvPath(), FILE_READ);
  if (!f) { server.send(404, "text/plain", "no wardrive log"); return; }
  server.sendHeader("Content-Disposition", "attachment; filename=wigle_wardrive.csv");
  server.streamFile(f, "text/csv");
  f.close();
}

// Live view: JPEG-encode the current (grayscale/NIR) frame on demand.
// The browser re-requests this a few times a second for a "video" feed, which
// coexists with the detection loop far better than a blocking MJPEG stream.
void handleFrame() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) { server.send(503, "text/plain", "no frame"); return; }
  uint8_t *jpg = nullptr; size_t jpgLen = 0;
  bool ok = frame2jpg(fb, 80, &jpg, &jpgLen);
  esp_camera_fb_return(fb);
  if (!ok || !jpg) { server.send(500, "text/plain", "jpeg encode failed"); return; }
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(jpgLen);
  server.send(200, "image/jpeg", "");
  server.sendContent((const char *)jpg, jpgLen);
  free(jpg);
}

// Start/stop recording. POST action=start|stop, audio=0|1.
void handleRec() {
  String action = server.arg("action");
  if (action == "start") {
    bool withAudio = server.arg("audio") == "1";
    bool ok = recorder.start(withAudio);
    String r = "{\"ok\":"; r += ok ? "true" : "false";
    r += ",\"audio\":"; r += recorder.audioOn() ? "true" : "false"; r += "}";
    server.send(ok ? 200 : 500, "application/json", r);
  } else if (action == "stop") {
    recorder.stop();
    server.send(200, "application/json", "{\"ok\":true}");
  } else {
    server.send(400, "application/json", "{\"ok\":false}");
  }
}

// List recordings in /videos as JSON [{name,size},...].
void handleRecs() {
  String j = "[";
  if (g_sdReady) {
    File dir = SD.open(REC_DIR);
    if (dir && dir.isDirectory()) {
      bool first = true;
      for (File e = dir.openNextFile(); e; e = dir.openNextFile()) {
        if (e.isDirectory()) continue;
        String nm = String(e.name());
        int slash = nm.lastIndexOf('/'); if (slash >= 0) nm = nm.substring(slash + 1);
        if (!first) j += ",";
        first = false;
        j += "{\"name\":\"" + nm + "\",\"size\":" + String((uint32_t)e.size()) + "}";
      }
    }
  }
  j += "]";
  server.send(200, "application/json", j);
}

// Download one recording:  /api/rec/get?f=rec_1234.avi
void handleRecGet() {
  if (!g_sdReady) { server.send(404, "text/plain", "no SD"); return; }
  String f = server.arg("f");
  if (f.indexOf("..") >= 0 || f.indexOf('/') >= 0) { server.send(400, "text/plain", "bad name"); return; }
  String path = String(REC_DIR) + "/" + f;
  File file = SD.open(path.c_str(), FILE_READ);
  if (!file) { server.send(404, "text/plain", "not found"); return; }
  String ct = f.endsWith(".wav") ? "audio/wav" : "video/x-msvideo";
  server.sendHeader("Content-Disposition", "attachment; filename=" + f);
  server.streamFile(file, ct);
  file.close();
}

// Toggle wardriving on/off (POST en=0/1).
void handleWardrive() {
  if (server.hasArg("en")) wardriver.setEnabled(server.arg("en") == "1");
  server.send(200, "application/json", "{\"ok\":true}");
}

// Serve the brand logo UNALTERED: prefer an SD /logo.png override, else the
// copy embedded in flash (logo.h).
void handleLogo() {
  if (g_sdReady && SD.exists("/logo.png")) {
    File f = SD.open("/logo.png", FILE_READ);
    if (f) { server.streamFile(f, "image/png"); f.close(); return; }
  }
  if (LOGO_PNG_LEN > 0) {
    server.sendHeader("Cache-Control", "max-age=86400");
    server.send_P(200, LOGO_MIME, (const char *)LOGO_PNG, LOGO_PNG_LEN);
    return;
  }
  server.send(404, "text/plain", "no logo");
}

// GET current buzzer/tone settings as JSON.
void handleGetSettings() { server.send(200, "application/json", buzzer.toJson()); }

// POST buzzer/tone settings (application/x-www-form-urlencoded).
void handleSetSettings() {
  buzzer.setEnabled(server.arg("enabled") == "1");
  int count = server.arg("count").toInt();
  if (count < 0) count = 0;
  if (count > BUZZER_MAX_TONES) count = BUZZER_MAX_TONES;
  buzzer.setCount(count);
  for (int i = 0; i < count; i++) {
    buzzer.setTone(i, server.arg("nm" + String(i)), server.arg("rt" + String(i)));
  }
  buzzer.setAlertIdx(server.arg("alertIdx").toInt());
  buzzer.save();
  server.send(200, "application/json", "{\"ok\":true}");
}

// POST a one-off RTTTL preview (arg rtttl=...), or idx=N to play a slot.
void handleTest() {
  if (server.hasArg("rtttl") && server.arg("rtttl").length())
    buzzer.play(server.arg("rtttl"));
  else if (server.hasArg("idx"))
    buzzer.playSlot(server.arg("idx").toInt());
  server.send(200, "application/json", "{\"ok\":true}");
}

// ---------------------------------------------------------------------------
//  Setup / loop
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Flock Noir v0.3 ===");

  GPSserial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  if (!initCamera()) Serial.println("[CAM] DISABLED (check ribbon/pins)");
  detector.begin(g_scanned);
  buzzer.begin();                    // loads tones from NVS, optional boot chirp

  g_sdReady = initSD();
  recorder.begin(g_sdReady);

  // AP + STA: SoftAP serves the UI; STA lets the wardriver scan WiFi.
  WiFi.mode(WIFI_AP_STA);
  // Pin the SoftAP to 192.168.4.1 (this is also the ESP32 default, made explicit).
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, (strlen(AP_PASSWORD) >= 8) ? AP_PASSWORD : nullptr);
  WiFi.setSleep(false);
  Serial.print("[AP] "); Serial.print(AP_SSID);
  Serial.print("  http://"); Serial.println(WiFi.softAPIP());

  // Captive portal: answer every DNS query with our IP so phones (esp. iPhone)
  // pop the UI automatically instead of refusing a "no-internet" network.
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", AP_IP);

  wardriver.begin(g_sdReady);
  Serial.printf("[WD] wardriver %s -> %s\n",
                wardriver.enabled() ? "ON" : "off", wardriver.csvPath().c_str());

  server.on("/", handleRoot);
  server.on("/logo.png", handleLogo);
  server.on("/api/status", handleStatus);
  server.on("/api/frame.jpg", handleFrame);
  server.on("/api/log", handleLog);
  server.on("/api/wardrive.csv", handleWardriveCsv);
  server.on("/api/wardrive", HTTP_POST, handleWardrive);
  server.on("/api/rec", HTTP_POST, handleRec);
  server.on("/api/recs", handleRecs);
  server.on("/api/rec/get", handleRecGet);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_POST, handleSetSettings);
  server.on("/api/test", HTTP_POST, handleTest);
  server.onNotFound(handleCaptive);        // captive-portal redirect for phones
  server.begin();

  g_fpsWin = millis();
}

void loop() {
  // 0) captive-portal DNS (answers phone probes so the UI opens on connect)
  dnsServer.processNextRequest();

  // 1) feed GPS bytes
  while (GPSserial.available()) gps.encode(GPSserial.read());

  // 2) grab + scan one camera frame
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) {
    uint16_t level, blobPx, cx, cy;
    scanFrame(fb, level, blobPx, cx, cy);
    detector.feed(level, blobPx, micros());
    recorder.addVideoFrame(fb);           // append to AVI if recording
    esp_camera_fb_return(fb);

    g_frames++;
    // last centroid for logging
    static uint16_t s_cx = 0, s_cy = 0; s_cx = cx; s_cy = cy;

    // 3) periodic analysis
    static uint32_t lastAnalyze = 0;
    uint32_t now = millis();
    if (now - lastAnalyze >= ANALYZE_EVERY_MS) {
      lastAnalyze = now;
      DetectionResult d = detector.analyze();
      if (d.detected && (now - g_lastAlert >= ALERT_HOLDOFF_MS)) {
        g_lastAlert = now;
        logDetection(d, s_cx, s_cy);
        buzzer.playAlert();               // sound the configured alert tone
      }
    }

    // fps estimate
    if (now - g_fpsWin >= 1000) {
      g_fps = g_frames * 1000.0f / (now - g_fpsWin);
      g_frames = 0; g_fpsWin = now;
    }
  }

  // 4) advance buzzer tune + drain mic audio if recording (non-blocking)
  buzzer.update();
  recorder.pumpAudio();

  // 5) wardriver: async WiFi scan -> WiGLE CSV (separate file), GPS-tagged
  {
    bool fix   = gps.location.isValid();
    double lat = fix ? gps.location.lat() : 0.0;
    double lon = fix ? gps.location.lng() : 0.0;
    double alt = gps.altitude.isValid() ? gps.altitude.meters() : 0.0;
    char when[24]; wigleTime(when, sizeof(when));
    wardriver.update(fix, lat, lon, alt, when);
  }

  // 6) serve web clients (cheap, non-blocking)
  server.handleClient();
}
