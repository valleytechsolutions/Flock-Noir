// =============================================================================
//  Flock Noir  -  wardriver.h
//  WiFi (2.4 GHz) wardriver -> WiGLE-compatible CSV on the SD card.
//  Kept in its OWN log file, separate from the IR-detection CSV.
//  Non-blocking: uses async WiFi scans so the camera detector keeps running.
//  Modeled on the "piglet" wardriver (Hamspiced/piglet): WiFi-only, WiGLE CSV.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

class Wardriver {
public:
  // sdReady lets us log rows; opens/creates the WiGLE CSV with its header.
  void begin(bool sdReady);
  // Call every loop(); drives the async scan state machine.
  //   haveFix/lat/lon/alt : current GPS state
  //   whenStr : "YYYY-MM-DD HH:MM:SS" for the WiGLE FirstSeen column
  void update(bool haveFix, double lat, double lon, double alt, const char *whenStr);

  void observePassive(const uint8_t *mac, const char *ssid, int channel, int rssi, bool privacy,
                      bool fix, double lat, double lon, double alt, const char *when);
  void setEnabled(bool e);              // persists to NVS
  bool enabled()     const { return _enabled; }
  bool scanning()    const { return _scanning; }
  uint32_t logged()  const { return _logged; }       // total rows written
  int  lastTotal()   const { return _lastScanTotal; }// APs seen in last scan
  int  newLast()     const { return _newThisScan; }  // new (not recently seen)
  const String &csvPath() const { return _csvPath; }

private:
  bool     _enabled = false;
  bool     _sdReady = false;
  bool     _scanning = false;
  uint32_t _lastScanAt = 0;
  uint32_t _logged = 0;
  int      _lastScanTotal = 0;
  int      _newThisScan = 0;
  String   _csvPath;

  // recent-BSSID ring (dedupe so we don't re-log the same AP every few seconds)
  uint64_t _ring[WARDRIVE_DEDUP_RING];
  int      _ringHead = 0;
  int      _ringCount = 0;

  bool seen(uint64_t bssid);
  void remember(uint64_t bssid);
  void process(int n, bool haveFix, double lat, double lon, double alt,
               const char *whenStr);
  void writeHeader();
};

extern Wardriver wardriver;
