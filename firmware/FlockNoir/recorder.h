// =============================================================================
//  Flock Noir  -  recorder.h
//  Records the live (grayscale/NIR) camera feed to SD as an MJPEG AVI, with
//  optional audio from the onboard PDM mic written to a matching WAV sidecar.
//  Non-blocking: video frames are pushed from the main loop (reusing the frame
//  already grabbed for detection); audio is drained from the I2S DMA each loop.
// =============================================================================
#pragma once
#include <Arduino.h>
#include <FS.h>
#include "esp_camera.h"
#include "config.h"

class Recorder {
public:
  void begin(bool sdReady);
  // Start recording. withAudio -> also open the mic + WAV. Returns false on error.
  bool start(bool withAudio);
  void stop();

  bool     active()   const { return _active; }
  bool     audioOn()  const { return _audioOn; }
  uint32_t frames()   const { return _frameCount; }
  uint32_t seconds()  const { return _active ? (millis() - _startMs) / 1000 : 0; }
  const String &videoPath() const { return _videoPath; }

  // Called from loop():
  void addVideoFrame(camera_fb_t *fb);  // encode+append one AVI frame
  void pumpAudio();                     // drain I2S -> WAV (safe if audio off)

private:
  bool     _sdReady   = false;
  bool     _active    = false;
  bool     _audioOn   = false;
  File     _avi;
  File     _wav;
  String   _videoPath;
  String   _audioPath;
  uint16_t _w = 0, _h = 0;
  uint32_t _frameCount = 0;
  uint32_t _moviOffset = 0;             // running offset within movi data
  uint32_t _startMs = 0;
  uint32_t _audioBytes = 0;
  uint32_t *_idxOff = nullptr;          // PSRAM index tables
  uint32_t *_idxSize = nullptr;
  uint32_t _idxCap = 0;

  void writeAviHeader();
  void finishAvi();
  void writeWavHeader();
  void finishWav();
};

extern Recorder recorder;
