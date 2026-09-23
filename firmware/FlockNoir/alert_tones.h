#pragma once
#include <stdint.h>
#include <string.h>
#include "radio_protocol.h"

// Stable IDs are persisted separately from the user's existing RTTTL library.
namespace AlertTones {
enum Kind : uint8_t { Combined, Ir, Ble, Wifi, Camera, Axon, Ring, Meta, Flipper, Pineapple, Drone, Other, Biscuit, Count };
struct Preset { const char *id; const char *name; const char *rtttl; };
static constexpr Preset presets[] = {
  {"retro", "Retro blaster", "Blaster:d=16,o=6,b=180:c,g,c7,p,a#,g,8c7"},
  {"siren", "Two-tone siren", "Siren:d=8,o=6,b=150:c,g,c,g,c,g"},
  {"confused", "Confused", "Hmm:d=8,o=5,b=170:g,c6,f#,4d#"},
  {"ring", "Question chime", "Door:d=8,o=6,b=170:e,c,16p,f#,4d#"},
  {"flipper", "Digital warble", "Warble:d=16,o=6,b=170:c,f#,c7,f#,c,g"},
  {"pineapple", "Network warning", "Network:d=16,o=5,b=160:g,g,p,c6,c6,p,f#6"},
  {"drone", "Radar", "Radar:d=16,o=6,b=160:c,p,g,p,c7,p,g"},
  {"chirp", "Triple chirp", "Chirp:d=32,o=7,b=200:c,p,c,p,c"},
  {"silent", "Silent", ""},
};
static constexpr const char *ids[] = {"alpr_combined","alpr_ir","alpr_ble","alpr_wifi","camera","axon","ring","meta","flipper","pineapple","drone","other","biscuit"};
static constexpr const char *labels[] = {"ALPR radio + optical","ALPR pulse / OPT101","ALPR / Flock BLE","ALPR / Flock Wi-Fi","Camera pulse candidate","Axon candidate","Ring candidate","Meta glasses","Flipper Zero","Wi-Fi Pineapple","Drone","Other watchlist devices","Biscuit candidate"};
static constexpr const char *defaults[] = {"retro","retro","retro","retro","retro","siren","ring","confused","flipper","pineapple","drone","chirp","flipper"};
inline const Preset *preset(const char *id) {
  for(const auto &p:presets)if(!strcmp(p.id,id))return &p;
  return nullptr;
}
inline int kind(const char *id) {
  for(int i=0;i<Count;++i)if(!strcmp(ids[i],id))return i;
  return -1;
}
inline int slot(const char *id) {
  // Slots are bounded to single digits (current library limit: 5).
  return strlen(id)==6 && !strncmp(id,"slot:",5) && id[5]>='0' && id[5]<='9' ? id[5]-'0' : -1;
}
inline bool valid(const char *id,int count) {int s=slot(id);return preset(id) || (s>=0 && s<count);}
inline Kind classify(const RadioProtocol::Match &m,bool ble,bool ir,bool camera) {
  if(m.alpr)return m.tier>=2 && (ir || camera)?Combined:(ble?Ble:Wifi);
  if(RadioProtocol::contains(m.category,"Axon"))return Axon;
  if(RadioProtocol::contains(m.category,"Ring"))return Ring;
  if(RadioProtocol::contains(m.category,"Meta"))return Meta;
  if(RadioProtocol::contains(m.category,"Flipper"))return Flipper;
  if(RadioProtocol::contains(m.category,"Biscuit"))return Biscuit;
  if(RadioProtocol::contains(m.category,"Pineapple"))return Pineapple;
  if(RadioProtocol::contains(m.category,"Drone") || RadioProtocol::contains(m.category,"DJI") ||
     RadioProtocol::contains(m.category,"Parrot") || RadioProtocol::contains(m.category,"Skydio"))return Drone;
  return Other;
}

// Coalesce repeats of the same class without replacing another device's sound.
// ALPR has priority, with a bounded wait and no melody restarts on every packet.
class Queue {
  uint16_t pending=0,played=0;
  uint32_t requested[Count]={},last[Count]={};
public:
  void clear() {pending=0;}
  void request(Kind k,uint32_t now) {
    if(k>=Count || (played&(1u<<k) && now-last[k]<4000))return;
    if(!(pending&(1u<<k)))requested[k]=now;
    pending|=1u<<k;
  }
  bool take(uint32_t now,bool muted,bool enabled,bool busy,Kind &out) {
    if(muted || !enabled)clear();
    for(int i=0;i<Count;++i) {
      if(!(pending&(1u<<i)))continue;
      if(now-requested[i]>30000){pending&=~(1u<<i);continue;}
      if(busy)continue;
      pending&=~(1u<<i);played|=1u<<i;last[i]=now;out=Kind(i);return true;
    }
    return false;
  }
};
}
