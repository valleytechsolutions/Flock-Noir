#include "radio.h"
#include "config.h"
#include "wardriver.h"
#include "buzzer.h"
#include <WiFi.h>
#include <SD.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <host/ble_hs.h>
#include <host/ble_gap.h>

Radio radio;
static std::atomic<bool> bleReady{false},bleScanning{false},bleBusy{false};
static std::atomic<bool> bleFault{false};
static std::atomic<bool> captureAll{false},customWatch{false};
static constexpr uint32_t CAPTURE_LIMIT=16*1024*1024;
String jsonQuote(const String &s) {
  String out="\"";
  for(size_t i=0;i<s.length();++i) {
    unsigned char c=s[i];
    if(c=='"' || c=='\\') {out+='\\';out+=char(c);}
    else if(c<32 || c>=127) { char hex[7]; snprintf(hex,sizeof(hex),"\\u%04x",c);out+=hex; }
    else out+=char(c);
  }
  return out+'"';
}
String jsonNumber(double v,int places) { return isfinite(v) ? String(v,places) : String("null"); }
static String macString(const uint8_t *m) {
  char s[18]; snprintf(s,sizeof(s),"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]); return s;
}
static void wifiReceive(void *buf,wifi_promiscuous_pkt_type_t type) {
  if(!buf || (type!=WIFI_PKT_MGMT && type!=WIFI_PKT_DATA)) return;
  auto *p=static_cast<wifi_promiscuous_pkt_t *>(buf);
  if(p->rx_ctrl.rx_state || p->rx_ctrl.sig_len<28) return;
  if(type==WIFI_PKT_DATA && !captureAll && !customWatch &&
     !RadioProtocol::flockPrefix(p->payload+4) && !RadioProtocol::flockPrefix(p->payload+10) &&
     !*RadioProtocol::vendor(p->payload+10)) return;
  RadioObservation o;
  o.ms=millis(); o.original=p->rx_ctrl.sig_len-4; // omit FCS, including from PCAP
  o.length=min(size_t(o.original),sizeof(o.data));
  o.rssi=p->rx_ctrl.rssi;o.channel=p->rx_ctrl.channel;
  memcpy(o.data,p->payload,o.length); radio.enqueue(o);
}
static int bleEvent(ble_gap_event *event,void *) {
  if(event->type==BLE_GAP_EVENT_DISC_COMPLETE) {bleScanning=false;return 0;}
  if(event->type!=BLE_GAP_EVENT_DISC)return 0;
  const auto &p=event->disc;
  RadioObservation o;
  o.kind=1;o.ms=millis();o.rssi=p.rssi;o.addressType=p.addr.type;o.eventType=p.event_type;
  for(int i=0;i<6;++i)o.mac[i]=p.addr.val[5-i];
  o.length=min(size_t(p.length_data),sizeof(o.data));o.original=o.length;
  memcpy(o.data,p.data,o.length);radio.enqueue(o);return 0;
}
static void bleHost(void *) {nimble_port_run();nimble_port_freertos_deinit();}
static void bleSync() {bleReady=true;}
static void bleReset(int) {bleReady=false;bleScanning=false;bleFault=true;}
void Radio::enqueue(const RadioObservation &o) {
  ++_packets;
  if(!_queue || xQueueSend(_queue,&o,0)!=pdTRUE) ++_dropped;
}
bool Radio::startWifi() {
  wifi_promiscuous_filter_t filter={};
  filter.filter_mask=WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_DATA;
  return esp_wifi_set_promiscuous_filter(&filter)==ESP_OK &&
         esp_wifi_set_promiscuous_rx_cb(wifiReceive)==ESP_OK && esp_wifi_set_promiscuous(true)==ESP_OK;
}
void Radio::begin(bool sdReady) {
  _sd=sdReady;
  pinMode(RADIO_BOOT_PIN,INPUT_PULLUP);
  _queue=xQueueCreate(32,sizeof(RadioObservation));
  if(!_queue) {Serial.println("[RADIO] queue allocation failed");return;}
  Preferences prefs;
  if(prefs.begin("flockradio",true)) {
    _ble=prefs.getBool("ble",true);_watch=prefs.getString("watch","");_target=prefs.getString("target","");
    _dashboardChannel=prefs.getUChar("channel",1);prefs.end();
  }
  if(_dashboardChannel<1 || _dashboardChannel>11) _dashboardChannel=1;
  _channel=_dashboardChannel;
  customWatch=_watch.length()>0 || _target.length()>0;
  _switchAt=millis()+1; // restore the saved dashboard channel through the mode state machine
  _wifiReady=startWifi();
  if(!_wifiReady) Serial.println("[RADIO] promiscuous initialization failed; will retry");
  if(nimble_port_init()==ESP_OK) {
    ble_hs_cfg.sync_cb=bleSync;ble_hs_cfg.reset_cb=bleReset;
    nimble_port_freertos_init(bleHost);
  } else {bleFault=true;Serial.println("[BLE] host initialization failed");}
  if(_sd) {
    if(!SD.exists("/radio") && !SD.mkdir("/radio")) {_sd=false;++_logErrors;}
    char session[24];snprintf(session,sizeof(session),"%08lx",(unsigned long)esp_random());
    _logPath=String("/radio/events_")+session+".jsonl";
    _pcapPath=String("/radio/wifi_")+session+".pcap";
    _blePath=String("/radio/ble_")+session+".jsonl";
  }
}
static bool validHex(const String &s,int digits,bool colon) {
  int count=0;
  for(size_t i=0;i<s.length();++i) {
    if(colon && i%3==2) {if(s[i]!=':')return false;}
    else {if(!isxdigit((unsigned char)s[i]))return false;++count;}
  }
  return count==digits;
}
bool Radio::configure(const String &mode,bool ble,bool cap,const String &watch,const String &target,int channel) {
  if((mode!="field" && mode!="dashboard") || channel<1 || channel>11 || watch.length()>1024 ||
     (target.length() && (target.length()!=17 || !validHex(target,12,true)))) return false;
  for(int pos=0;pos<int(watch.length());) {
    int end=watch.indexOf('\n',pos);if(end<0)end=watch.length();
    String line=watch.substring(pos,end);line.trim();pos=end+1;if(!line.length())continue;
    int sep=line.indexOf(':');if(sep<0)return false;
    String type=line.substring(0,sep),v=line.substring(sep+1);v.trim();
    if(type=="oui") {if(v.length()!=8 || !validHex(v,6,true))return false;}
    else if(type=="mac") {if(v.length()!=17 || !validHex(v,12,true))return false;}
    else if(type=="cid" || type=="svc") {if(v.length()!=4 || !validHex(v,4,false))return false;}
    else if(type=="name") {if(!v.length() || v.length()>40)return false;}
    else return false;
  }
  Preferences prefs;
  if(!prefs.begin("flockradio",false))return false;
  bool ok=prefs.putBool("ble",ble)>0 && prefs.putString("watch",watch)==watch.length() &&
    prefs.putString("target",target)==target.length() && prefs.putUChar("channel",channel)>0;
  prefs.end();if(!ok)return false;
  _ble=ble;_capture=cap;_watch=watch;_target=target;_target.toUpperCase();
  captureAll=cap;customWatch=watch.length()>0 || target.length()>0;
  _desiredField=mode=="field";_dashboardChannel=channel;_switchAt=millis()+1200;
  return true;
}
RadioProtocol::Match Radio::watchMatch(const uint8_t *mac,const char *name,const RadioProtocol::Advert *a) const {
  String address=macString(mac);
  for(int pos=0;pos<int(_watch.length());) {
    int end=_watch.indexOf('\n',pos);if(end<0)end=_watch.length();
    String line=_watch.substring(pos,end);line.trim();pos=end+1;
    int sep=line.indexOf(':');String type=line.substring(0,sep),v=line.substring(sep+1);v.trim();
    if(type=="name" && RadioProtocol::contains(name,v.c_str()))return {"Watchlist","name",2,false};
    if(type=="cid" && a && a->hasCompany(strtoul(v.c_str(),nullptr,16)))return {"Watchlist","company_id",2,false};
    if(type=="svc" && a && a->hasService(strtoul(v.c_str(),nullptr,16)))return {"Watchlist","service_uuid",2,false};
    v.toUpperCase();
    if((type=="mac" && address==v) || (type=="oui" && address.startsWith(v)))return {"Watchlist",type=="mac"?"mac":"oui",1,false};
  }
  return {};
}
void Radio::capture(const RadioObservation &o) {
  if(!_capture || !_sd)return;
  if(o.kind) {
    if(_bleBytes>=CAPTURE_LIMIT)return;
    String hex;hex.reserve(o.length*2);
    for(size_t i=0;i<o.length;++i) {char b[3];snprintf(b,sizeof(b),"%02x",o.data[i]);hex+=b;}
    String row="{\"uptime_ms\":"+String(o.ms)+",\"mac\":"+jsonQuote(macString(o.mac))+
      ",\"rssi\":"+String(o.rssi)+",\"address_type\":"+String(o.addressType)+
      ",\"event_type\":"+String(o.eventType)+",\"advertisement_hex\":"+jsonQuote(hex)+"}\n";
    File f=SD.open(_blePath,FILE_APPEND);
    if(!f || f.print(row)!=row.length())++_logErrors;else _bleBytes+=row.length();
  } else {
    if(_pcapBytes>=CAPTURE_LIMIT)return;
    File f=SD.open(_pcapPath,FILE_APPEND);if(!f){++_logErrors;return;}
    // Little-endian PCAP, linktype 105 (raw 802.11); timestamps are boot-relative.
    if(!f.size()) {
      const uint32_t header[]={0xa1b2c3d4,0x00040002,0,0,768,105};
      if(f.write((const uint8_t*)header,sizeof(header))!=sizeof(header)){++_logErrors;return;}
      _pcapBytes+=sizeof(header);
    }
    uint32_t record[]={o.ms/1000,(o.ms%1000)*1000,o.length,o.original};
    if(f.write((uint8_t*)record,sizeof(record))!=sizeof(record) || f.write(o.data,o.length)!=o.length)++_logErrors;
    else _pcapBytes+=sizeof(record)+o.length;
  }
}
void Radio::observe(const RadioObservation &o,const uint8_t *mac,const char *name,RadioProtocol::Match match,
 const RadioProtocol::Drone &drone,bool fix,double lat,double lon,const char *iso,bool ir,bool camera,bool muted) {
  Device *d=nullptr,*oldest=&_devices[0];
  for(auto &candidate:_devices) {
    if(candidate.used && candidate.ble==bool(o.kind) && !memcmp(candidate.mac,mac,6)) {d=&candidate;break;}
    if(!candidate.used || millis()-candidate.seen>millis()-oldest->seen)oldest=&candidate;
  }
  if(!d) {d=oldest;*d=Device();d->used=true;d->ble=o.kind;memcpy(d->mac,mac,6);}
  bool newEvidence=match.tier>d->tier;
  d->seen=o.ms;d->rssi=o.rssi;++d->count;
  if(*name)strlcpy(d->name,name,sizeof(d->name));
  if(match.tier>=d->tier) {d->tier=match.tier;d->alpr=match.alpr;strlcpy(d->category,match.category,sizeof(d->category));strlcpy(d->method,match.method,sizeof(d->method));}
  if(drone.types) {
    if(*drone.id)strlcpy(d->drone.id,drone.id,sizeof(d->drone.id));
    if(*drone.operatorId)strlcpy(d->drone.operatorId,drone.operatorId,sizeof(d->drone.operatorId));
    if(*drone.description)strlcpy(d->drone.description,drone.description,sizeof(d->drone.description));
    if(drone.types&2){d->drone.lat=drone.lat;d->drone.lon=drone.lon;d->drone.altitude=drone.altitude;d->drone.speed=drone.speed;d->drone.heading=drone.heading;}
    if(drone.types&16){d->drone.pilotLat=drone.pilotLat;d->drone.pilotLon=drone.pilotLon;}
    d->drone.types|=drone.types;
  }
  if(match.alpr && match.tier>=2)_lastAlpr=o.ms;
  if(!match.tier || (!newEvidence && d->logged && o.ms-d->logged<10000))return;
  d->logged=o.ms;
  String row="{\"schema\":1,\"time\":"+jsonQuote(iso)+",\"uptime_ms\":"+String(o.ms)+
    ",\"protocol\":"+jsonQuote(o.kind?"ble":"wifi")+",\"mac\":"+jsonQuote(macString(mac))+
    ",\"name\":"+jsonQuote(d->name)+",\"category\":"+jsonQuote(match.category)+",\"method\":"+jsonQuote(match.method)+
    ",\"tier\":"+String(match.tier)+",\"rssi\":"+String(o.rssi)+",\"channel\":"+String(o.channel)+
    ",\"gps_valid\":"+(fix?"true":"false")+",\"lat\":"+jsonNumber(fix?lat:NAN)+",\"lon\":"+jsonNumber(fix?lon:NAN)+
    ",\"ir_timing_match\":"+(ir?"true":"false")+",\"camera_pattern\":"+(camera?"true":"false")+
    ",\"evidence\":"+jsonQuote(ir && match.alpr && match.tier>=2 ? "ir_radio_nearby":"radio_candidate")+
    ",\"drone_id\":"+jsonQuote(d->drone.id)+",\"operator_id\":"+jsonQuote(d->drone.operatorId)+
    ",\"drone_lat\":"+jsonNumber(d->drone.lat)+",\"drone_lon\":"+jsonNumber(d->drone.lon)+
    ",\"altitude_m\":"+jsonNumber(d->drone.altitude,1)+",\"speed_mps\":"+jsonNumber(d->drone.speed,1)+
    ",\"heading_deg\":"+jsonNumber(d->drone.heading,1)+",\"pilot_lat\":"+jsonNumber(d->drone.pilotLat)+
    ",\"pilot_lon\":"+jsonNumber(d->drone.pilotLon)+",\"odid_types\":"+String(d->drone.types)+"}";
  if(_sd) {File f=SD.open(_logPath,FILE_APPEND);if(!f || f.println(row)!=row.length()+2)++_logErrors;}
  if(Serial && Serial.availableForWrite()>int(row.length()+2))Serial.println(row);
  if(newEvidence && match.tier>=2 && !muted && !buzzer.isPlaying())buzzer.playAlert();
}
void Radio::process(const RadioObservation &o,bool fix,double lat,double lon,double alt,const char *iso,bool ir,bool camera,bool muted) {
  capture(o);
  fix = fix && millis()-o.ms <= GPS_MAX_AGE_MS;
  RadioProtocol::Drone drone;
  if(o.kind) {
    auto a=RadioProtocol::advert(o.data,o.length);
    auto match=RadioProtocol::bleMatch(a,o.mac,o.addressType==BLE_ADDR_PUBLIC);
    if(RadioProtocol::drone(a.remote,a.remoteLen,drone))match={"Drone Remote ID","ble_remote_id",3,false};
    if(!match.tier)match=watchMatch(o.mac,a.name,&a);
    observe(o,o.mac,a.name,match,drone,fix,lat,lon,iso,ir,camera,muted);
    return;
  }
  auto w=RadioProtocol::wifi(o.data,o.length);if(!w.valid)return;
  if(w.beacon && _field) {
    char when[24];strlcpy(when,iso,sizeof(when));
    if(strlen(when)>=19) {when[10]=' ';when[19]=0;}
    wardriver.observePassive(o.data+10,w.ssid,o.channel,o.rssi,(o.data[34]&0x10)!=0,fix,lat,lon,alt,when);
  }
  bool remote=RadioProtocol::drone(w.remote,w.remoteLen,drone);
  // addr1 is the receiver: its RSSI belongs to the transmitting frame, not that target.
  const int offsets[]={10,4,16};
  for(int i=0;i<3;++i) {
    if(i==2 && (o.data[0]&0x0c)!=0)continue;
    const uint8_t *mac=o.data+offsets[i];if(mac[0]&1)continue;
    RadioProtocol::Match m;
    if(i==0 && remote)m={"Drone Remote ID","wifi_remote_id",3,false};
    else if(RadioProtocol::flockPrefix(mac)) {
      m={"Flock candidate",i==0?"oui_addr2":(i==1?"oui_addr1":"oui_addr3"),uint8_t(i==0?2:1),true};
      if(i==0 && (o.data[0]&0xfc)==0x40 && w.wildcard)m={"Flock candidate",w.fingerprint?"wildcard_ie":"wildcard_probe",uint8_t(w.fingerprint?4:3),true};
    } else if(i==0) {
      const char *v=RadioProtocol::vendor(mac);if(*v)m={v,"oui_addr2",1,false};
      if(RadioProtocol::contains(w.ssid,"flock") || RadioProtocol::contains(w.ssid,"penguin"))m={"Flock candidate","ssid",2,true};
    }
    if(!m.tier)m=watchMatch(mac,w.ssid,nullptr);
    if(m.tier || (i==0 && _target==macString(mac))) observe(o,mac,w.ssid,m,drone,fix,lat,lon,iso,ir,camera,muted);
  }
}
void Radio::update(bool fix,double lat,double lon,double alt,const char *iso,bool ir,bool camera,bool muted) {
  uint32_t now=millis();
  if(!digitalRead(RADIO_BOOT_PIN)) {
    if(!_bootPressed)_bootPressed=now;
    if(_bootPressed!=UINT32_MAX && now-_bootPressed>=1500) {
      _desiredField=false;_switchAt=now;_bootPressed=UINT32_MAX;
    }
  } else _bootPressed=0;
  if(_switchAt && int32_t(now-_switchAt)>=0) {
    _switchAt=0;
    if(wardriver.scanning()) {esp_wifi_scan_stop();WiFi.scanDelete();}
    bool ok=WiFi.mode(_desiredField ? WIFI_STA : WIFI_AP_STA);
    if(!_desiredField) {
      const IPAddress ip(192,168,4,1);
      ok=WiFi.softAPConfig(ip,ip,IPAddress(255,255,255,0)) && ok;
      ok=WiFi.softAP(AP_SSID,AP_PASSWORD,_dashboardChannel) && ok;
    }
    if(ok) {_field=_desiredField;_channel=_field?1:_dashboardChannel;}
    if(esp_wifi_set_channel(_channel,WIFI_SECOND_CHAN_NONE)!=ESP_OK)ok=false;
    _wifiReady=startWifi() && ok;
    if(!ok) {Serial.println("[RADIO] mode change failed; retrying");_switchAt=now+2000;}
  }
  if(_field && now-_hopAt>=RADIO_DWELL_MS) {
    _hopAt=now;uint8_t next=_channel>=11?1:_channel+1;
    if(esp_wifi_set_channel(next,WIFI_SECOND_CHAN_NONE)==ESP_OK)_channel=next;
  }
  if(now-_retryAt>=2000) {
    _retryAt=now;
    if(!_wifiReady)_wifiReady=startWifi();
    if(bleReady && !bleBusy && _ble!=bleScanning.load()) {
      bleBusy=true;
      ble_gap_disc_params params={};
      params.passive=1;params.itvl=800;params.window=80;params.filter_duplicates=0;
      int err=_ble ? ble_gap_disc(BLE_OWN_ADDR_PUBLIC,BLE_HS_FOREVER,&params,bleEvent,nullptr) : ble_gap_disc_cancel();
      bleBusy=false;
      if(err==0) {bleScanning=_ble;bleFault=false;} else bleFault=true;
    }
  }
  _bleReady=bleReady;_bleScanning=bleScanning;
  RadioObservation o;
  for(int i=0;_queue && i<8 && xQueueReceive(_queue,&o,0)==pdTRUE;++i)
    process(o,fix,lat,lon,alt,iso,ir,camera,muted);
  for(const auto &d:_devices) if(d.used && _target==macString(d.mac) && now-d.seen<2500 &&
    strcmp(d.method,"oui_addr1") && strcmp(d.method,"oui_addr3")) {
    uint32_t gap=constrain((-d.rssi-30)*25,120,2000);
    if(!muted && now-_trackBeep>=gap && !buzzer.isPlaying()) {buzzer.play("track:d=32,o=6,b=200:c");_trackBeep=now;}
  }
}
bool Radio::recentAlpr(uint32_t now) const {return _lastAlpr && now-_lastAlpr<=RADIO_CORRELATE_MS;}
void Radio::clearLive() {for(auto &d:_devices)d=Device();_lastAlpr=0;}
String Radio::rowsJson() const {
  String s="[";bool first=true;
  for(const auto &d:_devices) if(d.used) {
    if(!first)s+=',';first=false;
    s+="{\"mac\":"+jsonQuote(macString(d.mac))+",\"protocol\":"+jsonQuote(d.ble?"BLE":"WiFi")+
      ",\"name\":"+jsonQuote(d.name)+",\"category\":"+jsonQuote(d.category)+",\"method\":"+jsonQuote(d.method)+
      ",\"tier\":"+String(d.tier)+",\"rssi\":"+String(d.rssi)+",\"ageMs\":"+String(millis()-d.seen)+
      ",\"count\":"+String(d.count)+",\"droneId\":"+jsonQuote(d.drone.id)+
      ",\"droneLat\":"+jsonNumber(d.drone.lat)+",\"droneLon\":"+jsonNumber(d.drone.lon)+"}";
  }
  return s+"]";
}
String Radio::json() const {
  return "{\"supported\":true,\"mode\":"+jsonQuote(_field?"field":"dashboard")+
    ",\"channel\":"+String(_channel)+",\"ble\":"+(_ble?"true":"false")+",\"bleScanning\":"+(_bleScanning?"true":"false")+
    ",\"wifiReady\":"+(_wifiReady?"true":"false")+",\"bleReady\":"+(_bleReady && !bleFault?"true":"false")+
    ",\"capture\":"+(_capture?"true":"false")+",\"packets\":"+String(_packets.load())+
    ",\"dropped\":"+String(_dropped.load())+",\"logErrors\":"+String(_logErrors)+
    ",\"freeHeap\":"+String(ESP.getFreeHeap())+",\"minFreeHeap\":"+String(ESP.getMinFreeHeap())+
    ",\"captureFull\":"+(_pcapBytes>=CAPTURE_LIMIT || _bleBytes>=CAPTURE_LIMIT?"true":"false")+
    ",\"watch\":"+jsonQuote(_watch)+",\"target\":"+jsonQuote(_target)+"}";
}
