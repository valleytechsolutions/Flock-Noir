// =============================================================================
//  Flock Noir  -  wardriver.cpp
// =============================================================================
#include "wardriver.h"
#include "radio.h"
#include <WiFi.h>
#include <FS.h>
#include <SD.h>
#include <Preferences.h>

Wardriver wardriver;
static Preferences wdPrefs;

// Map ESP32 auth mode -> WiGLE-style capability string.
static const char *authToWigle(wifi_auth_mode_t m) {
  switch (m) {
    case WIFI_AUTH_OPEN:            return "[ESS]";
    case WIFI_AUTH_WEP:             return "[WEP][ESS]";
    case WIFI_AUTH_WPA_PSK:         return "[WPA-PSK][ESS]";
    case WIFI_AUTH_WPA2_PSK:        return "[WPA2-PSK-CCMP][ESS]";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "[WPA-PSK][WPA2-PSK-CCMP][ESS]";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "[WPA2-EAP][ESS]";
    case WIFI_AUTH_WPA3_PSK:        return "[WPA3-SAE][ESS]";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "[WPA2-PSK][WPA3-SAE][ESS]";
    default:                        return "[ESS]";
  }
}

static uint64_t bssidToU64(const uint8_t *b) {
  uint64_t v = 0;
  for (int i = 0; i < 6; i++) v = (v << 8) | b[i];
  return v;
}

// CSV-escape an SSID (quote if it contains comma/quote/newline).
static String csvField(const String &in) {
  bool needQuote = false;
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == ',' || c == '"' || c == '\n' || c == '\r') { needQuote = true; break; }
  }
  if (!needQuote) return in;
  String out = "\"";
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"') out += '"';   // double the quote
    out += c;
  }
  out += '"';
  return out;
}

// -----------------------------------------------------------------------------
void Wardriver::begin(bool sdReady) {
  _sdReady = sdReady;

  wdPrefs.begin("flockwd", true);
  _enabled = wdPrefs.getBool("en", WARDRIVE_DEFAULT_ON ? true : false);
  wdPrefs.end();

  if (_sdReady) {
    if (!SD.exists(WARDRIVE_DIR)) SD.mkdir(WARDRIVE_DIR);
    char name[64];
    snprintf(name, sizeof(name), "%s/wigle_%08lx.csv", WARDRIVE_DIR,
             (unsigned long)esp_random());
    _csvPath = name;
    writeHeader();
  }
}

void Wardriver::writeHeader() {
  File f = SD.open(_csvPath, FILE_WRITE);
  if (!f) return;
  // WiGLE pre-header (line 1) + column header (line 2).
  f.printf("WigleWifi-1.4,appRelease=%s,model=XIAO ESP32S3 Sense,release=%s,"
           "device=%s,display=,board=ESP32S3,brand=Seeed\n",
           WIGLE_APP_RELEASE, WIGLE_APP_RELEASE, WIGLE_DEVICE_NAME);
  f.println("MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,"
            "CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
  f.close();
}

void Wardriver::setEnabled(bool e) {
  _enabled = e;
  wdPrefs.begin("flockwd", false);
  wdPrefs.putBool("en", e);
  wdPrefs.end();
}

bool Wardriver::seen(uint64_t b) {
  for (int i = 0; i < _ringCount; i++) if (_ring[i] == b) return true;
  return false;
}
void Wardriver::remember(uint64_t b) {
  _ring[_ringHead] = b;
  _ringHead = (_ringHead + 1) % WARDRIVE_DEDUP_RING;
  if (_ringCount < WARDRIVE_DEDUP_RING) _ringCount++;
}

// -----------------------------------------------------------------------------
void Wardriver::update(bool haveFix, double lat, double lon, double alt,
                       const char *whenStr) {
  if (radio.field()) { _scanning = false; return; }
  int st = WiFi.scanComplete();

  if (st == WIFI_SCAN_RUNNING) { _scanning = true; return; }

  if (st >= 0) {                                   // a scan finished
    _scanning = false;
    process(st, haveFix, lat, lon, alt, whenStr);
    WiFi.scanDelete();
    _lastScanAt = millis();
    return;                                        // start next scan on a later loop
  }

  // st == WIFI_SCAN_FAILED (-2): idle. Kick a new scan when it's time.
  // IMPORTANT: pause scanning while a device is connected to our SoftAP. A WiFi
  // scan hops channels and knocks AP clients offline, which would make the web
  // UI unreachable. So we only scan when nobody is viewing the UI -- i.e. while
  // you're actually driving with the phone disconnected. This also means the
  // wardriver can never lock you out of the UI.
  bool uiInUse = (WiFi.softAPgetStationNum() > 0);
  if (_enabled && !uiInUse && (millis() - _lastScanAt >= WARDRIVE_SCAN_MS)) {
    int result = WiFi.scanNetworks(true /*async*/, true /*show_hidden*/, true /*passive*/, 120);
    _scanning = result == WIFI_SCAN_RUNNING;
    _lastScanAt = millis(); // a failed start retries after the normal interval
  }
}

void Wardriver::process(int n, bool haveFix, double lat, double lon, double alt,
                        const char *whenStr) {
  _lastScanTotal = n;
  _newThisScan = 0;
  if (n <= 0) return;

  bool canLog = _sdReady && (haveFix || !WARDRIVE_LOG_NEEDS_FIX);
  File f;
  if (canLog) f = SD.open(_csvPath, FILE_APPEND);

  for (int i = 0; i < n; i++) {
    uint8_t *bssid = WiFi.BSSID(i);
    if (!bssid) continue;
    uint64_t id = bssidToU64(bssid);
    if (seen(id)) continue;                        // logged recently -> skip
    _newThisScan++;

    if (f) {
      String ssid = csvField(WiFi.SSID(i));
      // AccuracyMeters: rough GPS accuracy placeholder (WiGLE expects a number).
      size_t written = f.printf("%s,%s,%s,%s,%d,%d,%.6f,%.6f,%.1f,%.1f,WIFI\n",
               WiFi.BSSIDstr(i).c_str(), ssid.c_str(),
               authToWigle(WiFi.encryptionType(i)), whenStr,
               WiFi.channel(i), WiFi.RSSI(i),
               haveFix ? lat : 0.0, haveFix ? lon : 0.0,
               haveFix ? alt : 0.0, haveFix ? 10.0 : 9999.0);
      if (written) { remember(id); _logged++; }
    }
  }
  if (f) f.close();
}

void Wardriver::observePassive(const uint8_t *mac, const char *ssid, int channel, int rssi,
 bool privacy, bool fix, double lat, double lon, double alt, const char *when) {
  if (!_enabled || !_sdReady || (WARDRIVE_LOG_NEEDS_FIX && !fix)) return;
  uint64_t id=bssidToU64(mac); if(seen(id)) return;
  File f=SD.open(_csvPath,FILE_APPEND);if(!f)return;
  char address[18];snprintf(address,sizeof(address),"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  // Privacy bit alone cannot distinguish WPA/WEP; leave that explicit.
  size_t written=f.printf("%s,%s,%s,%s,%d,%d,%.6f,%.6f,%.1f,%.1f,WIFI\n",
    address,csvField(ssid).c_str(),privacy?"[UNKNOWN][ESS]":"[ESS]",when,channel,rssi,
    fix?lat:0,fix?lon:0,fix?alt:0,fix?10.0:9999.0);
  if(written) {remember(id);++_logged;++_lastScanTotal;++_newThisScan;}
}
