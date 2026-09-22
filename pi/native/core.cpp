// Shared XIAO/Pi pulse and packet parsers, exposed through a small C ABI.
#include "pulse_detector.h"
#include "radio_protocol.h"
#include <new>
#include <string>

#ifdef _WIN32
#define FN_EXPORT extern "C" __declspec(dllexport)
#else
#define FN_EXPORT extern "C"
#endif

using namespace RadioProtocol;
static std::string quote(const char *s) {
  std::string out="\"";
  for (; *s; ++s) {
    unsigned char c=*s;
    if(c=='"' || c=='\\') {out+='\\';out+=char(c);}
    else if(c<32 || c>=127) {char h[7];snprintf(h,sizeof(h),"\\u%04x",c);out+=h;}
    else out+=char(c);
  }
  return out+'"';
}
static std::string number(double n) {
  if(!isfinite(n)) return "null";
  char s[64];snprintf(s,sizeof(s),"%.7f",n);return s;
}
static std::string mac(const uint8_t *m) {
  char s[18];snprintf(s,sizeof(s),"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]);return s;
}
static int output(const std::string &s,char *out,size_t cap) {
  if(!out || s.size()+1>cap) return -1;
  memcpy(out,s.c_str(),s.size()+1);return int(s.size());
}
static std::string droneJson(const Drone &d) {
  return "{\"types\":"+std::to_string(d.types)+",\"id\":"+quote(d.id)+
    ",\"operatorId\":"+quote(d.operatorId)+",\"description\":"+quote(d.description)+
    ",\"lat\":"+number(d.lat)+",\"lon\":"+number(d.lon)+
    ",\"pilotLat\":"+number(d.pilotLat)+",\"pilotLon\":"+number(d.pilotLon)+
    ",\"altitude\":"+number(d.altitude)+",\"speed\":"+number(d.speed)+
    ",\"heading\":"+number(d.heading)+"}";
}
static std::string matchJson(const Match &m,const uint8_t *address,int slot) {
  return "{\"mac\":"+quote(mac(address).c_str())+",\"slot\":"+std::to_string(slot)+
    ",\"category\":"+quote(m.category)+",\"method\":"+quote(m.method)+
    ",\"tier\":"+std::to_string(m.tier)+",\"alpr\":"+(m.alpr?"true":"false")+"}";
}
FN_EXPORT int fn_decode_wifi(const uint8_t *p,size_t n,char *out,size_t cap) {
  if(!p || n>4096) return -1;
  const auto w=wifi(p,n);
  if(!w.valid)return output("{\"valid\":false}",out,cap);
  Drone d;bool remote=drone(w.remote,w.remoteLen,d);
  std::string s="{\"valid\":true,\"name\":"+quote(w.ssid)+
    ",\"beacon\":"+(w.beacon?"true":"false")+
    ",\"privacy\":"+((w.beacon && (p[34]&0x10))?"true":"false")+
    ",\"drone\":"+droneJson(d)+",\"rows\":[";
  const int offsets[]={10,4,16};bool first=true;
  for(int i=0;i<3;++i) {
    if(i==2 && (p[0]&0x0c))continue;
    const uint8_t *m=p+offsets[i];if(m[0]&1)continue;
    Match match;
    if(i==0 && remote)match={"Drone Remote ID","wifi_remote_id",3,false};
    else if(flockPrefix(m)) {
      match={"Flock candidate",i==0?"oui_addr2":(i==1?"oui_addr1":"oui_addr3"),uint8_t(i==0?2:1),true};
      if(i==0 && (p[0]&0xfc)==0x40 && w.wildcard)
        match={"Flock candidate",w.fingerprint?"wildcard_ie":"wildcard_probe",uint8_t(w.fingerprint?4:3),true};
    } else if(i==0) {
      const char *v=vendor(m);if(*v)match={v,"oui_addr2",1,false};
      if(contains(w.ssid,"flock") || contains(w.ssid,"penguin"))match={"Flock candidate","ssid",2,true};
    }
    if(!first)s+=',';
    first=false;s+=matchJson(match,m,i);
  }
  return output(s+"]}",out,cap);
}
FN_EXPORT int fn_decode_ble(const uint8_t *p,size_t n,const uint8_t *address,int publicAddress,char *out,size_t cap) {
  if(!p || !address || n>4096)return -1;
  auto a=advert(p,n);
  if(a.malformed)return output("{\"valid\":false}",out,cap);
  auto m=bleMatch(a,address,publicAddress!=0);Drone d;
  if(drone(a.remote,a.remoteLen,d))m={"Drone Remote ID","ble_remote_id",3,false};
  std::string s="{\"valid\":true,\"name\":"+quote(a.name)+",\"drone\":"+droneJson(d)+
    ",\"rows\":["+matchJson(m,address,0)+"],\"companies\":[";
  for(size_t i=0;i<a.companies;++i){if(i)s+=',';s+=std::to_string(a.company[i]);}
  s+="],\"services\":[";
  for(size_t i=0;i<a.serviceCount;++i){if(i)s+=',';s+=std::to_string(a.services[i]);}
  return output(s+"]}",out,cap);
}
FN_EXPORT void *fn_pulse_new() {return new(std::nothrow) PulseDetector;}
FN_EXPORT int fn_decode_survey(const uint8_t *address,const char *ssid,char *out,size_t cap) {
  if(!address || !ssid || strlen(ssid)>128)return -1;
  Match match;
  if(flockPrefix(address))match={"Flock candidate","survey_oui",2,true};
  else {const char *v=vendor(address);if(*v)match={v,"survey_oui",1,false};}
  if(contains(ssid,"flock") || contains(ssid,"penguin"))match={"Flock candidate","survey_ssid",2,true};
  return output("{\"valid\":true,\"name\":"+quote(ssid)+",\"drone\":"+droneJson(Drone())+
                ",\"rows\":["+matchJson(match,address,0)+"]}",out,cap);
}
FN_EXPORT void fn_pulse_free(void *p) {delete static_cast<PulseDetector*>(p);}
FN_EXPORT void fn_pulse_reset(void *p) {if(p)static_cast<PulseDetector*>(p)->reset();}
FN_EXPORT void fn_pulse_configure(void *p,const float *f,const uint32_t *u) {
  if(!p || !f || !u)return;
  auto &c=static_cast<PulseDetector*>(p)->config;
  c.minHz=f[0];c.maxHz=f[1];c.minDuty=f[2];c.maxDuty=f[3];
  c.minWidthMs=f[4];c.maxWidthMs=f[5];c.threshold=f[6];c.release=f[7];c.noiseMultiplier=f[8];
  c.required=u[0];c.staleUs=u[1];c.maxGapUs=u[2];
}
FN_EXPORT void fn_pulse_feed(void *p,uint16_t raw,uint32_t us) {
  if(p)static_cast<PulseDetector*>(p)->feed(raw,us);
}
FN_EXPORT void fn_pulse_read(void *p,float *f,uint32_t *u) {
  if(!p || !f || !u)return;
  const auto &d=*static_cast<PulseDetector*>(p);
  f[0]=d.baseline;f[1]=d.noise;f[2]=d.amplitude;f[3]=d.frequency;f[4]=d.duty;f[5]=d.widthMs;
  u[0]=d.valid;u[1]=d.gaps;u[2]=d.matched;u[3]=d.clipped;
}
