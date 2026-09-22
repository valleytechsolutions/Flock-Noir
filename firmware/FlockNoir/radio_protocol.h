#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>

// Independent parsers; signature provenance and limitations: docs/RADIO.md.
namespace RadioProtocol {
inline uint16_t u16(const uint8_t *p) { return p[0] | (uint16_t(p[1]) << 8); }
inline int32_t i32(const uint8_t *p) {
  return int32_t(uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24);
}
inline void label(char *out, size_t cap, const uint8_t *p, size_t n) {
  size_t len = n < cap - 1 ? n : cap - 1;
  for (size_t i = 0; i < len; ++i) out[i] = p[i] >= 32 && p[i] < 127 ? char(p[i]) : ' ';
  out[len] = 0;
  while(len && out[len-1]==' ') out[--len]=0;
}
inline bool contains(const char *s, const char *part) {
  for (; *s; ++s) {
    size_t i = 0;
    while (s[i] && part[i] && tolower((unsigned char)s[i]) == tolower((unsigned char)part[i])) ++i;
    if (!part[i]) return true;
  }
  return false;
}
inline uint32_t prefix(const uint8_t *m) { return uint32_t(m[0]) << 16 | uint32_t(m[1]) << 8 | m[2]; }
inline bool flockPrefix(const uint8_t *m) {
  static const uint32_t prefixes[] = {
    0x70c94e,0x3c9180,0xd8f3bc,0x803049,0xb83532,0x145afc,0x744ca1,0x083a88,
    0x9c2f9d,0xc03532,0x940853,0xe4aaea,0xf46add,0xe00af6,0x24b2b9,0x00f48d,
    0xd03957,0xe8d0fc,0xe04f43,0xb81ea4,0x700894,0x588e81,0xec1bbd,0x3c71bf,
    0x5800e3,0x9035ea,0x5c93a2,0x646e69,0x4827ea,0xa4cf12,0x14b5cd,0x826bf2,
    0xb41e52,0x00037f
  };
  if (m[0] & 1) return false;
  for (auto p : prefixes) if (prefix(m) == p) return true;
  return false;
}
inline const char *vendor(const uint8_t *m) {
  struct Entry { uint32_t oui; const char *name; };
  static const Entry entries[] = {
    {0xb41e52,"Flock"},{0x0025df,"Axon"},
    {0x187f88,"Ring"},{0x242bd6,"Ring"},{0x343ea4,"Ring"},{0x54e019,"Ring"},
    {0x5c475e,"Ring"},{0x649a63,"Ring"},{0x90486c,"Ring"},{0x9c7613,"Ring"},
    {0xac9fc3,"Ring"},{0xc4dbad,"Ring"},{0xcc3bfb,"Ring"},
    {0x0c9ae6,"DJI"},{0x8c5823,"DJI"},{0x04a85a,"DJI"},{0x58b858,"DJI"},
    {0xe47a2c,"DJI"},{0x60601f,"DJI"},{0x481cb9,"DJI"},{0x34d262,"DJI"},
    {0x00121c,"Parrot"},{0x00267e,"Parrot"},{0x9003b7,"Parrot"},{0x903ae6,"Parrot"},
    {0xa0143d,"Parrot"},{0x381d14,"Skydio"}
  };
  if (m[0] & 1) return "";
  for (const auto &e : entries) if (prefix(m) == e.oui) return e.name;
  return "";
}
struct Match { const char *category = ""; const char *method = ""; uint8_t tier = 0; bool alpr = false; };
struct Advert {
  char name[64] = {};
  uint16_t company[8] = {}, services[32] = {};
  size_t companies = 0, serviceCount = 0;
  bool flockService = false, malformed = false;
  const uint8_t *remote = nullptr; size_t remoteLen = 0;
  bool hasCompany(uint16_t v) const { for (size_t i=0;i<companies;++i) if(company[i]==v)return true; return false; }
  bool hasService(uint16_t v) const { for (size_t i=0;i<serviceCount;++i) if(services[i]==v)return true; return false; }
};
inline Advert advert(const uint8_t *p, size_t n) {
  Advert a;
  const uint8_t flockUuid[] = {0x6f,0x2e,0x17,0xdf,0x14,0x18,0xe5,0x9f,0xa8,0x46,0x32,0x95,0x38,0xbb,0xcc,0xe8};
  for (size_t pos=0;pos<n;) {
    size_t len=p[pos++]; if (!len) break;
    if(len>n-pos) { a.malformed=true; break; }
    uint8_t type=p[pos]; const uint8_t *v=p+pos+1; size_t size=len-1;
    if(type==8 || type==9) label(a.name,sizeof(a.name),v,size);
    if(type==0xff && size>=2 && a.companies<8) a.company[a.companies++]=u16(v);
    if(type==2 || type==3 || type==0x16) {
      size_t count=type==0x16 ? (size>=2 ? 2 : 0) : size;
      for(size_t i=0;i+1<count && a.serviceCount<32;i+=2) a.services[a.serviceCount++]=u16(v+i);
      if(type==0x16 && size>=4 && u16(v)==0xfffa && v[2]==0x0d) { a.remote=v+4; a.remoteLen=size-4; }
    }
    if(type==6 || type==7) for(size_t i=0;i+16<=size;i+=16)
      if(!memcmp(v+i,flockUuid,16)) a.flockService=true;
    pos+=len;
  }
  return a;
}
inline Match bleMatch(const Advert &a, const uint8_t *mac, bool publicAddress) {
  if(a.malformed) return {};
  if((a.hasCompany(0x0d53) && a.hasService(0xfd5f)) || contains(a.name,"ray-ban") ||
     contains(a.name,"wayfarer") || contains(a.name,"oakley meta")) return {"Meta glasses","meta_composite",3,false};
  if(a.flockService) return {"Flock accessory","service_uuid128",3,true};
  if(contains(a.name,"penguin-") || contains(a.name,"fs ext battery") || contains(a.name,"flock"))
    return {"Flock candidate","name",2,true};
  if(a.hasCompany(0x09c8)) return {"Xuntong candidate","company_id",1,true};
  for(size_t i=0;i<a.serviceCount;++i) if(a.services[i]>=0x3100 && a.services[i]<=0x3500)
    return {"Raven candidate","service_range",1,true};
  if(a.hasCompany(0x034d) || a.hasService(0xfc81)) return {"Axon candidate","company_or_service",2,false};
  const char *v=publicAddress ? vendor(mac) : "";
  if(*v) return {v,"public_oui",1,!strcmp(v,"Flock")};
  return {};
}
struct Wifi {
  bool valid=false, beacon=false, wildcard=false, fingerprint=false;
  char ssid[33]={};
  const uint8_t *remote=nullptr; size_t remoteLen=0;
};
inline Wifi wifi(const uint8_t *p,size_t n) {
  Wifi w; if(n<24 || (p[0]&3)!=0) return w;
  w.valid=true;
  if((p[0]&0x0c)!=0) return w;
  uint8_t sub=p[0]&0xf0;
  if(sub==0xd0) {
    const uint8_t nanHeader[]={4,9,0x50,0x6f,0x9a,0x13};
    const uint8_t service[]={0x88,0x69,0x19,0x9d,0x92,9};
    if(n<30 || memcmp(p+24,nanHeader,6))return w;
    for(size_t at=30;at<n;) {
      if(n-at<3){w.valid=false;return w;}
      uint8_t id=p[at];size_t len=u16(p+at+1);at+=3;
      if(len>n-at){w.valid=false;return w;}
      if(id==3 && len>=11 && !memcmp(p+at,service,6) && p[at+8]==0x10 &&
         size_t(p[at+9])+10==len && p[at+9]>=1) {
        w.remote=p+at+11;w.remoteLen=p[at+9]-1;
      }
      at+=len;
    }
    return w;
  }
  w.beacon=sub==0x80 || sub==0x50;
  size_t pos=w.beacon ? 36 : 24;
  if(sub!=0x40 && !w.beacon) return w;
  if(n<pos) {w.valid=false;return w;}
  const uint8_t expected[]={2,12,127,221,45,191,221};
  const uint8_t v1[]={0x50,0x6f,0x9a,0x16,3,1,3},v2[]={0,0x50,0xf2,8,0,0,0};
  size_t seq=0; bool exact=true;
  while(pos<n) {
    if(n-pos<2) {w.valid=false;return w;}
    uint8_t id=p[pos++],len=p[pos++];
    if(len>n-pos) {w.valid=false;return w;}
    const uint8_t *v=p+pos;
    if(id==0) { label(w.ssid,sizeof(w.ssid),v,len); w.wildcard=len==0; }
    else {
      if(seq>=sizeof(expected) || id!=expected[seq]) exact=false;
      if(seq==3 && (len!=sizeof(v1) || memcmp(v,v1,sizeof(v1)))) exact=false;
      if(seq==6 && (len!=sizeof(v2) || memcmp(v,v2,sizeof(v2)))) exact=false;
      ++seq;
    }
    if(id==221 && len>=5 && v[3]==0x0d &&
       ((v[0]==0xfa && v[1]==0x0b && v[2]==0xbc) || (v[0]==0x90 && v[1]==0x3a && v[2]==0xe6))) {
      w.remote=v+5; w.remoteLen=len-5;
    }
    pos+=len;
  }
  w.fingerprint=exact && seq==sizeof(expected) && w.wildcard && sub==0x40;
  return w;
}
// Decode all six ODID message types. Authentication bytes are retained in capture;
// receiving an authentication message is NOT cryptographic verification.
struct Drone {
  uint8_t types=0;
  char id[21]={},operatorId[21]={},description[24]={};
  double lat=NAN,lon=NAN,pilotLat=NAN,pilotLon=NAN;
  float altitude=NAN,speed=NAN,heading=NAN;
};
inline bool droneMessage(const uint8_t *p,size_t n,Drone &d) {
  if(n!=25 || (p[0]&15)>2 || (p[0]>>4)>5) return false;
  uint8_t t=p[0]>>4;
  if(t==0) label(d.id,sizeof(d.id),p+2,20);
  if(t==1) {
    double lat=i32(p+5)/1e7,lon=i32(p+9)/1e7;
    if(fabs(lat)<=90 && fabs(lon)<=180 && (lat!=0 || lon!=0)) {d.lat=lat;d.lon=lon;}
    uint16_t alt=u16(p+15); d.altitude=alt ? alt*0.5f-1000 : NAN;
    d.speed=p[3]==255 ? NAN : ((p[1]&1) ? p[3]*0.75f+63.75f : p[3]*0.25f);
    d.heading=p[2]<180 ? p[2]+((p[1]&2)?180:0) : NAN;
  }
  if(t==3) label(d.description,sizeof(d.description),p+2,23);
  if(t==4) {
    double lat=i32(p+2)/1e7,lon=i32(p+6)/1e7;
    if(fabs(lat)<=90 && fabs(lon)<=180 && (lat!=0 || lon!=0)) {d.pilotLat=lat;d.pilotLon=lon;}
  }
  if(t==5) label(d.operatorId,sizeof(d.operatorId),p+2,20);
  d.types|=1<<t; return true;
}
inline bool drone(const uint8_t *p,size_t n,Drone &d) {
  if(!p || !n) return false;
  if((p[0]>>4)!=15) return droneMessage(p,n,d);
  if(n<3 || (p[0]&15)>2 || p[1]!=25 || p[2]==0 || p[2]>9 || n!=size_t(3+25*p[2])) return false;
  Drone candidate=d;
  for(size_t i=0;i<p[2];++i) if(!droneMessage(p+3+i*25,25,candidate)) return false;
  d=candidate; return true;
}
} // namespace RadioProtocol
