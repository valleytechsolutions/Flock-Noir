// =============================================================================
//  Flock Noir  -  detector.h
//  Time-domain periodicity + duty-cycle detector for a pulsed IR source.
//
//  Camera timing is quantized by frame exposure and can miss narrow pulses.
//  A half-rate alias is retained as a weaker candidate, with the OBSERVED
//  frequency reported. Only the OPT101 can resolve the configured pulse width.
//  Neither timing nor compact brightness establishes an ALPR's identity.
// =============================================================================
#pragma once
#include <stdint.h>
#include <math.h>
#include "config.h"

struct Sample {
  uint32_t t_us;     // micros() timestamp
  uint16_t level;    // tracked brightness (max blob pixel, 0..255)
  uint16_t blobPx;   // pixels >= SAT_THRESHOLD this frame (compactness gate)
};

struct DetectionResult {
  bool     detected   = false;
  bool     aliased = false;
  float    sampleHz = 0;
  float    freqHz     = 0.0f;
  float    dutyCycle  = 0.0f;
  float    confidence = 0.0f;
  int      goodCycles = 0;
  uint16_t levelPP    = 0;   // peak-to-peak level in window
  float    blobFrac   = 0.0f;// mean blob fraction while "on"
  float    periodScore= 0.0f;// fraction of edge intervals near the target period
  float    jitter     = 1.0f;// spread of the matching intervals (0 = metronome)
};

class Detector {
public:
  void begin(uint32_t scannedPixels);
  // Push one per-frame observation.
  void feed(uint16_t level, uint16_t blobPx, uint32_t t_us);
  // Analyze the current window. Returns the latest result.
  DetectionResult analyze();
  DetectionResult last() const { return _last; }

  // Copy the most recent up-to-maxN brightness samples, normalized 0..100 over
  // their own min/max, into out[]. Returns the count. For the live UI waveform.
  int snapshot(uint8_t *out, int maxN) const;

private:
  Sample   _buf[SAMPLE_BUFFER];
  int      _head = 0;      // next write index
  int      _count = 0;     // valid samples
  uint32_t _scannedPixels = 1;
  DetectionResult _last;
};
