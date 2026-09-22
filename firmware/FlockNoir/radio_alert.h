#pragma once
#include <stdint.h>

// One audible notification per encounter or stronger signature. Reappearance
// after 60 seconds is a new encounter. Unsigned differences survive millis wrap.
struct RadioEncounter {
  uint32_t seen=0;
  uint8_t tier=0;
  bool observe(uint8_t strength,uint32_t now) {
    bool fresh=!tier || now-seen>=60000;
    bool notify=fresh || strength>tier;
    tier=fresh?strength:(strength>tier?strength:tier);seen=now;
    return notify;
  }
};
struct RadioAlertGate {
  bool pending=false,played=false;
  uint32_t requested=0,last=0;
  void request(uint32_t now) {pending=true;requested=now;}
  bool take(uint32_t now,bool muted,bool enabled,bool busy) {
    if(muted || !enabled || (pending && now-requested>8000))pending=false;
    if(!pending || busy || (played && now-last<4000))return false;
    pending=false;played=true;last=now;return true;
  }
};
