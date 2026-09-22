#include "pulse_detector.h"
#include "radio_protocol.h"
#include "detector.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <random>
using namespace RadioProtocol;
static void train(PulseDetector &p,uint32_t &us,int period=100,int width=20,int duration=1500) {
  for(int ms=0;ms<duration;++ms) {us+=1000;p.feed(200+((ms%period)<width?650:0),us);}
}
int main() {
  Detector camera;
  for(int blob:{100,19200}) {
    camera.begin(19200);
    for(int i=0;i<300;++i)camera.feed(i%5==0?240:20,i%5==0?blob:0,i*20000u);
    assert(camera.analyze().detected==(blob==100));
    for(int i=300;i<325;++i)camera.feed(20,0,i*20000u);
    assert(!camera.analyze().detected);
  }
  PulseDetector p;uint32_t us=0;
  for(int i=0;i<300;++i){us+=1000;p.feed(200,us);}
  train(p,us);assert(p.matched);assert(p.frequency>9.8 && p.frequency<10.2);
  assert(p.duty>=.10 && p.duty<=.30);
  for(int i=0;i<400;++i){us+=1000;p.feed(200,us);}assert(!p.matched);
  train(p,us);assert(p.matched);
  us+=20000;p.feed(200,us);assert(!p.matched && p.gaps==1);
  train(p,us);assert(p.matched);us+=1000;p.feed(4095,us);assert(!p.matched && p.clipped);
  for(int period:{10,20,50,200}) {p.reset();train(p,us,period,period/5);assert(!p.matched);}
  for(int width:{1,5,50,80}) {p.reset();train(p,us,100,width);assert(!p.matched);}
  p.reset();us=0xffff0000;for(int i=0;i<300;++i){us+=1000;p.feed(200,us);}train(p,us);assert(p.matched);
  p.reset();std::mt19937 rng(1234);
  for(int i=0;i<20000;++i){us+=1000;p.feed(200+rng()%35,us);assert(!p.matched);}
  uint8_t mac[]={0xb4,0x1e,0x52,1,2,3};assert(flockPrefix(mac));
  uint8_t group[]={0xff,0xff,0xff,0xff,0xff,0xff};assert(!flockPrefix(group));
  uint8_t local[]={0x82,0x6b,0xf2,0,0,0};assert(flockPrefix(local));
  const uint8_t cid[]={3,0xff,0x53,0x0d},svc[]={3,3,0x5f,0xfd};
  assert(!bleMatch(advert(cid,sizeof(cid)),group,true).tier);
  assert(!bleMatch(advert(svc,sizeof(svc)),group,true).tier);
  uint8_t composite[]={3,0xff,0x53,0x0d,3,3,0x5f,0xfd};
  assert(bleMatch(advert(composite,sizeof(composite)),group,false).tier==3);
  assert(!bleMatch(advert(nullptr,0),mac,false).tier);
  uint8_t malformed[]={5,0xff,0xc8,9};assert(advert(malformed,sizeof(malformed)).malformed);
  assert(!bleMatch(advert(malformed,sizeof(malformed)),mac,true).tier);
  std::vector<uint8_t> probe(24);probe[0]=0x40;
  probe.insert(probe.end(),{0,0,2,0,12,0,127,0,221,7,0x50,0x6f,0x9a,0x16,3,1,3,45,0,191,0,221,7,0,0x50,0xf2,8,0,0,0});
  assert(wifi(probe.data(),probe.size()).fingerprint);
  probe.back()=1;assert(!wifi(probe.data(),probe.size()).fingerprint);
  probe.pop_back();assert(!wifi(probe.data(),probe.size()).valid);
  uint8_t id[25]={2,0x10,'T','E','S','T'};Drone d;
  assert(drone(id,sizeof(id),d));assert(!strcmp(d.id,"TEST"));
  assert(!drone(id,24,d));
  uint8_t pack[28]={0xf2,25,1};memcpy(pack+3,id,25);assert(drone(pack,sizeof(pack),d));
  pack[2]=2;assert(!drone(pack,sizeof(pack),d));
  uint8_t location[25]={0x12};
  int32_t lat=400000000,lon=-750000000;
  memcpy(location+5,&lat,4);memcpy(location+9,&lon,4);
  location[15]=0xd0;location[16]=7; // encoded altitude 2000 -> 0 m
  location[2]=90;location[3]=20;
  assert(drone(location,sizeof(location),d));
  assert(d.lat==40 && d.lon==-75 && d.altitude==0 && d.heading==90 && d.speed==5);
  location[3]=255;location[15]=location[16]=0;
  assert(drone(location,sizeof(location),d));assert(isnan(d.speed) && isnan(d.altitude));
  pack[2]=1;
  std::vector<uint8_t> nan(30);nan[0]=0xd0;
  const uint8_t nh[]={4,9,0x50,0x6f,0x9a,0x13};memcpy(nan.data()+24,nh,6);
  nan.insert(nan.end(),{3,39,0,0x88,0x69,0x19,0x9d,0x92,9,1,0,0x10,29,0});
  nan.insert(nan.end(),pack,pack+28);
  auto parsed=wifi(nan.data(),nan.size());assert(parsed.valid && drone(parsed.remote,parsed.remoteLen,d));
  // Deterministic malformed/truncated packet corpus under address sanitizer.
  for(int i=0;i<5000;++i) {
    std::vector<uint8_t> bytes(rng()%800);for(auto &v:bytes)v=rng();
    advert(bytes.data(),bytes.size());wifi(bytes.data(),bytes.size());Drone x;drone(bytes.data(),bytes.size(),x);
  }
  puts("camera, pulse, radio signatures, Remote ID and malformed-packet tests passed");
}
