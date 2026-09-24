#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Bounds-checked beacon/probe-response security metadata for passive WiGLE rows.
// A privacy bit alone does not identify a cipher; retain UNKNOWN in that case.
namespace WifiSecurity {
inline uint16_t le16(const uint8_t *p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
inline bool akm(const uint8_t *p,size_t n,bool &psk,bool &eap,bool &sae) {
  if(n<8 || le16(p)!=1)return false;
  size_t at=6,count=le16(p+at);at+=2;
  if(count>(n-at)/4)return false;
  at+=count*4;
  if(n-at<2)return false;
  count=le16(p+at);at+=2;
  if(count>(n-at)/4)return false;
  for(size_t i=0;i<count;++i,at+=4) {
    if(p[at]!=0 || p[at+1]!=0x0f || p[at+2]!=0xac)continue;
    uint8_t type=p[at+3];
    psk=psk || type==2 || type==4 || type==6;
    eap=eap || type==1 || type==3 || type==5 || type==11 || type==12 || type==13;
    sae=sae || type==8 || type==9;
  }
  return true;
}
inline const char *capabilities(const uint8_t *p,size_t n) {
  if(n<36)return "[UNKNOWN][ESS]";
  bool privacy=(p[34]&0x10)!=0,wpa=false,rsn=false,psk=false,eap=false,sae=false;
  for(size_t at=36;at<n;) {
    if(n-at<2)return "[UNKNOWN][ESS]";
    uint8_t id=p[at++],len=p[at++];if(len>n-at)return "[UNKNOWN][ESS]";
    if(id==48) {if(!akm(p+at,len,psk,eap,sae))return "[UNKNOWN][ESS]";rsn=true;}
    if(id==221 && len>=6 && !memcmp(p+at,"\x00\x50\xf2\x01\x01\x00",6))wpa=true;
    at+=len;
  }
  if(sae && psk)return "[WPA2-PSK][WPA3-SAE][ESS]";
  if(sae)return "[WPA3-SAE][ESS]";
  if(eap)return "[WPA2-EAP][ESS]";
  if(wpa && psk)return "[WPA][WPA2-PSK][ESS]";
  if(psk)return "[WPA2-PSK][ESS]";
  if(rsn)return "[RSN][ESS]";
  if(wpa)return "[WPA][ESS]";
  return privacy?"[UNKNOWN][ESS]":"[ESS]";
}
}
