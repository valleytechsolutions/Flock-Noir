#pragma once
#include <Arduino.h>
#include "config.h"
#include "wigle_format.h"

class Wardriver {
public:
  void begin(bool sdReady);
  void update(bool haveFix,double lat,double lon,double alt,double accuracy,const char *when);
  void observe(const uint8_t *mac,const char *name,const char *auth,int channel,int rssi,bool ble,uint32_t observed);
  void setEnabled(bool enabled);
  bool enabled() const {return _enabled;}
  bool scanning() const {return _scanning;}
  uint32_t logged() const {return _logged;}
  uint32_t wifiLogged() const {return _wifiLogged;}
  uint32_t bleLogged() const {return _bleLogged;}
  uint32_t errors() const {return _errors;}
  uint32_t noFix() const {return _noFix;}
  int lastTotal() const {return _lastScanTotal;}
  int newLast() const {return _newThisScan;}
  const String &csvPath() const {return _csvPath;}
private:
  bool _enabled=false,_sdReady=false,_scanning=false,_fix=false;
  uint32_t _lastScanAt=0,_logged=0,_wifiLogged=0,_bleLogged=0,_errors=0,_noFix=0;
  int _lastScanTotal=0,_newThisScan=0;
  double _lat=0,_lon=0,_alt=0,_accuracy=0;
  char _when[24]={};
  String _csvPath;
  Wigle::Recent<WARDRIVE_DEDUP_RING> _recent;
  void process(int count);
};
extern Wardriver wardriver;
