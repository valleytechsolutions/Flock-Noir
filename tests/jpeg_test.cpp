#include "jpeg_luma.h"
#include <assert.h>
#include <stdio.h>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

static std::vector<uint8_t> read(const std::string &path) {
  std::ifstream file(path,std::ios::binary);assert(file.good());
  return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
int main() {
  JpegLuma decoder;uint8_t out[48];std::mt19937 rng(82);
  for(const char *name:{"gray","444","422","420","optimized","restart"}) {
    std::string base=std::string("tests/fixtures/jpeg/")+name;
    auto jpeg=read(base+".jpg"),reference=read(base+".luma");
    assert(decoder.decode(jpeg.data(),jpeg.size(),64,48,out,sizeof(out)));
    for(size_t i=0;i<reference.size();++i)assert(abs(int(out[i])-reference[i])<=3);
    assert(!decoder.decode(jpeg.data(),jpeg.size(),64,48,out,sizeof(out)-1));
    assert(!decoder.decode(jpeg.data(),jpeg.size(),32,48,out,sizeof(out)));
    for(size_t n=0;n<jpeg.size();++n)assert(!decoder.decode(jpeg.data(),n,64,48,out,sizeof(out)));
    for(int i=0;i<1000;++i) {
      auto corrupt=jpeg;
      for(int j=0;j<5;++j)corrupt[rng()%corrupt.size()]=rng();
      decoder.decode(corrupt.data(),corrupt.size(),64,48,out,sizeof(out));
    }
  }
  auto progressive=read("tests/fixtures/jpeg/progressive.jpg");
  assert(!decoder.decode(progressive.data(),progressive.size(),64,48,out,sizeof(out)));
  assert(!decoder.decode(nullptr,100,64,48,out,sizeof(out)));
  puts("JPEG block luminance, subsampling, restart, truncation and mutation tests passed");
}
