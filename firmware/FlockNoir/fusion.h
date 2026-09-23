#pragma once
#include <stdint.h>
#include "evidence.h"

// Temporal co-occurrence, never identity or direction finding. One bounded
// timestamp per input; no allocations in acquisition tasks or radio callbacks.
namespace AlprFusion {
static constexpr uint32_t WINDOW_MS=3000;
enum Source : uint8_t { Ir, Camera, Ble, Wifi, Count };
struct Snapshot {
  uint8_t mask=0,radioTier=0;
  int32_t age[Count]={-1,-1,-1,-1};
  bool has(Source s) const {return mask & (1u<<s);}
  const char *method() const {return detectionMethod(has(Ir),has(Camera),has(Ble),has(Wifi));}
  const char *assessment() const {
    if(radioTier>=2 && has(Ir) && has(Camera))return "multiple_sources_nearby";
    if(radioTier>=2 && (has(Ir) || has(Camera)))return "corroborated_camera_candidate";
    if(has(Ir) || has(Camera))return "possible_camera";
    if(radioTier>=3)return "camera_signature_match";
    return mask?"possible_camera":"no_recent_evidence";
  }
};
class Tracker {
  struct Stamp {uint32_t at=0;uint8_t tier=0;bool valid=false;} stamps[Count];
public:
  void clear() {for(auto &s:stamps)s=Stamp();}
  void observe(Source source,uint32_t at,uint8_t tier=0) {
    if(source>=Count)return;
    auto &s=stamps[source];
    // A delayed queue item must not replace more recent evidence.
    if(s.valid && int32_t(at-s.at)<0)return;
    s={at,tier,true};
  }
  Snapshot snapshot(uint32_t now) const {
    Snapshot out;
    for(int i=0;i<Count;++i) {
      const auto &s=stamps[i];
      // Symmetric distance also handles optical events queued just before radio.
      uint32_t age=uint32_t(now-s.at),reverse=uint32_t(s.at-now);
      if(reverse<age)age=reverse;
      if(!s.valid || age>WINDOW_MS)continue;
      out.age[i]=int32_t(age);out.mask|=1u<<i;
      if(i>=Ble && s.tier>out.radioTier)out.radioTier=s.tier;
    }
    return out;
  }
};
}
