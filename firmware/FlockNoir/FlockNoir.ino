// =============================================================================
//  FLOCK NOIR
//  Seeed XIAO ESP32-S3 Sense  -  IR-camera-flash "wardriving" logger
//
//  Combines passive WiFi/BLE signatures with camera brightness patterns and
//  optional OPT101 pulse timing. Candidates are logged with fresh GPS fixes;
//  timing or an OUI alone cannot establish a camera's identity or wavelength.
//  OV2640 | ATGM336H NMEA/UART | OPT101 ADC | SD | buzzer | WiGLE | web UI
//
//  Arduino IDE board: "XIAO_ESP32S3"  (enable PSRAM: OPI PSRAM)
//  Libraries: esp32 core (>=3.x) + TinyGPSPlus
//  ---------------------------------------------------------------------------
//  /logs/flock_*.csv | /wardrive/wigle_*.csv | /radio/events_*.jsonl
// =============================================================================
#include "esp_camera.h"
#include "jpeg_luma.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>          // captive portal (iPhone can't reach a bare IP)
#include <FS.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGPS++.h>
#include <atomic>
#include <freertos/queue.h>

#include "config.h"
#include "detector.h"
#include "buzzer.h"
#include "wardriver.h"
#include "recorder.h"
#include "irsensor.h"
#include "radio.h"
#include "gps_time.h"
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
bool g_cameraReady = false;
bool g_serialReplyActive = false;
uint8_t *g_previewJpeg=nullptr;
size_t g_previewLength=0;
uint32_t g_cameraDecodeErrors=0,g_previewAt=0,g_cameraFrames=0;
uint16_t g_cameraWidth=0,g_cameraHeight=0;
struct CameraSample {bool ok;uint16_t level,blob,cx,cy;uint32_t stamp,decodeMs,generation;};
QueueHandle_t g_cameraSamples=nullptr;
uint8_t *g_analysisJpeg=nullptr,*g_analysisLuma=nullptr;
JpegLuma g_jpegLuma;
std::atomic<bool> g_analysisPending{false};
size_t g_analysisLength=0;
uint32_t g_analysisStamp=0,g_analysisGeneration=0,g_modeAt=0,g_previewRequestedAt=0;
uint32_t g_cameraAnalysisMs=0,g_analysisFrames=0,g_cameraAt=0,g_analyzedAt=0;
float g_analysisFps=0;
uint32_t g_logErrors = 0;
uint32_t g_sdTotalMB = 0, g_sdFreeMB = 0;
uint32_t g_scanned   = 1;            // scanned pixels per frame
uint32_t g_logged    = 0;
uint32_t g_lastAlert = 0;
bool     g_muted     = false;        // quick "mute alerts" (still logs)
String   g_csvPath;

// fps estimate
uint32_t g_frames = 0, g_fpsWin = 0;
float    g_fps = 0;

struct RecentAlert { char t[24]; char src[8]; char evidence[24]; double lat, lon; float hz, duty, conf; };
RecentAlert g_recent[RECENT_ALERTS];
int         g_recentCount = 0, g_recentHead = 0;

// ---------------------------------------------------------------------------
//  Time / GPS helpers
// ---------------------------------------------------------------------------
bool freshFix() { return gps.location.isValid() && gps.location.age() < GPS_MAX_AGE_MS; }
bool freshGpsTime() {
  return gps.date.isValid() && gps.time.isValid() && gps.date.age()<GPS_MAX_AGE_MS &&
    gps.time.age()<GPS_MAX_AGE_MS && validGpsDate(gps.date.year(),gps.date.month(),gps.date.day()) &&
    gps.time.hour()<24 && gps.time.minute()<60 && gps.time.second()<60;
}

void isoUtc(char *out, size_t n) {
  if (freshGpsTime()) {
    snprintf(out, n, "%04d-%02d-%02dT%02d:%02d:%02dZ",
             gps.date.year(), gps.date.month(), gps.date.day(),
             gps.time.hour(), gps.time.minute(), gps.time.second());
  } else {
    snprintf(out, n, "nofix+%lus", (unsigned long)(millis() / 1000));
  }
}

// WiGLE "FirstSeen" wants "YYYY-MM-DD HH:MM:SS".
void wigleTime(char *out, size_t n) {
  if (freshGpsTime()) {
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
void freeCameraBuffers() {
  free(g_previewJpeg);free(g_analysisJpeg);free(g_analysisLuma);
  g_previewJpeg=g_analysisJpeg=g_analysisLuma=nullptr;
  if(g_cameraSamples){vQueueDelete(g_cameraSamples);g_cameraSamples=nullptr;}
}
bool initCamera() {
  g_previewJpeg=(uint8_t*)ps_malloc(CAM_JPEG_CAPACITY);
  g_analysisJpeg=(uint8_t*)ps_malloc(CAM_JPEG_CAPACITY);
  g_analysisLuma=(uint8_t*)heap_caps_malloc(CAM_ANALYSIS_WIDTH*CAM_ANALYSIS_HEIGHT,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  g_cameraSamples=xQueueCreate(2,sizeof(CameraSample));
  if(!g_previewJpeg || !g_analysisJpeg || !g_analysisLuma || !g_cameraSamples) {
    freeCameraBuffers();
    Serial.println("[CAM] PSRAM image buffers unavailable");return false;
  }
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
  c.pixel_format = PIXFORMAT_JPEG;
  c.jpeg_quality = CAM_JPEG_QUALITY;
  c.fb_location  = CAMERA_FB_IN_PSRAM;
  c.fb_count     = CAM_FB_COUNT;
  c.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;   // take every frame, don't drop

  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("[CAM] init failed 0x%x\n", err);
    freeCameraBuffers();
    return false;
  }
  // DMA directly into the S3's PSRAM instead of copying DMA chunks on the
  // WiFi core. Apply before manual controls: this call reinitializes the sensor.
  err = esp_camera_set_psram_mode(true);
  if (err != ESP_OK) {
    Serial.printf("[CAM] PSRAM DMA initialization failed 0x%x\n", err);
    esp_camera_deinit();freeCameraBuffers();
    return false;
  }

  // CRITICAL: freeze exposure/gain/white-balance so the pulse is not
  // auto-corrected away. This is the biggest lever for detection quality.
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    int controls = 0;
    controls |= s->set_whitebal(s, 0);
    controls |= s->set_awb_gain(s, 0);
    controls |= s->set_exposure_ctrl(s, 0);   // AEC off
    controls |= s->set_aec2(s, 0);
    controls |= s->set_gain_ctrl(s, 0);       // AGC off
    controls |= s->set_aec_value(s, CAM_AEC_VALUE);
    controls |= s->set_agc_gain(s, CAM_AGC_GAIN);
    controls |= s->set_brightness(s, CAM_BRIGHTNESS);
    controls |= s->set_gainceiling(s, GAINCEILING_2X);
    controls |= s->set_lenc(s, 1);            // lens correction on
    if (controls) {
      Serial.println("[CAM] one or more sensor controls failed");
      esp_camera_deinit();freeCameraBuffers();return false;
    }
  } else {
    Serial.println("[CAM] sensor unavailable");
    esp_camera_deinit();freeCameraBuffers();return false;
  }
  return true;
}

// Scan JPEG block-average luminance: brightest block, centroid and blob size.
bool scanFrame(uint16_t &level, uint16_t &blobPx,
               uint16_t &cx, uint16_t &cy) {
  if(!g_jpegLuma.decode(g_analysisJpeg,g_analysisLength,640,480,g_analysisLuma,CAM_ANALYSIS_WIDTH*CAM_ANALYSIS_HEIGHT))return false;
  const uint8_t *p = g_analysisLuma;
  const int W = CAM_ANALYSIS_WIDTH, H = CAM_ANALYSIS_HEIGHT;
  const int step = CAM_PIXEL_STRIDE;
  uint16_t maxV = 0; uint32_t sumX = 0, sumY = 0, cnt = 0; uint32_t scanned = 0;

  for (int y = 0; y < H; y += step) {
    const uint8_t *row = p + (size_t)y * W;
    for (int x = 0; x < W; x += step) {
      uint8_t v=row[x];
      if (v > maxV) maxV = v;
      if (v >= SAT_THRESHOLD) { sumX += x; sumY += y; cnt++; }
      scanned++;
    }
  }
  (void)scanned;
  level  = maxV;
  blobPx = (cnt > 0xFFFF) ? 0xFFFF : cnt;
  cx = cnt ? (uint16_t)(sumX * 8 / cnt) : 0;
  cy = cnt ? (uint16_t)(sumY * 8 / cnt) : 0;
  return true;
}
void cameraAnalysisTask(void *) {
  for(;;) {
    if(g_analysisPending.load()) {
      CameraSample sample={};
      uint32_t started=millis();
      sample.ok=scanFrame(sample.level,sample.blob,sample.cx,sample.cy);
      sample.generation=g_analysisGeneration;
      sample.stamp=g_analysisStamp;sample.decodeMs=millis()-started;
      xQueueSend(g_cameraSamples,&sample,0);
      g_analysisPending=false;
    }
    vTaskDelay(1);
  }
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
  if (!SD.exists(CSV_DIR) && !SD.mkdir(CSV_DIR)) return false;

  char name[48];
  snprintf(name, sizeof(name), "%s/alpr_%08lx.csv", CSV_DIR,
           (unsigned long)esp_random());
  g_csvPath = name;

  File f = SD.open(g_csvPath, FILE_WRITE);
  if (!f) { Serial.println("[SD] cannot open csv"); return false; }
  if (!f.println(CSV_HEADER)) return false;
  f.close();
  g_sdTotalMB = (uint32_t)(SD.totalBytes() >> 20);
  g_sdFreeMB  = (uint32_t)((SD.totalBytes() - SD.usedBytes()) >> 20);
  Serial.printf("[SD] ALPR log -> %s  (%lu/%lu MB free)\n",
                g_csvPath.c_str(), (unsigned long)g_sdFreeMB, (unsigned long)g_sdTotalMB);
  return true;
}

// Generic hit logger used by BOTH the camera detector and the IR photodiode.
void logHit(const char *source, float freqHz, float duty, float conf,
            uint16_t bx, uint16_t by, float blobFrac, uint16_t levelPP, uint32_t observedMs,
            float irPulseMs,float irSampleHz) {
  if(!radio.opticalEnabled())return;
  char iso[24]; isoUtc(iso, sizeof(iso));
  bool fixAtLog = freshFix() && millis()-observedMs <= GPS_MAX_AGE_MS;
  double lat = fixAtLog ? gps.location.lat() : NAN;
  double lon = fixAtLog ? gps.location.lng() : NAN;
  double alt = fixAtLog && gps.altitude.isValid() && gps.altitude.age()<GPS_MAX_AGE_MS ? gps.altitude.meters() : NAN;
  int    sats= gps.satellites.isValid()? gps.satellites.value() : 0;
  double hdop= gps.hdop.isValid() ? gps.hdop.hdop() : NAN;

  // remember for the web UI
  RecentAlert &ra = g_recent[g_recentHead];
  strncpy(ra.t, iso, sizeof(ra.t)); ra.t[sizeof(ra.t)-1]=0;
  strncpy(ra.src, source, sizeof(ra.src)); ra.src[sizeof(ra.src)-1]=0;
  radio.optical(!strcmp(source,"ir"),observedMs);
  auto fused=radio.fusion(observedMs);
  RadioEvidence nearby=radio.nearbyAlpr(observedMs);
  bool ir=fused.has(AlprFusion::Ir),camera=fused.has(AlprFusion::Camera);
  const char *evidence = nearby.found ? "optical_radio_nearby" : ir ? "ir_timing_match" : "camera_pattern";
  strlcpy(ra.evidence,evidence,sizeof(ra.evidence));
  ra.lat = lat; ra.lon = lon; ra.hz = freqHz; ra.duty = duty; ra.conf = conf;
  g_recentHead = (g_recentHead + 1) % RECENT_ALERTS;
  if (g_recentCount < RECENT_ALERTS) g_recentCount++;

  g_logged++;

  if (g_sdReady) {
    File f = SD.open(g_csvPath, FILE_APPEND);
    if (f) {
      char row[768];
      int n=snprintf(row,sizeof(row),"%s,%llu,%s,%.6f,%.6f,%.1f,%d,%.1f,%.2f,%.3f,%.3f,%u,%u,%.3f,%u,%s,%lu,%s,%s,%s,%s,%s,%s,%u,%u,%u,%s,%s,%.1f,%s,%s\n",
               iso, (unsigned long long)observedMs, source, lat, lon, alt, sats, hdop,
               freqHz, duty, conf, bx, by, blobFrac, levelPP, evidence, (unsigned long)millis(),
               fused.method(),
               nearby.found?nearby.match.category:"Optical pulse candidate",
               nearby.found?"corroborated_camera_candidate":"possible_camera",nearby.mac,
               nearby.found?String(nearby.rssi).c_str():"",nearby.match.method,nearby.match.tier,ir,camera,
               isfinite(irPulseMs)?String(irPulseMs,1).c_str():"",isfinite(irSampleHz)?String(irSampleHz,1).c_str():"",
               g_analysisFps,camera?(detector.last().aliased?"aliased_candidate":"frame_limited_pattern"):"",radio.profile());
      if(n<=0 || size_t(n)>=sizeof(row) || f.write((const uint8_t*)row,n)!=size_t(n))++g_logErrors;
      f.close();
    } else ++g_logErrors;
  }
  if(!g_serialReplyActive) Serial.printf("[ALERT/%s] %s  %.6f,%.6f  %.1fHz duty=%.0f%% conf=%.2f\n",
                source, iso, lat, lon, freqHz, duty*100, conf);
  if(!g_muted)buzzer.requestAlert(nearby.found?AlertTones::Combined:!strcmp(source,"ir")?AlertTones::Ir:AlertTones::Camera);
}

// ALPR radio and optical candidates share a dedicated ALPR CSV. Other categories stay in JSONL. Empty optical
// fields in a radio-only row mean unmeasured, never an invented pulse frequency.
void logRadioHit(uint32_t observed,const char *iso,bool fix,double lat,double lon,
    const char *protocol,const char *mac,RadioProtocol::Match match,int rssi,bool ir,bool camera) {
  if(!match.alpr || radio.scanMode()==ScanMode::Wardrive || radio.scanMode()==ScanMode::Axon)return;
  ++g_logged;
  if(!g_sdReady)return;
  File f=SD.open(g_csvPath,FILE_APPEND);
  if(!f){++g_logErrors;return;}
  String row=String(iso)+","+String(observed)+","+protocol+","+
    (fix?String(lat,6):String())+","+(fix?String(lon,6):String());
  // Columns 6..15: optical measurements and ancillary GPS fields unavailable here.
  for(int i=0;i<10;++i)row+=',';
  row+=String(",")+(match.alpr && (ir || camera)?"optical_radio_nearby":"radio_candidate")+","+String(millis())+","+
    (match.alpr?radio.fusion(observed).method():detectionMethod(false,false,!strcmp(protocol,"ble"),!strcmp(protocol,"wifi")))+","+match.category+","+
    RadioProtocol::assessment(match,ir,camera)+","+mac+","+String(rssi)+","+match.method+","+
    String(match.tier)+","+(ir?"1":"0")+","+(camera?"1":"0");
  IrResult optical=irSensor.result();
  row+=","+String(ir && optical.detected?String(optical.pulseMs,1):String())+","+
    (ir && optical.detected?String(optical.sampleHz,1):String())+","+String(g_analysisFps,1)+","+
    (camera?(detector.last().aliased?"aliased_candidate":"frame_limited_pattern"):"")+","+radio.profile()+"\n";
  if(f.print(row)!=row.length())++g_logErrors;
}

void logDetection(const DetectionResult &d, uint16_t bx, uint16_t by) {
  logHit("camera", d.freqHz, d.dutyCycle, d.confidence, bx, by, d.blobFrac, d.levelPP, millis(), NAN, NAN);
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

String statusJson() {
  DetectionResult d = detector.last();
  char iso[24]; isoUtc(iso, sizeof(iso));

  String j; j.reserve(1600);
  j += "{";
  j += "\"detected\":"   + String(d.detected ? "true":"false");
  j += ",\"freq\":"      + String(d.freqHz, 2);
  j += ",\"duty\":"      + String(d.dutyCycle, 3);
  j += ",\"confidence\":"+ String(d.confidence, 3);
  j += ",\"fps\":"       + String(g_analysisFps, 1);
  j += ",\"captureFps\":" + String(g_fps, 1);
  j += ",\"cameraReady\":" + String(g_cameraReady ? "true":"false");
  j += ",\"cameraWidth\":"+String(g_cameraWidth)+",\"cameraHeight\":"+String(g_cameraHeight);
  j += ",\"cameraDecodeErrors\":"+String(g_cameraDecodeErrors)+",\"cameraFrames\":"+String(g_cameraFrames);
  j += ",\"cameraAnalysisMs\":"+String(g_cameraAnalysisMs)+",\"analysisFrames\":"+String(g_analysisFrames);
  j += ",\"version\":\"" FLOCK_NOIR_VERSION "\"";
  j += ",\"sd\":"        + String(g_sdReady ? "true":"false");
  j += ",\"logged\":"    + String(g_logged);
  j += ",\"fix\":"       + String(freshFix() ? "true":"false");
  j += ",\"buzzer\":"    + String(buzzer.enabled() ? "true":"false");
  j += ",\"wd\":"        + String(wardriver.enabled() ? "true":"false");
  j += ",\"wdScan\":"    + String(wardriver.scanning() ? "true":"false");
  j += ",\"wdWifiLogged\":"+String(wardriver.wifiLogged())+",\"wdBleLogged\":"+String(wardriver.bleLogged());
  j += ",\"wdErrors\":"+String(wardriver.errors())+",\"wdNoFix\":"+String(wardriver.noFix());
  j += ",\"exclusiveModes\":true,\"opticalActive\":"+String(radio.opticalEnabled()?"true":"false");
  j += ",\"cameraAliased\":"+String(d.aliased?"true":"false");
  j += ",\"irProfile\":\"experimental_8_12_hz\"";
  j += ",\"wdLogged\":"  + String(wardriver.logged());
  j += ",\"wdTotal\":"   + String(wardriver.lastTotal());
  j += ",\"wdNew\":"     + String(wardriver.newLast());
  j += ",\"rec\":"       + String(recorder.active() ? "true":"false");
  j += ",\"recAudio\":"  + String(recorder.audioOn() ? "true":"false");
  j += ",\"recSecs\":"   + String(recorder.seconds());
  j += ",\"recFrames\":" + String(recorder.frames());
  j += ",\"sats\":"      + String(gps.satellites.isValid()? gps.satellites.value():0);
  if (freshFix()) {
    j += ",\"lat\":" + String(gps.location.lat(), 6);
    j += ",\"lon\":" + String(gps.location.lng(), 6);
  }
  j += ",\"time\":\"" + String(iso) + "\"";
  // GPS health: chars proves the module is wired + talking at the right baud;
  // good/fail are valid/invalid NMEA sentences.
  j += ",\"gpsChars\":" + String((uint32_t)gps.charsProcessed());
  j += ",\"gpsGood\":"  + String((uint32_t)gps.passedChecksum());
  j += ",\"gpsFail\":"  + String((uint32_t)gps.failedChecksum());
  j += ",\"hdop\":"     + String(gps.hdop.isValid() ? gps.hdop.hdop() : 0.0, 1);
  // Live signal diagnostic: amplitude + a recent brightness waveform (0..100).
  j += ",\"amp\":"      + String(d.levelPP);
  {
    uint8_t w[64]; int wn = detector.snapshot(w, 64);
    j += ",\"wave\":[";
    for (int i = 0; i < wn; i++) { if (i) j += ","; j += String(w[i]); }
    j += "]";
  }
  // IR photodiode sensor
  IrResult ir = irSensor.result();
  j += ",\"irEn\":"    + String(irSensor.enabled() ? "true":"false");
  j += ",\"irPaused\":"+String(irSensor.suspended()?"true":"false");
  j += ",\"irDet\":"   + String(ir.detected ? "true":"false");
  j += ",\"irPresent\":"+ String(ir.present ? "true":"false");
  j += ",\"irFreq\":"  + String(ir.freqHz, 1);
  j += ",\"irDuty\":"  + String(ir.dutyCycle, 3);
  j += ",\"irAmp\":"   + String(ir.amp);
  {
    uint8_t w[64]; int wn = irSensor.snapshot(w, 64);
    j += ",\"irWave\":[";
    for (int i = 0; i < wn; i++) { if (i) j += ","; j += String(w[i]); }
    j += "]";
  }
  j += ",\"irRaw\":" + String(ir.raw);
  j += ",\"irBaseline\":" + String(ir.baseline);
  j += ",\"irClipped\":" + String(ir.clipped ? "true":"false");
  j += ",\"irPulseMs\":" + String(ir.pulseMs,1);
  j += ",\"irSampleHz\":" + String(ir.sampleHz,1);
  j += ",\"irGaps\":" + String(ir.gaps);
  j += ",\"irDroppedEvents\":" + String(ir.droppedEvents);
  j += ",\"irNoise\":" + String(ir.noise,1);
  j += ",\"radioNearby\":" + String(radio.recentAlpr(millis()) ? "true":"false");
  j += ",\"radioAlert\":" + radio.alertJson();
  j += ",\"alprAlert\":" + radio.alertJson(true);
  j += ",\"profile\":" + jsonQuote(radio.profile());
  j += ",\"fusion\":" + radio.fusionJson(millis());
  j += ",\"radioEvents\":" + String(radio.events());
  j += ",\"logErrors\":" + String(g_logErrors);
  // system / QoL
  j += ",\"muted\":"   + String(g_muted ? "true":"false");
  j += ",\"uptime\":"  + String((uint32_t)(millis()/1000));
  {
    static uint32_t sdT = 0;
    if (g_sdReady && (millis() - sdT > 10000)) {
      sdT = millis();
      g_sdFreeMB = (uint32_t)((SD.totalBytes() - SD.usedBytes()) >> 20);
    }
  }
  j += ",\"sdFree\":"  + String(g_sdFreeMB);
  j += ",\"sdTotal\":" + String(g_sdTotalMB);
  j += ",\"recent\":[";
  for (int i = 0; i < g_recentCount; i++) {
    int idx = (g_recentHead - 1 - i + RECENT_ALERTS) % RECENT_ALERTS;
    RecentAlert &ra = g_recent[idx];
    if (i) j += ",";
    j += "{\"t\":\"" + String(ra.t) + "\",\"src\":\"" + String(ra.src) +
         "\",\"lat\":" + jsonNumber(ra.lat) +
         ",\"lon\":" + jsonNumber(ra.lon) + ",\"hz\":" + String(ra.hz,2) +
         ",\"duty\":" + String(ra.duty,3) + ",\"conf\":" + String(ra.conf,3) + ",\"evidence\":" + jsonQuote(ra.evidence) + "}";
  }
  j += "]}";
  return j;
}

void handleStatus() { server.send(200, "application/json", statusJson()); }

void handleLog() {
  if (!g_sdReady) { server.send(404, "text/plain", "no SD card"); return; }
  File f = SD.open(g_csvPath, FILE_READ);
  if (!f) { server.send(404, "text/plain", "no log"); return; }
  server.sendHeader("Content-Disposition", "attachment; filename=flock_detections.csv");
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

// Serve the latest background-encoded VGA image without blocking analysis.
void handleFrame() {
  g_previewRequestedAt=millis();
  if (!g_previewLength || millis()-g_previewAt>1000) {server.send(503,"text/plain","no recent frame");return;}
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(g_previewLength);
  server.send(200, "image/jpeg", "");
  server.sendContent((const char *)g_previewJpeg, g_previewLength);
}

// Start/stop recording. POST action=start|stop, audio=0|1.
void handleRec() {
  if(radio.scanMode()==ScanMode::Axon || radio.scanMode()==ScanMode::Wardrive) {
    server.send(409,"application/json","{\"error\":\"Recording is paused in this scan mode\"}");return;
  }
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
  bool ok=server.hasArg("en") && radio.setProfile(server.arg("en")=="1"?"wardrive":"general");
  server.send(ok?200:400,"application/json",ok?"{\"ok\":true}":"{\"ok\":false}");
}

// Toggle the IR photodiode detector on/off (POST en=0/1).
void handleIrSensor() {
  if (server.hasArg("en")) irSensor.setEnabled(server.arg("en") == "1");
  server.send(200, "application/json", "{\"ok\":true}");
}

// Toggle "mute alerts" (POST en=0/1) - silences the buzzer, still logs.
void handleMute() {
  if (server.hasArg("en")) g_muted = (server.arg("en") == "1");
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
  int count = server.arg("count").toInt();
  if(count<1 || count>BUZZER_MAX_TONES) {server.send(400,"application/json","{\"error\":\"Invalid tone count\"}");return;}
  for(int i=0;i<count;++i) {
    if(server.arg("nm"+String(i)).length()>48 || server.arg("rt"+String(i)).length()>1024) {
      server.send(400,"application/json","{\"error\":\"Tone too long\"}");return;
    }
  }
  for(int i=0;i<AlertTones::Count;++i) {
    String key="sound_"+String(AlertTones::ids[i]);
    if(server.hasArg(key) && !AlertTones::valid(server.arg(key).c_str(),count)) {
      server.send(400,"application/json","{\"error\":\"Invalid alert sound\"}");return;
    }
  }
  buzzer.setEnabled(server.arg("enabled") == "1");
  buzzer.setCount(count);
  for (int i = 0; i < count; i++) {
    buzzer.setTone(i, server.arg("nm" + String(i)), server.arg("rt" + String(i)));
  }
  buzzer.setAlertIdx(server.arg("alertIdx").toInt());
  for(int i=0;i<AlertTones::Count;++i) {
    String key="sound_"+String(AlertTones::ids[i]);
    if(server.hasArg(key))buzzer.setAlertSound(i,server.arg(key));
  }
  bool ok=buzzer.save();
  server.send(ok?200:500, "application/json",ok?"{\"ok\":true}":"{\"error\":\"Settings storage failed\"}");
}

// POST a one-off RTTTL preview (arg rtttl=...), or idx=N to play a slot.
void handleTest() {
  if(server.arg("rtttl").length()>1024) {server.send(400,"text/plain","Tone too long");return;}
  if(server.hasArg("sound")) {
    String sound=server.arg("sound");
    if(!AlertTones::valid(sound.c_str(),buzzer.count())) {server.send(400,"text/plain","Invalid sound");return;}
    const auto *p=AlertTones::preset(sound.c_str());
    if(p) {buzzer.stop();if(*p->rtttl)buzzer.play(p->rtttl);}
    else buzzer.playSlot(AlertTones::slot(sound.c_str()));
  } else if (server.hasArg("rtttl") && server.arg("rtttl").length())
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
  Serial.println("\n=== Flock Noir v" FLOCK_NOIR_VERSION " ===");

  if (!GPSserial.setRxBufferSize(2048)) Serial.println("[GPS] RX buffer allocation failed");
  GPSserial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  if (!GPSserial) Serial.println("[GPS] UART initialization failed");

  g_cameraReady = initCamera();
  if (!g_cameraReady) Serial.println("[CAM] DISABLED (check ribbon/pins)");
  g_scanned=CAM_ANALYSIS_WIDTH*CAM_ANALYSIS_HEIGHT;
  detector.begin(g_scanned);
  if(g_cameraReady && xTaskCreatePinnedToCore(cameraAnalysisTask,"camera-analysis",6144,nullptr,1,nullptr,1)!=pdPASS) {
    Serial.println("[CAM] analysis task allocation failed");g_cameraReady=false;
    esp_camera_deinit();freeCameraBuffers();
  }
  buzzer.begin();                    // loads tones from NVS, optional boot chirp

  g_sdReady = initSD();
  recorder.begin(g_sdReady);
  irSensor.begin();                  // starts the 1 kHz ADC sampling task

  // AP + STA: SoftAP serves the UI; STA lets the wardriver scan WiFi.
  if (!WiFi.mode(WIFI_AP_STA)) Serial.println("[AP] mode failed");
  // Pin the SoftAP to 192.168.4.1 (this is also the ESP32 default, made explicit).
  if (!WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0))) Serial.println("[AP] config failed");
  if (!WiFi.softAP(AP_SSID, (strlen(AP_PASSWORD) >= 8) ? AP_PASSWORD : nullptr)) Serial.println("[AP] start failed");
  WiFi.setSleep(false);
  Serial.print("[AP] "); Serial.print(AP_SSID);
  Serial.print("  http://"); Serial.println(WiFi.softAPIP());

  // Captive portal: answer every DNS query with our IP so phones (esp. iPhone)
  // pop the UI automatically instead of refusing a "no-internet" network.
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", AP_IP);

  wardriver.begin(g_sdReady);
  radio.begin(g_sdReady);
  Serial.printf("[WD] wardriver %s -> %s\n",
                wardriver.enabled() ? "ON" : "off", wardriver.csvPath().c_str());

  server.on("/", handleRoot);
  server.on("/logo.png", handleLogo);
  server.on("/api/status", handleStatus);
  server.on("/api/frame.jpg", handleFrame);
  server.on("/api/log", handleLog);
  server.on("/api/alpr.csv", handleLog);
  server.on("/api/wardrive.csv", handleWardriveCsv);
  server.on("/api/wardrive", HTTP_POST, handleWardrive);
  server.on("/api/irsensor", HTTP_POST, handleIrSensor);
  server.on("/api/mute", HTTP_POST, handleMute);
  server.on("/api/rec", HTTP_POST, handleRec);
  server.on("/api/recs", handleRecs);
  server.on("/api/rec/get", handleRecGet);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_POST, handleSetSettings);
  server.on("/api/test", HTTP_POST, handleTest);
  registerRadioRoutes();
  server.onNotFound(handleCaptive);        // captive-portal redirect for phones
  server.begin();

  g_fpsWin = millis();
}

// Called only on the foreground task after the persisted mode changes.
void applyScanProfile() {
  irSensor.setSuspended(!radio.opticalEnabled());
  wardriver.setEnabled(radio.scanMode()==ScanMode::Wardrive);
  detector.begin(g_scanned);g_analysisFps=0;g_analyzedAt=0;g_modeAt=micros();
  g_recentHead=g_recentCount=0;g_lastAlert=0;
  if(radio.scanMode()==ScanMode::Axon || radio.scanMode()==ScanMode::Wardrive)recorder.stop();
}
void loop() {
  buzzer.setMuted(g_muted);
  // 0) captive-portal DNS (answers phone probes so the UI opens on connect)
  dnsServer.processNextRequest();

  // 1) feed GPS bytes
  while (GPSserial.available()) gps.encode(GPSserial.read());

  // GPS health heartbeat on the serial console (every 5 s)
  static uint32_t gpsDbg = 0;
  if (!g_serialReplyActive && millis() - gpsDbg >= 5000) {
    gpsDbg = millis();
    Serial.printf("[GPS] chars=%lu good=%lu fail=%lu sats=%d fix=%d\n",
                  (unsigned long)gps.charsProcessed(),
                  (unsigned long)gps.passedChecksum(),
                  (unsigned long)gps.failedChecksum(),
                  gps.satellites.isValid() ? gps.satellites.value() : 0,
                  freshFix() ? 1 : 0);
  }

  // 2) grab + scan one camera frame
  camera_fb_t *fb = g_cameraReady && esp_camera_available_frames() ? esp_camera_fb_get() : nullptr;
  if (fb) {
    bool valid=fb->format==PIXFORMAT_JPEG && fb->width==640 && fb->height==480 && fb->len<=CAM_JPEG_CAPACITY;
    g_cameraWidth=fb->width;g_cameraHeight=fb->height;g_cameraAt=millis();
    if(valid) {
      if(radio.opticalEnabled() || recorder.active() || (g_previewRequestedAt && millis()-g_previewRequestedAt<3000)) {
        memcpy(g_previewJpeg,fb->buf,fb->len);g_previewLength=fb->len;g_previewAt=millis();
      }
      recorder.addVideoFrame(fb);
      if(radio.opticalEnabled() && !g_analysisPending.load()) {
        memcpy(g_analysisJpeg,fb->buf,fb->len);g_analysisLength=fb->len;
        g_analysisStamp=uint32_t(fb->timestamp.tv_sec*1000000ULL+fb->timestamp.tv_usec);
        g_analysisGeneration=radio.generation();
        g_analysisPending=true;
      }
    } else {
      ++g_cameraDecodeErrors;detector.begin(g_scanned);
    }
    esp_camera_fb_return(fb);

    g_frames++;++g_cameraFrames;
    uint32_t now = millis();
    // fps estimate
    if (now - g_fpsWin >= 1000) {
      g_fps = g_frames * 1000.0f / (now - g_fpsWin);
      g_frames = 0; g_fpsWin = now;
    }
  }
  CameraSample sample;
  while(g_cameraSamples && xQueueReceive(g_cameraSamples,&sample,0)==pdTRUE) {
    if(!radio.opticalEnabled() || sample.generation!=radio.generation() || int32_t(sample.stamp-g_modeAt)<0)continue;
    g_cameraAnalysisMs=sample.decodeMs;
    if(sample.ok) {
      ++g_analysisFrames;
      g_analyzedAt=millis();
      static uint32_t frames=0,window=0;
      ++frames;
      if(g_analyzedAt-window>=1000) {
        g_analysisFps=frames*1000.0f/(g_analyzedAt-window);frames=0;window=g_analyzedAt;
      }
      static uint32_t previous=0,lastAnalyze=0;
      if(previous && sample.stamp-previous>50000)detector.begin(g_scanned);
      previous=sample.stamp;detector.feed(sample.level,sample.blob,sample.stamp);
      uint32_t now=millis();
      if(now-lastAnalyze>=ANALYZE_EVERY_MS) {
        lastAnalyze=now;auto d=detector.analyze();
        if(d.detected && now-g_lastAlert>=ALERT_HOLDOFF_MS) {
          g_lastAlert=now;logDetection(d,sample.cx,sample.cy);
        }
      }
    } else {
      ++g_cameraDecodeErrors;detector.begin(g_scanned);
    }
  }

  // Drain latched IR events even if an SD download delayed the foreground loop.
  if(!g_cameraAt || millis()-g_cameraAt>500 || !g_analyzedAt || millis()-g_analyzedAt>500) {
    detector.begin(g_scanned);g_analysisFps=0;
  }
  IrResult event;
  while (irSensor.popEvent(event)) {
    float conf = min(1.0f, event.validCount / 8.0f);
    logHit("ir", event.freqHz, event.dutyCycle, conf, 0, 0, 0, event.amp, event.timestampMs, event.pulseMs, event.sampleHz);
  }

  // 4) advance buzzer tune + drain mic audio if recording (non-blocking)
  buzzer.update();
  recorder.pumpAudio();

  // 5) wardriver: async WiFi scan -> WiGLE CSV (separate file), GPS-tagged
  {
    bool fix=freshFix() && freshGpsTime() && gps.hdop.isValid() && gps.hdop.age()<GPS_MAX_AGE_MS &&
      gps.hdop.hdop()>0 && gps.altitude.isValid() && gps.altitude.age()<GPS_MAX_AGE_MS;
    double lat = fix ? gps.location.lat() : 0.0;
    double lon = fix ? gps.location.lng() : 0.0;
    double alt = gps.altitude.isValid() ? gps.altitude.meters() : 0.0;
    char when[24]; wigleTime(when, sizeof(when));
    // HDOP is dimensionless; 5 m UERE is an explicit estimate, not measured accuracy.
    wardriver.update(fix, lat, lon, alt, fix?gps.hdop.hdop()*5.0:NAN, when);
  }

  char radioIso[24]; isoUtc(radioIso,sizeof(radioIso));
  radio.update(freshFix(), gps.location.lat(), gps.location.lng(), gps.altitude.meters(), radioIso,
               irSensor.result().detected, detector.last().detected, g_muted);
  serviceRadioSerial();

  // 6) serve web clients (cheap, non-blocking)
  server.handleClient();
  vTaskDelay(1); // yield while the next camera frame is being acquired
}
