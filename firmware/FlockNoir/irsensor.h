// =============================================================================
//  Flock Noir  -  irsensor.h
//  Analog IR photodiode/phototransistor detector - the high-accuracy path.
//  A FreeRTOS task samples an ADC pin at ~1 kHz, tracks an EMA ambient baseline,
//  extracts the AC (pulse) component, validates rising-edge intervals against
//  the Flock band (~5-15 Hz), and asserts a detection after enough consecutive
//  valid intervals. This nails the 20 ms/80 ms timing the camera cannot.
//  Approach follows the open-source Noflock/Flock-IR-Detection project.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

struct IrResult {
  bool     detected   = false;   // pattern currently seen (algorithm output)
  float    freqHz     = 0.0f;
  float    dutyCycle  = 0.0f;
  uint16_t amp        = 0;        // recent peak AC amplitude (ADC counts)
  uint16_t baseline   = 0;        // ambient baseline (ADC counts)
  int      validCount = 0;        // consecutive valid intervals
  bool     present    = false;    // sensor looks connected (baseline in range)
};

class IrSensor {
public:
  void begin();                       // configure ADC + start the sampling task
  bool enabled() const { return _enabled; }
  void setEnabled(bool e);            // persists to NVS

  IrResult result();                  // thread-safe copy of the latest result
  int snapshot(uint8_t *out, int maxN); // recent waveform, normalized 0..100

private:
  static void taskThunk(void *arg);
  void run();

  bool _enabled = false;
  volatile bool _started = false;

  // shared result (guarded by _mux)
  IrResult _res;
  portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

  // decimated scope ring (written by task, read by snapshot; races tolerated)
  uint16_t _ring[IR_RING];
  volatile int _rhead = 0;
  volatile int _rcount = 0;
};

extern IrSensor irSensor;
