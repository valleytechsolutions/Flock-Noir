// =============================================================================
//  Flock Noir  -  irsensor.cpp
// =============================================================================
#include "irsensor.h"
#include <Preferences.h>

IrSensor irSensor;
static Preferences irPrefs;

void IrSensor::begin() {
  irPrefs.begin("flockir", true);
  _enabled = irPrefs.getBool("en", IR_DEFAULT_ENABLED ? true : false);
  irPrefs.end();

  analogReadResolution(12);
  analogSetPinAttenuation(IR_SENSOR_PIN, ADC_11db);   // ~full 0..3.1V range

  if (!_started) {
    _started = true;
    xTaskCreatePinnedToCore(taskThunk, "irsensor", 4096, this, 2, nullptr, 0);
  }
}

void IrSensor::setEnabled(bool e) {
  _enabled = e;
  irPrefs.begin("flockir", false);
  irPrefs.putBool("en", e);
  irPrefs.end();
}

IrResult IrSensor::result() {
  IrResult r;
  portENTER_CRITICAL(&_mux);
  r = _res;
  portEXIT_CRITICAL(&_mux);
  return r;
}

int IrSensor::snapshot(uint8_t *out, int maxN) {
  int cnt = _rcount, head = _rhead;
  int n = (cnt < maxN) ? cnt : maxN;
  if (n <= 0) return 0;
  int start = (head - n + IR_RING) % IR_RING;
  uint16_t vmin = 0xFFFF, vmax = 0;
  for (int i = 0; i < n; i++) {
    uint16_t v = _ring[(start + i) % IR_RING];
    if (v < vmin) vmin = v;
    if (v > vmax) vmax = v;
  }
  int range = (vmax > vmin) ? (vmax - vmin) : 1;
  for (int i = 0; i < n; i++) {
    uint16_t v = _ring[(start + i) % IR_RING];
    out[i] = (uint8_t)(((int)(v - vmin) * 100) / range);
  }
  return n;
}

void IrSensor::taskThunk(void *arg) { static_cast<IrSensor *>(arg)->run(); }

// -----------------------------------------------------------------------------
//  1 kHz sampling loop: EMA baseline, AC extraction, edge/interval validation.
// -----------------------------------------------------------------------------
void IrSensor::run() {
  const TickType_t period = pdMS_TO_TICKS(1000 / IR_SAMPLE_HZ);  // ~1 ms
  TickType_t last = xTaskGetTickCount();

  float    baseline   = analogRead(IR_SENSOR_PIN);
  bool     high       = false;             // above-threshold state (hysteresis)
  uint32_t lastEdgeMs = 0;                  // time of last counted rising edge
  uint32_t prevEdgeMs = 0;
  int      validCount = 0;
  uint16_t ampWin     = 0;                   // running peak AC (decays)
  float    onAccum    = 0, totAccum = 0;     // for duty estimate
  uint32_t lastActive = 0;                   // last time a valid pulse train seen
  int      decim      = 0;
  const int DECN      = IR_SAMPLE_HZ / 200;  // ~200 Hz into the scope ring

  for (;;) {
    int raw = analogRead(IR_SENSOR_PIN);
    uint32_t nowMs = millis();

    // ambient baseline (slow EMA) and AC (upward pulses)
    baseline += (raw - baseline) * IR_BASELINE_ALPHA;
    int ac = raw - (int)baseline;
    if (ac < 0) ac = 0;
    if ((uint16_t)ac > ampWin) ampWin = ac;
    else ampWin = (uint16_t)(ampWin * 0.995f);   // slow decay

    // duty estimate over a rolling sense of "on" (above locked threshold)
    totAccum = totAccum * 0.999f + 1.0f;
    onAccum  = onAccum  * 0.999f + (ac > IR_THR_LOCKED ? 1.0f : 0.0f);

    // hysteresis edge detection with a refractory gap
    if (!high && ac > IR_THR_IDLE && (nowMs - lastEdgeMs) >= IR_REFRACTORY_MS) {
      high = true;
      uint32_t interval = nowMs - lastEdgeMs;
      if (prevEdgeMs != 0) {
        float hz = (interval > 0) ? (1000.0f / interval) : 0.0f;
        if (hz >= IR_MIN_HZ && hz <= IR_MAX_HZ) {
          validCount++;
          lastActive = nowMs;
        } else if (interval > 400) {
          validCount = 0;                  // long gap resets the train
        }
      }
      prevEdgeMs = lastEdgeMs;
      lastEdgeMs = nowMs;
    } else if (high && ac < IR_THR_LOCKED) {
      high = false;
    }

    // decimate into the scope ring
    if (++decim >= DECN) {
      decim = 0;
      _ring[_rhead] = (uint16_t)raw;
      _rhead = (_rhead + 1) % IR_RING;
      if (_rcount < IR_RING) _rcount++;
    }

    // publish result ~ every 100 ms
    static uint32_t pub = 0;
    if (nowMs - pub >= 100) {
      pub = nowMs;
      bool active = (nowMs - lastActive) <= IR_ACTIVE_WINDOW_MS;
      bool det = active && (validCount >= IR_REQUIRED_INTERVALS);
      // freq from the most recent valid interval spacing
      float hz = 0.0f;
      uint32_t iv = lastEdgeMs - prevEdgeMs;
      if (iv > 0 && iv < 400) hz = 1000.0f / iv;
      float duty = (totAccum > 1) ? (onAccum / totAccum) : 0.0f;

      portENTER_CRITICAL(&_mux);
      _res.detected   = det;
      _res.freqHz     = det ? hz : 0.0f;
      _res.dutyCycle  = duty;
      _res.amp        = ampWin;
      _res.baseline   = (uint16_t)baseline;
      _res.validCount = validCount;
      _res.present    = (baseline > 30 && baseline < 4060);  // not floating rail
      portEXIT_CRITICAL(&_mux);

      if (!active) validCount = 0;         // decay the train when idle
    }

    vTaskDelayUntil(&last, period);
  }
}
