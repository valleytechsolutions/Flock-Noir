// =============================================================================
//  Flock Noir  -  detector.cpp
// =============================================================================
#include "detector.h"

void Detector::begin(uint32_t scannedPixels) {
  _scannedPixels = scannedPixels ? scannedPixels : 1;
  _head = 0;
  _count = 0;
  _last = DetectionResult();
}

void Detector::feed(uint16_t level, uint16_t blobPx, uint32_t t_us) {
  _buf[_head] = { t_us, level, blobPx };
  _head = (_head + 1) % SAMPLE_BUFFER;
  if (_count < SAMPLE_BUFFER) _count++;
}

// Iterate the ring buffer in chronological order.
static inline int idxAt(int head, int count, int i) {
  int start = (head - count + SAMPLE_BUFFER) % SAMPLE_BUFFER;
  return (start + i) % SAMPLE_BUFFER;
}

DetectionResult Detector::analyze() {
  DetectionResult r;
  if (_count < MIN_GOOD_CYCLES * 2) { _last = r; return r; }

  // --- 1. window statistics ---
  uint16_t vmin = 0xFFFF, vmax = 0;
  for (int i = 0; i < _count; i++) {
    uint16_t v = _buf[idxAt(_head, _count, i)].level;
    if (v < vmin) vmin = v;
    if (v > vmax) vmax = v;
  }
  uint16_t pp = vmax - vmin;
  r.levelPP = pp;
  if (pp < MIN_AMPLITUDE) { _last = r; return r; }   // scene too flat

  // --- 2. threshold with hysteresis -> binary on/off stream + edges ---
  float hi = vmin + pp * 0.60f;
  float lo = vmin + pp * 0.40f;

  bool     on = false;
  uint32_t lastRise_us = 0;
  uint32_t onStart_us  = 0;
  bool     haveRise    = false;

  // Accumulators
  int      cyclesInTol = 0;    // rising-edge intervals near TARGET_PERIOD
  int      totalRises  = 0;
  double   onTimeSum   = 0;    // total "on" microseconds (completed pulses)
  double   spanSum     = 0;    // total time covered by completed cycles
  double   blobFracSum = 0;    // mean blob fraction during "on"
  int      blobSamples = 0;

  for (int i = 0; i < _count; i++) {
    int k = idxAt(_head, _count, i);
    uint16_t v = _buf[k].level;
    uint32_t t = _buf[k].t_us;

    if (!on && v >= hi) {
      // rising edge
      on = true;
      onStart_us = t;
      if (haveRise) {
        double interval_ms = (double)(t - lastRise_us) / 1000.0;
        totalRises++;
        if (fabs(interval_ms - TARGET_PERIOD_MS) <= PERIOD_TOL_MS) {
          cyclesInTol++;
          spanSum += (t - lastRise_us);
        }
      }
      lastRise_us = t;
      haveRise = true;
    } else if (on && v <= lo) {
      // falling edge -> pulse complete
      on = false;
      onTimeSum += (t - onStart_us);
    }

    if (on) {
      blobFracSum += (double)_buf[k].blobPx / (double)_scannedPixels;
      blobSamples++;
    }
  }

  r.goodCycles = cyclesInTol;
  if (blobSamples) r.blobFrac = (float)(blobFracSum / blobSamples);

  // Frequency from the in-tolerance cycles (robust to jitter/outliers).
  if (cyclesInTol > 0 && spanSum > 0) {
    double avgPeriod_ms = (spanSum / cyclesInTol) / 1000.0;
    r.freqHz = (avgPeriod_ms > 0) ? (float)(1000.0 / avgPeriod_ms) : 0.0f;
  }

  // Duty from total on-time over covered span (approx; frame-rate limited).
  double totalSpan_us = 0;
  {
    int first = idxAt(_head, _count, 0);
    int lastI = idxAt(_head, _count, _count - 1);
    totalSpan_us = (double)(_buf[lastI].t_us - _buf[first].t_us);
  }
  if (totalSpan_us > 0) r.dutyCycle = (float)(onTimeSum / totalSpan_us);

  // --- 3. score it ---
  // period score: fraction of rising intervals that matched the target
  float periodScore = (totalRises > 0)
                        ? (float)cyclesInTol / (float)totalRises : 0.0f;
  // duty score: 1.0 inside the band, tapering outside
  float dutyScore;
  if (r.dutyCycle >= DUTY_MIN && r.dutyCycle <= DUTY_MAX) dutyScore = 1.0f;
  else {
    float d = (r.dutyCycle < DUTY_MIN) ? (DUTY_MIN - r.dutyCycle)
                                       : (r.dutyCycle - DUTY_MAX);
    dutyScore = fmaxf(0.0f, 1.0f - d * 6.0f);
  }
  // cycle-count score: saturates at MIN_GOOD_CYCLES
  float cycleScore = fminf(1.0f, (float)cyclesInTol / (float)MIN_GOOD_CYCLES);
  // compactness gate: penalize whole-frame flicker
  float compactScore = (r.blobFrac <= BLOB_MAX_FRACTION) ? 1.0f
                        : fmaxf(0.0f, 1.0f - (r.blobFrac - BLOB_MAX_FRACTION) * 3.0f);

  r.confidence = periodScore * 0.4f + dutyScore * 0.3f
               + cycleScore * 0.2f + compactScore * 0.1f;

  // Loose ("approximate") decision: enough matching cycles, a little regularity,
  // and a low confidence bar. This intentionally allows false positives.
  r.detected = (cyclesInTol >= MIN_GOOD_CYCLES) &&
               (periodScore >= PERIOD_SCORE_MIN) &&
               (r.confidence >= DETECT_CONFIDENCE);

  _last = r;
  return r;
}
