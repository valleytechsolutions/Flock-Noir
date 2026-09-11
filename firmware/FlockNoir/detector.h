// =============================================================================
//  Flock Noir  -  detector.h
//  Time-domain periodicity + duty-cycle detector for a pulsed IR source.
//
//  Design notes:
//   * We feed one brightness "level" sample per camera frame, each stamped with
//     micros(). Frame timing jitters, so we work in the TIME domain (edge
//     intervals) rather than doing an FFT that would assume a fixed rate.
//   * A detection requires BOTH the right period (~100 ms between rising edges)
//     AND the right duty (~20% on). That combination is what separates a Flock
//     IR illuminator from mains flicker, indicators, and steady lights.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

struct Sample {
  uint32_t t_us;     // micros() timestamp
  uint16_t level;    // tracked brightness (max blob pixel, 0..255)
  uint16_t blobPx;   // pixels >= SAT_THRESHOLD this frame (compactness gate)
};

struct DetectionResult {
  bool     detected   = false;
  float    freqHz     = 0.0f;
  float    dutyCycle  = 0.0f;
  float    confidence = 0.0f;
  int      goodCycles = 0;
  uint16_t levelPP    = 0;   // peak-to-peak level in window
  float    blobFrac   = 0.0f;// mean blob fraction while "on"
};

class Detector {
public:
  void begin(uint32_t scannedPixels);
  // Push one per-frame observation.
  void feed(uint16_t level, uint16_t blobPx, uint32_t t_us);
  // Analyze the current window. Returns the latest result.
  DetectionResult analyze();
  DetectionResult last() const { return _last; }

private:
  Sample   _buf[SAMPLE_BUFFER];
  int      _head = 0;      // next write index
  int      _count = 0;     // valid samples
  uint32_t _scannedPixels = 1;
  DetectionResult _last;
};
