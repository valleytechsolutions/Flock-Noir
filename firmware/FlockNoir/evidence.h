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
  static constexpr const char *methods[]={"unknown","ir","camera","ir+camera",
    "ble","ir+ble","camera+ble","ir+camera+ble","wifi","ir+wifi","camera+wifi",
    "ir+camera+wifi","ble+wifi","ir+ble+wifi","camera+ble+wifi","ir+camera+ble+wifi"};
  return methods[(ir?1:0)|(camera?2:0)|(ble?4:0)|(wifi?8:0)];
}
