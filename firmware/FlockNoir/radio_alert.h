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
