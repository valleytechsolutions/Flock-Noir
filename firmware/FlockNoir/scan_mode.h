#pragma once
#include <stdint.h>
#include <string.h>

namespace ScanMode {
enum Mode : uint8_t { General, Alpr, Axon, Wardrive };
inline const char *name(Mode mode) {
  const char *names[]={"general","alpr","axon","wardrive"};
  return names[mode<=Wardrive?mode:General];
}
inline bool parse(const char *value,Mode &mode) {
  for(int i=General;i<=Wardrive;++i)if(!strcmp(value,name(Mode(i)))) {mode=Mode(i);return true;}
  return false;
}
inline bool optical(Mode mode) {return mode==Alpr;}
inline bool wifi(Mode mode) {return mode!=Axon;}
inline bool accepts(Mode mode,bool ble,bool alpr,bool axon) {
  return mode==General || (mode==Alpr && alpr) || (mode==Axon && ble && axon);
}
// BLE timing units are 0.625 ms. Pig Detector gives reception most radio time;
// the dashboard still shares the radio. Field mode removes that contention.
inline uint16_t interval(Mode mode) {return mode==Axon?160:320;}
inline uint16_t window(Mode mode,bool field) {return mode==Axon?(field?160:144):160;}

// Global reminder pacing avoids one siren per advertisement / per nearby MAC.
struct Reminder {
  uint32_t last=0;
  bool sounded=false;
  bool due(uint32_t now,uint32_t matched,bool audible,uint32_t repeatMs) {
    if(!audible || now-matched>3000 || (sounded && now-last<repeatMs))return false;
    last=now;sounded=true;return true;
  }
};
}
