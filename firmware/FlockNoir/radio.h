#pragma once
#include <Arduino.h>
#include <atomic>
#include <freertos/queue.h>
#include "radio_protocol.h"
#include "radio_alert.h"
#include "fusion.h"

struct RadioObservation {
  uint32_t ms=0;
  uint16_t length=0, original=0;
  int8_t rssi=0;
  uint8_t channel=0, kind=0, addressType=0, eventType=0, mac[6]={};
  uint8_t data[768]={};
};
class Radio {
public:
  void begin(bool sdReady);
  void update(bool fix,double lat,double lon,double alt,const char *iso,bool ir,bool camera,bool muted);
  void enqueue(const RadioObservation &observation);
  bool configure(const String &mode,bool ble,bool capture,const String &watch,const String &target,int channel,const String &hop);
  String json() const;
  String rowsJson() const;
  String alertJson(bool alprOnly=false) const;
  String fusionJson(uint32_t now) const;
  AlprFusion::Snapshot fusion(uint32_t now) const {return _fusion.snapshot(now);}
  void optical(bool ir,uint32_t at) {_fusion.observe(ir?AlprFusion::Ir:AlprFusion::Camera,at);}
  bool setProfile(const String &profile);
  const char *profile() const {return _alprFocus?"alpr":"general";}
  uint32_t events() const {return _events;}
  bool field() const { return _field; }
  bool recentAlpr(uint32_t now) const;
  RadioEvidence nearbyAlpr(uint32_t now) const;
  const String &logPath() const { return _logPath; }
  const String &pcapPath() const { return _pcapPath; }
  const String &blePath() const { return _blePath; }
  void clearLive();
private:
  struct Device {
    bool used=false,ble=false,alpr=false,corroborated=false;
    uint8_t mac[6]={},tier=0;
    int rssi=-127;
    uint32_t seen=0,logged=0,count=0;
    uint32_t matched=0;
    RadioEncounter encounter;
    char name[64]={},category[32]={},method[32]={};
    RadioProtocol::Drone drone;
  };
  Device _devices[64];
  AlprFusion::Tracker _fusion;
  bool _alprFocus=false;
  uint32_t _events=0,_alertRequests=0;
  uint32_t _irAt=0,_cameraAt=0;
  QueueHandle_t _queue=nullptr;
  std::atomic<uint32_t> _dropped{0},_packets{0};
  bool _sd=false,_field=false,_desiredField=false,_ble=true,_capture=false,_wifiReady=false;
  bool _bleReady=false,_bleScanning=false;
  bool _allChannels=false;
  uint8_t _channel=1,_dashboardChannel=1;
  uint32_t _switchAt=0,_hopAt=0,_retryAt=0,_trackBeep=0,_bootPressed=0,_lastAlpr=0;
  uint32_t _logErrors=0,_pcapBytes=0,_bleBytes=0;
  String _watch,_target,_logPath,_pcapPath,_blePath;
  void process(const RadioObservation &o,bool fix,double lat,double lon,double alt,const char *iso,bool ir,bool camera,bool muted);
  void observe(const RadioObservation &o,const uint8_t *mac,const char *name,RadioProtocol::Match match,
               const RadioProtocol::Drone &drone,bool fix,double lat,double lon,const char *iso,bool ir,bool camera,bool muted);
  void capture(const RadioObservation &o);
  RadioProtocol::Match watchMatch(const uint8_t *mac,const char *name,const RadioProtocol::Advert *a) const;
  bool startWifi();
};
extern Radio radio;
String jsonQuote(const String &s);
String jsonNumber(double value,int places=6);
