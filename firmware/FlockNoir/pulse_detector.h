#pragma once
#include <stdint.h>
#include <math.h>

// Hardware-independent pulse validator. Timing matches are evidence, not identity.
struct PulseConfig {
  float minHz = 8, maxHz = 12, minDuty = 0.10f, maxDuty = 0.30f;
  float minWidthMs = 8, maxWidthMs = 35;
  float threshold = 120, release = 70, noiseMultiplier = 6;
  uint32_t required = 4, staleUs = 300000, maxGapUs = 5000;
};
class PulseDetector {
public:
  PulseConfig config;
  float baseline = 0, noise = 1, amplitude = 0, frequency = 0, duty = 0, widthMs = 0;
  uint32_t valid = 0, gaps = 0;
  bool matched = false, clipped = false;
  void reset() { initialized = false; clear(); }
  void feed(uint16_t raw, uint32_t us) {
    if (!initialized) { baseline = raw; previousRaw = raw; previous = us; initialized = true; return; }
    uint32_t elapsed = us - previous;
    previous = us;
    if (elapsed > config.maxGapUs) { ++gaps; clear(); baseline = raw; }
    clipped = raw >= 4000;
    if (clipped) clear();
    float delta = raw - baseline;
    float alpha = 1 - expf(-float(elapsed) / 500000.0f);
    baseline += alpha * delta;
    amplitude = fmaxf(fmaxf(delta, 0), amplitude * (1 - alpha));
    if (!high && fabsf(delta) < config.threshold)
      noise += alpha * (fabsf(float(raw) - previousRaw) - noise);
    previousRaw = raw;
    float trigger = fmaxf(config.threshold, noise * config.noiseMultiplier);
    float release = fmaxf(config.release, trigger * 0.5f);
    if (!clipped && !high && delta >= trigger) {
      uint32_t interval = us - rise;
      if (haveRise && haveWidth && interval > 0) {
        float hz = 1000000.0f / interval, d = float(width) / interval, w = width / 1000.0f;
        bool regular = valid == 0 || fabsf(float(interval) - lastPeriod) <= lastPeriod * 0.15f;
        if (hz >= config.minHz && hz <= config.maxHz && d >= config.minDuty &&
            d <= config.maxDuty && w >= config.minWidthMs && w <= config.maxWidthMs && regular) {
          if (valid < 1000) ++valid;
          frequency = hz; duty = d; widthMs = w; lastValid = us; lastPeriod = interval;
        } else { valid = 0; matched = false; }
      } else { valid = 0; }
      rise = us; haveRise = true; haveWidth = false; high = true;
    } else if (high && delta <= release) {
      high = false; width = us - rise; haveWidth = true;
      if (width / 1000.0f > config.maxWidthMs || width / 1000.0f < config.minWidthMs) {
        valid = 0; matched = false;
      }
    }
    if ((haveRise && us - rise > config.staleUs) ||
        (high && (us - rise) / 1000.0f > config.maxWidthMs)) clear();
    matched = valid >= config.required && us - lastValid <= config.staleUs && !clipped;
  }
private:
  bool initialized = false, high = false, haveRise = false, haveWidth = false;
  uint32_t previous = 0, rise = 0, width = 0, lastValid = 0, lastPeriod = 0;
  uint16_t previousRaw = 0;
  void clear() {
    high = haveRise = haveWidth = matched = false;
    valid = 0; frequency = duty = widthMs = 0;
  }
};
