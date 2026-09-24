#include "wardriver.h"
#include "radio.h"
#include <WiFi.h>
#include <SD.h>
#include <esp_wifi.h>

Wardriver wardriver;
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
    default:                        return "[UNKNOWN][ESS]";
  }
}

void Wardriver::begin(bool sdReady) {
  _sdReady=sdReady;
  if(!_sdReady)return;
  if(!SD.exists(WARDRIVE_DIR) && !SD.mkdir(WARDRIVE_DIR)){_sdReady=false;++_errors;return;}
  char path[64];snprintf(path,sizeof(path),"%s/wigle_%08lx.csv",WARDRIVE_DIR,(unsigned long)esp_random());
  _csvPath=path;
  File f=SD.open(_csvPath,FILE_WRITE);
  String header="WigleWifi-1.4,appRelease=" WIGLE_APP_RELEASE ",model=XIAO ESP32S3 Sense,release=" WIGLE_APP_RELEASE
    ",device=FlockNoir,display=,board=ESP32S3,brand=Seeed,accuracy=HDOPx5mEstimate\n";
  header+=Wigle::columns;header+='\n';
  if(!f || f.print(header)!=header.length()){_sdReady=false;++_errors;}
}
void Wardriver::setEnabled(bool enabled) {
  if(_enabled==enabled)return;
  _enabled=enabled;_recent.clear();_fix=false;
  if(_scanning){esp_wifi_scan_stop();WiFi.scanDelete();_scanning=false;}
  // The scan profile is the single persisted mode; there is no second wardrive toggle.
}
void Wardriver::update(bool fix,double lat,double lon,double alt,double accuracy,const char *when) {
  _fix=fix;_lat=lat;_lon=lon;_alt=alt;_accuracy=accuracy;strlcpy(_when,when,sizeof(_when));
  if(!_enabled)return;
  if(radio.field()){_scanning=false;return;}
  int state=WiFi.scanComplete();
  if(state==WIFI_SCAN_RUNNING){_scanning=true;return;}
  if(state>=0){_scanning=false;process(state);WiFi.scanDelete();_lastScanAt=millis();return;}
  if(WiFi.softAPgetStationNum()==0 && millis()-_lastScanAt>=WARDRIVE_SCAN_MS) {
    _scanning=WiFi.scanNetworks(true,true,true,120)==WIFI_SCAN_RUNNING;
    _lastScanAt=millis();
  }
}
void Wardriver::process(int count) {
  _lastScanTotal=count;_newThisScan=0;
  for(int i=0;i<count;++i) {
    const uint8_t *mac=WiFi.BSSID(i);if(!mac)continue;
    observe(mac,WiFi.SSID(i).c_str(),authToWigle(WiFi.encryptionType(i)),WiFi.channel(i),WiFi.RSSI(i),false,millis());
  }
}
void Wardriver::observe(const uint8_t *mac,const char *name,const char *auth,int channel,int rssi,bool ble,uint32_t observed) {
  if(!_enabled)return;
  if(!_fix || millis()-observed>GPS_MAX_AGE_MS){++_noFix;return;}
  if(!_sdReady || _recent.seen(mac,ble,observed))return;
  char row[512];size_t n=Wigle::row(row,sizeof(row),mac,name,auth,_when,channel,rssi,_lat,_lon,_alt,_accuracy,ble);
  if(!n){++_errors;return;}
  File f=SD.open(_csvPath,FILE_APPEND);
  if(!f || f.write((const uint8_t*)row,n)!=n){++_errors;return;}
  _recent.remember(mac,ble,observed);++_logged;++_newThisScan;
  if(ble)++_bleLogged;else ++_wifiLogged;
}
