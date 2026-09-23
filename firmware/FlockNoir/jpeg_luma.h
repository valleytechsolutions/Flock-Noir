#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// This hot path must finish within one VGA frame interval even in textured
// scenes. Optimize the decoder independently of Arduino's size-first defaults.
#if defined(ARDUINO_ARCH_ESP32) && defined(__GNUC__)
#pragma GCC push_options
#pragma GCC optimize ("O3")
#endif

// Baseline JPEG block-average luminance, implemented from ITU-T T.81.
// DC * quantizer / 8 + 128 is the mean of an 8x8 Y block. Consume AC codes
// without IDCT/color conversion. This is a brightness sensor, not a renderer.
// Supports the OV2640's interleaved YCbCr JPEG, plus grayscale test fixtures.
// Reject unsupported/malformed input; no allocation, recursion or unbounded loops.
class JpegLuma {
  struct Huffman {
    uint16_t fast[256],first[17],base[17];
    uint8_t count[17],value[256];
    bool valid;
    bool build(const uint8_t *p,size_t n,size_t &used) {
      valid=false;used=0;if(n<16)return false;
      memset(fast,0,sizeof(fast));
      uint32_t code=0;unsigned total=0;
      for(unsigned bits=1;bits<=16;++bits) {
        count[bits]=p[bits-1];first[bits]=uint16_t(code);base[bits]=total;
        if(code+count[bits]>(1u<<bits) || total+count[bits]>256)return false;
        total+=count[bits];code=(code+count[bits])<<1;
      }
      if(!total || n<16+total)return false;
      memcpy(value,p+16,total);
      for(unsigned bits=1;bits<=8;++bits)for(unsigned i=0;i<count[bits];++i) {
        unsigned start=(first[bits]+i)<<(8-bits),end=start+(1u<<(8-bits));
        for(unsigned j=start;j<end;++j)fast[j]=uint16_t(bits<<8)|value[base[bits]+i];
      }
      used=16+total;valid=true;return true;
    }
  } tables[2][4];
  struct Bits {
    const uint8_t *p;size_t n,pos=0;
    uint32_t cache=0;unsigned available=0;bool bad=false;
    bool fill(unsigned wanted) {
      while(available<wanted && pos<n) {
        uint8_t byte=p[pos];
        if(byte==255) {
          if(pos+1>=n || p[pos+1]!=0)break; // leave marker for restart/end check
          ++pos;
        }
        ++pos;cache=(cache<<8)|byte;available+=8;
      }
      return available>=wanted;
    }
    unsigned get(unsigned nbits) {
      if(!nbits)return 0;
      if(nbits>16 || !fill(nbits)){bad=true;return 0;}
      available-=nbits;return (cache>>available)&((1u<<nbits)-1);
    }
    bool marker(uint8_t expected) {
      // Entropy padding must be ones, with at most seven bits before a marker.
      if(available>7 || (available && (cache&((1u<<available)-1))!=((1u<<available)-1)))return false;
      available=0;cache=0;
      if(pos>=n || p[pos++]!=255)return false;
      while(pos<n && p[pos]==255)++pos;
      return pos<n && p[pos++]==expected;
    }
  };
  struct Component {uint8_t id,h,v,q,dc,ac;int predictor;};
  static uint16_t word(const uint8_t *p){return uint16_t(p[0])<<8|p[1];}
  static int symbol(Bits &bits,const Huffman &table) {
    if(!table.valid)return -1;
    unsigned code=0,length=1;
    if(bits.fill(8)) {
      code=(bits.cache>>(bits.available-8))&255;
      uint16_t fast=table.fast[code];
      if(fast){bits.available-=fast>>8;return fast&255;}
      // A lookup miss already proves the code is longer than eight bits.
      // Consume that prefix once instead of rereading it one bit at a time.
      bits.available-=8;length=9;
    }
    for(;length<=16;++length) {
      code=(code<<1)|bits.get(1);if(bits.bad)return -1;
      if(code>=table.first[length] && code-table.first[length]<table.count[length])
        return table.value[table.base[length]+code-table.first[length]];
    }
    return -1;
  }
  bool block(Bits &bits,Component &c) {
    int size=symbol(bits,tables[0][c.dc]);if(size<0 || size>11)return false;
    int value=int(bits.get(size));if(bits.bad)return false;
    if(size && value<(1<<(size-1)))value-=(1<<size)-1;
    c.predictor+=value;if(c.predictor<-32768 || c.predictor>32767)return false;
    for(unsigned coefficient=1;coefficient<64;) {
      int code=symbol(bits,tables[1][c.ac]);if(code<0)return false;
      unsigned run=unsigned(code)>>4,magnitude=code&15;
      if(!magnitude) {
        if(!run)return true;
        if(run!=15 || coefficient+16>64)return false;
        coefficient+=16;
      } else {
        if(magnitude>10 || coefficient+run>=64)return false;
        coefficient+=run+1;bits.get(magnitude);if(bits.bad)return false;
      }
    }
    return true;
  }
public:
  bool decode(const uint8_t *jpeg,size_t n,uint16_t width,uint16_t height,uint8_t *out,size_t capacity) {
    if(!jpeg || !out || n<4 || n>131072 || !width || !height || width>640 || height>480 ||
       word(jpeg)!=0xffd8 || capacity<size_t((width+7)/8)*((height+7)/8))return false;
    for(auto &kind:tables)for(auto &table:kind)table.valid=false;
    Component components[3]={};uint8_t quant[4]={},order[3]={};
    unsigned count=0;uint16_t restart=0;size_t pos=2;bool scan=false;
    while(pos+4<=n) {
      if(jpeg[pos++]!=255)return false;
      while(pos<n && jpeg[pos]==255)++pos;
      if(pos+3>n)return false;
      uint8_t marker=jpeg[pos++];size_t length=word(jpeg+pos);
      if(length<2 || length>n-pos)return false;
      const uint8_t *p=jpeg+pos+2;size_t size=length-2;pos+=length;
      if(marker==0xdb) {
        for(size_t i=0;i<size;) {
          uint8_t table=p[i++];if(table>3 || size-i<64 || !p[i])return false;
          quant[table]=p[i];i+=64;
        }
      } else if(marker==0xc4) {
        for(size_t i=0;i<size;) {
          uint8_t table=p[i++];size_t used=0;
          if((table>>4)>1 || (table&15)>3 || !tables[table>>4][table&15].build(p+i,size-i,used))return false;
          i+=used;
        }
      } else if(marker==0xc0) {
        if(count || size<6 || p[0]!=8 || word(p+1)!=height || word(p+3)!=width)return false;
        count=p[5];if((count!=1 && count!=3) || size!=6+3*count)return false;
        unsigned blocks=0;
        for(unsigned i=0;i<count;++i) {
          auto &c=components[i];c.id=p[6+i*3];c.h=p[7+i*3]>>4;c.v=p[7+i*3]&15;c.q=p[8+i*3];
          if(c.id!=i+1 || !c.h || c.h>2 || !c.v || c.v>2 || c.q>3 ||
             (i && (c.h!=1 || c.v!=1)))return false;
          blocks+=c.h*c.v;
        }
        if(blocks>6 || (count==1 && blocks!=1))return false;
      } else if(marker==0xdd) {
        if(size!=2)return false;
        restart=word(p);
      } else if(marker==0xda) {
        if(!count || size!=4+2*count || p[0]!=count || p[size-3]!=0 || p[size-2]!=63 || p[size-1]!=0)return false;
        unsigned seen=0;
        for(unsigned i=0;i<count;++i) {
          unsigned k=0;while(k<count && components[k].id!=p[1+i*2])++k;
          if(k==count || (seen&(1u<<k)))return false;
          seen|=1u<<k;order[i]=k;auto &c=components[k];
          c.dc=p[2+i*2]>>4;c.ac=p[2+i*2]&15;
          if(c.dc>3 || c.ac>3 || !quant[c.q] || !tables[0][c.dc].valid || !tables[1][c.ac].valid)return false;
        }
        scan=true;break;
      } else if(!((marker>=0xe0 && marker<=0xef) || marker==0xfe))return false;
    }
    if(!scan)return false;
    Bits bits{jpeg+pos,n-pos};
    unsigned cols=(width+7)/8,rows=(height+7)/8;
    unsigned h=components[0].h,v=components[0].v,mcu=0,nextRestart=0;
    for(unsigned y=0;y<rows;y+=v)for(unsigned x=0;x<cols;x+=h) {
      if(restart && mcu && mcu%restart==0) {
        if(!bits.marker(uint8_t(0xd0+(nextRestart++&7))))return false;
        for(auto &c:components)c.predictor=0;
      }
      for(unsigned i=0;i<count;++i) {
        unsigned k=order[i];auto &c=components[k];
        for(unsigned by=0;by<c.v;++by)for(unsigned bx=0;bx<c.h;++bx) {
          if(!block(bits,c))return false;
          if(!k && y+by<rows && x+bx<cols) {
            int luminance=128+(c.predictor*quant[c.q])/8;
            out[(y+by)*cols+x+bx]=uint8_t(luminance<0?0:luminance>255?255:luminance);
          }
        }
      }
      ++mcu;
    }
    return !bits.bad && bits.marker(0xd9);
  }
};
#if defined(ARDUINO_ARCH_ESP32) && defined(__GNUC__)
#pragma GCC pop_options
#endif
