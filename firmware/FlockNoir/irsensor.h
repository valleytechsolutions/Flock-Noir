// =============================================================================
//  Flock Noir  -  irsensor.h
//  OPT101 sampling task and event queue. The portable validator lives in
//  pulse_detector.h. ADC diagnostics cannot establish sensor connection or
//  identify the optical source. See docs/RADIO.md for validation and limits.
// =============================================================================
#pragma once
#include <Arduino.h>
#include <atomic>
#include <freertos/queue.h>
#include "config.h"

struct IrResult {
  bool     detected   = false;   // pattern currently seen (algorithm output)
  float    freqHz     = 0.0f;
  float    dutyCycle  = 0.0f;
  uint16_t amp        = 0;        // recent peak AC amplitude (ADC counts)
  uint16_t baseline   = 0;        // ambient baseline (ADC counts)
  int      validCount = 0;        // consecutive valid intervals
  bool     present    = false;    // ADC task running; cannot prove connection
  bool clipped = false;
  float pulseMs = 0, sampleHz = 0, noise = 0;
  uint32_t gaps = 0;
  uint16_t raw = 0;
  uint32_t timestampMs = 0, droppedEvents = 0, generation = 0;
};

class IrSensor {
public:
  void begin();                       // configure ADC + start the sampling task
  bool enabled() const;
  void setEnabled(bool e);            // persists to NVS

  void setSuspended(bool suspended);
  bool suspended() const {return _suspended.load();}

  IrResult result();                  // thread-safe copy of the latest result
  bool popEvent(IrResult &event);
  int snapshot(uint8_t *out, int maxN); // recent waveform, normalized 0..100

private:
  static void taskThunk(void *arg);
  void run();

  std::atomic<bool> _enabled{false},_suspended{true};
  std::atomic<uint32_t> _generation{0};
  volatile bool _started = false;

  // shared result (guarded by _mux)
  IrResult _res;
  QueueHandle_t _events = nullptr;
  portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

  // Decimated scope ring, guarded by _mux.
  uint16_t _ring[IR_RING];
  int _rhead = 0;
  int _rcount = 0;
};

extern IrSensor irSensor;
