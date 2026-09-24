#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

namespace Wigle {
constexpr const char *columns="MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type";
// SSIDs are up to 32 bytes; BLE names up to 63. Always quote textual fields.
inline bool quote(char *out,size_t capacity,const char *value) {
  size_t n=0;
  if(capacity<3)return false;
  out[n++]='"';
  for(const char *p=value;*p;++p) {
    if(n+(*p=='"'?2:1)+2>capacity)return false;
    if(*p=='"')out[n++]='"';
    // WiGLE's importer treats CR/LF as record delimiters, even inside quotes.
    out[n++]=(*p=='\r' || *p=='\n')?' ':*p;
  }
  out[n++]='"';out[n]=0;return true;
}
inline size_t row(char *out,size_t capacity,const uint8_t *mac,const char *name,const char *auth,
                  const char *when,int channel,int rssi,double lat,double lon,double alt,
                  double accuracy,bool ble) {
  if(!isfinite(lat) || !isfinite(lon) || !isfinite(alt) || !isfinite(accuracy) || accuracy<=0 ||
     lat<-90 || lat>90 || lon<-180 || lon>180 || strlen(when)!=19 || when[10]!=' ')return 0;
  char escaped[132],capabilities[132];
  if(!quote(escaped,sizeof(escaped),name) || !quote(capabilities,sizeof(capabilities),auth))return 0;
  int n=snprintf(out,capacity,"%02X:%02X:%02X:%02X:%02X:%02X,%s,%s,%s,%d,%d,%.7f,%.7f,%.1f,%.1f,%s\n",
    mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],escaped,capabilities,when,ble?0:channel,rssi,
    lat,lon,alt,accuracy,ble?"BLE":"WIFI");
  return n>0 && size_t(n)<capacity?size_t(n):0;
}
// Repeat measurements at new locations; keep WiFi and BLE address spaces distinct.
template<size_t N> class Recent {
  struct Entry {uint64_t id=0;uint32_t at=0;bool used=false;};
  Entry entries[N];size_t head=0;
  static uint64_t key(const uint8_t *mac,bool ble) {
    uint64_t id=ble?1:0;for(int i=0;i<6;++i)id=(id<<8)|mac[i];return id;
  }
public:
  bool seen(const uint8_t *mac,bool ble,uint32_t now) const {
    uint64_t id=key(mac,ble);
    for(const auto &e:entries)if(e.used && e.id==id && now-e.at<15000)return true;
    return false;
  }
  void remember(const uint8_t *mac,bool ble,uint32_t now) {
    uint64_t id=key(mac,ble);
    for(auto &e:entries)if(e.used && e.id==id){e.at=now;return;}
    entries[head]={id,now,true};head=(head+1)%N;
  }
  void clear(){for(auto &e:entries)e=Entry();head=0;}
};
}
