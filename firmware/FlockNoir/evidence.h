#pragma once
#include "radio_protocol.h"

struct RadioEvidence {
  bool found=false,ble=false;
  RadioProtocol::Match match;
  char mac[18]={};
  int rssi=-127;
};

// These describe observed sensors, not proof that they belong to one device.
inline const char *detectionMethod(bool ir,bool camera,bool ble,bool wifi) {
  if(ir && camera && ble)return "ir+camera+ble";
  if(ir && camera && wifi)return "ir+camera+wifi";
  if(ir && ble)return "ir+ble";
  if(ir && wifi)return "ir+wifi";
  if(camera && ble)return "camera+ble";
  if(camera && wifi)return "camera+wifi";
  if(ir && camera)return "ir+camera";
  return ir?"ir":camera?"camera":ble?"ble":wifi?"wifi":"unknown";
}
