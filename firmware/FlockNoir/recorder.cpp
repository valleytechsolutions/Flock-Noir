// =============================================================================
//  Flock Noir  -  recorder.cpp
//  MJPEG-AVI writer (+ optional PDM-mic WAV sidecar).
// =============================================================================
#include "recorder.h"
#include "img_converters.h"
#include <FS.h>
#include <SD.h>
#include <ESP_I2S.h>

Recorder recorder;
static I2SClass I2S_mic;
static uint8_t  audioBuf[1024];

// ---- little-endian writers --------------------------------------------------
static void wU32(File &f, uint32_t v) {
  uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
  f.write(b, 4);
}
static void wU16(File &f, uint16_t v) {
  uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
  f.write(b, 2);
}
static void wStr(File &f, const char *s) { f.write((const uint8_t *)s, 4); }

// Fixed byte offsets inside the 224-byte AVI header that we patch on stop.
static const uint32_t OFF_RIFFSIZE   = 4;
static const uint32_t OFF_MICROS     = 32;
static const uint32_t OFF_TOTFRAMES  = 48;
static const uint32_t OFF_SCALE      = 128;
static const uint32_t OFF_RATE       = 132;
static const uint32_t OFF_LENGTH     = 140;
static const uint32_t OFF_MOVISIZE   = 216;
static const uint32_t MOVI_DATA_POS  = 220;   // position of "movi" fourcc

// -----------------------------------------------------------------------------
void Recorder::begin(bool sdReady) {
  _sdReady = sdReady;
  if (_sdReady && !SD.exists(REC_DIR)) SD.mkdir(REC_DIR);
}

// ---- AVI ---------------------------------------------------------------------
void Recorder::writeAviHeader() {
  File &f = _avi;
  wStr(f, "RIFF"); wU32(f, 0); wStr(f, "AVI ");
  wStr(f, "LIST"); wU32(f, 192); wStr(f, "hdrl");
  wStr(f, "avih"); wU32(f, 56);
  wU32(f, 66666);          // dwMicroSecPerFrame (patched)
  wU32(f, 0);              // dwMaxBytesPerSec
  wU32(f, 0);              // dwPaddingGranularity
  wU32(f, 0x10);           // dwFlags = AVIF_HASINDEX
  wU32(f, 0);              // dwTotalFrames (patched)
  wU32(f, 0);              // dwInitialFrames
  wU32(f, 1);              // dwStreams
  wU32(f, 0);              // dwSuggestedBufferSize
  wU32(f, _w);             // dwWidth
  wU32(f, _h);             // dwHeight
  wU32(f, 0); wU32(f, 0); wU32(f, 0); wU32(f, 0);   // dwReserved[4]
  wStr(f, "LIST"); wU32(f, 116); wStr(f, "strl");
  wStr(f, "strh"); wU32(f, 56);
  wStr(f, "vids"); wStr(f, "MJPG");
  wU32(f, 0);              // dwFlags
  wU32(f, 0);              // wPriority + wLanguage
  wU32(f, 0);              // dwInitialFrames
  wU32(f, 1000);           // dwScale (patched)
  wU32(f, 15000);          // dwRate  (patched)
  wU32(f, 0);              // dwStart
  wU32(f, 0);              // dwLength (patched)
  wU32(f, 0);              // dwSuggestedBufferSize
  wU32(f, 0);              // dwQuality
  wU32(f, 0);              // dwSampleSize
  wU16(f, 0); wU16(f, 0); wU16(f, _w); wU16(f, _h);  // rcFrame
  wStr(f, "strf"); wU32(f, 40);
  wU32(f, 40);             // biSize
  wU32(f, _w);             // biWidth
  wU32(f, _h);             // biHeight
  wU16(f, 1);              // biPlanes
  wU16(f, 24);             // biBitCount
  wStr(f, "MJPG");         // biCompression
  wU32(f, (uint32_t)_w * _h * 3);   // biSizeImage
  wU32(f, 0); wU32(f, 0); wU32(f, 0); wU32(f, 0);   // ppm/clr
  wStr(f, "LIST"); wU32(f, 0); wStr(f, "movi");     // movi size patched
  _moviOffset = 4;         // first chunk sits right after the "movi" fourcc
}

void Recorder::addVideoFrame(camera_fb_t *fb) {
  if (!_active || !_avi || !fb) return;

  uint8_t *jpg = nullptr; size_t len = 0; bool allocated = false;
  if (fb->format == PIXFORMAT_JPEG) { jpg = fb->buf; len = fb->len; }
  else { if (!frame2jpg(fb, REC_JPEG_QUALITY, &jpg, &len)) return; allocated = true; }

  wStr(_avi, "00dc"); wU32(_avi, len);
  _avi.write(jpg, len);
  if (len & 1) { uint8_t z = 0; _avi.write(&z, 1); }   // pad to even

  if (_frameCount < _idxCap) { _idxOff[_frameCount] = _moviOffset; _idxSize[_frameCount] = len; }
  _moviOffset += 8 + len + (len & 1);
  _frameCount++;

  if (allocated) free(jpg);
  if ((_frameCount & 0x1F) == 0) _avi.flush();
  if (_frameCount >= _idxCap) stop();                  // hit the frame cap
}

void Recorder::finishAvi() {
  if (!_avi) return;
  // write idx1
  wStr(_avi, "idx1"); wU32(_avi, _frameCount * 16);
  for (uint32_t i = 0; i < _frameCount; i++) {
    wStr(_avi, "00dc"); wU32(_avi, 0x10);              // AVIIF_KEYFRAME
    wU32(_avi, _idxOff[i]); wU32(_avi, _idxSize[i]);
  }
  uint32_t fileSize = _avi.size();
  _avi.close();

  // playback rate from real duration
  float secs = (millis() - _startMs) / 1000.0f;
  float fps  = (secs > 0.1f && _frameCount > 0) ? (_frameCount / secs) : 15.0f;
  if (fps < 1) fps = 1;
  uint32_t micros = (uint32_t)(1000000.0f / fps);
  uint32_t rate   = (uint32_t)(fps * 1000.0f);

  // reopen for in-place patching of the header size/rate fields
  File pf = SD.open(_videoPath.c_str(), "r+");
  if (pf) {
    pf.seek(OFF_RIFFSIZE);  wU32(pf, fileSize - 8);
    pf.seek(OFF_MICROS);    wU32(pf, micros);
    pf.seek(OFF_TOTFRAMES); wU32(pf, _frameCount);
    pf.seek(OFF_SCALE);     wU32(pf, 1000);
    pf.seek(OFF_RATE);      wU32(pf, rate);
    pf.seek(OFF_LENGTH);    wU32(pf, _frameCount);
    pf.seek(OFF_MOVISIZE);  wU32(pf, _moviOffset);     // movi LIST data size
    pf.close();
  }
}

// ---- WAV / audio -------------------------------------------------------------
void Recorder::writeWavHeader() {
  File &f = _wav;                         // 44-byte placeholder, patched on stop
  wStr(f, "RIFF"); wU32(f, 0); wStr(f, "WAVE");
  wStr(f, "fmt "); wU32(f, 16);
  wU16(f, 1);                             // PCM
  wU16(f, 1);                             // mono
  wU32(f, MIC_SAMPLE_RATE);
  wU32(f, MIC_SAMPLE_RATE * 2);           // byte rate (mono 16-bit)
  wU16(f, 2);                             // block align
  wU16(f, 16);                            // bits/sample
  wStr(f, "data"); wU32(f, 0);            // data size (patched)
}

void Recorder::pumpAudio() {
  if (!_active || !_audioOn || !_wav) return;
  int avail = I2S_mic.available();
  if (avail <= 0) return;
  if (avail > (int)sizeof(audioBuf)) avail = sizeof(audioBuf);
  int n = I2S_mic.readBytes((char *)audioBuf, avail);
  if (n > 0) { _wav.write(audioBuf, n); _audioBytes += n; }
}

void Recorder::finishWav() {
  if (!_wav) return;
  uint32_t dataBytes = _audioBytes;
  File &f = _wav;
  f.flush();
  f.seek(4);  wU32(f, 36 + dataBytes);   // RIFF size
  f.seek(40); wU32(f, dataBytes);        // data size
  f.close();
  I2S_mic.end();
}

// ---- start / stop ------------------------------------------------------------
bool Recorder::start(bool withAudio) {
  if (_active || !_sdReady) return false;

  // We need one frame to learn the resolution before writing the header.
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return false;
  _w = fb->width; _h = fb->height;
  esp_camera_fb_return(fb);

  // index tables in PSRAM
  _idxCap  = REC_MAX_FRAMES;
  _idxOff  = (uint32_t *)ps_malloc(_idxCap * sizeof(uint32_t));
  _idxSize = (uint32_t *)ps_malloc(_idxCap * sizeof(uint32_t));
  if (!_idxOff || !_idxSize) { free(_idxOff); free(_idxSize); _idxOff = _idxSize = nullptr; return false; }

  char base[40];
  snprintf(base, sizeof(base), "%s/rec_%08lx", REC_DIR, (unsigned long)esp_random());
  _videoPath = String(base) + ".avi";
  _audioPath = String(base) + ".wav";

  _avi = SD.open(_videoPath.c_str(), FILE_WRITE);
  if (!_avi) { free(_idxOff); free(_idxSize); _idxOff = _idxSize = nullptr; return false; }
  _frameCount = 0; _moviOffset = 0; _audioBytes = 0;
  writeAviHeader();

  _audioOn = false;
  if (withAudio) {
    I2S_mic.setPinsPdmRx(MIC_CLK_PIN, MIC_DATA_PIN);
    if (I2S_mic.begin(I2S_MODE_PDM_RX, MIC_SAMPLE_RATE,
                      I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
      _wav = SD.open(_audioPath.c_str(), FILE_WRITE);
      if (_wav) { writeWavHeader(); _audioOn = true; }
      else I2S_mic.end();
    }
  }

  _startMs = millis();
  _active = true;
  Serial.printf("[REC] start %s%s\n", _videoPath.c_str(), _audioOn ? " (+audio)" : "");
  return true;
}

void Recorder::stop() {
  if (!_active) return;
  _active = false;                        // stop intake first
  finishAvi();
  if (_audioOn) { finishWav(); _audioOn = false; }
  free(_idxOff); free(_idxSize); _idxOff = _idxSize = nullptr;
  Serial.printf("[REC] stop, %lu frames\n", (unsigned long)_frameCount);
}
