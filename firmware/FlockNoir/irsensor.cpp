#include "irsensor.h"
#include "pulse_detector.h"
#include <Preferences.h>
#include <esp_timer.h>
#include <esp32-hal-periman.h>

IrSensor irSensor;
static Preferences irPrefs;
void IrSensor::begin() {
  if (irPrefs.begin("flockir", true)) {
    _enabled = irPrefs.getBool("en", IR_DEFAULT_ENABLED != 0); irPrefs.end();
  }
  analogReadResolution(12);
  // The Arduino core attaches an ADC channel on its first read. Per-pin
  // attenuation only applies after that attachment has succeeded.
  analogRead(IR_SENSOR_PIN);
  if (perimanGetPinBusType(IR_SENSOR_PIN) != ESP32_BUS_TYPE_ADC_ONESHOT) {
    Serial.println("[IR] ADC initialization failed");
    return;
  }
  analogSetPinAttenuation(IR_SENSOR_PIN, ADC_11db);
  if (!_events) _events = xQueueCreate(8, sizeof(IrResult));
  if (!_events) Serial.println("[IR] event queue allocation failed");
  if (!_started) {
    _started = xTaskCreatePinnedToCore(taskThunk, "irsensor", 4096, this, 3, nullptr, 1) == pdPASS;
    if (!_started) Serial.println("[IR] task creation failed");
  }
}
bool IrSensor::enabled() const { return _enabled.load(); }
bool IrSensor::popEvent(IrResult &event) {
  while(_events && xQueueReceive(_events,&event,0)==pdTRUE)
    if(!suspended() && enabled() && event.generation==_generation.load())return true;
  return false;
}
void IrSensor::setSuspended(bool suspended) {
  if(_suspended.exchange(suspended)==suspended)return;
  ++_generation;
  portENTER_CRITICAL(&_mux);_res=IrResult();_res.present=_started;_rcount=_rhead=0;portEXIT_CRITICAL(&_mux);
}
void IrSensor::setEnabled(bool e) {
  ++_generation;
  _enabled.store(e);
  if (irPrefs.begin("flockir", false)) {
    if (!irPrefs.putBool("en", e)) Serial.println("[IR] settings write failed");
    irPrefs.end();
  }
}
IrResult IrSensor::result() {
  portENTER_CRITICAL(&_mux); IrResult r = _res; portEXIT_CRITICAL(&_mux);
  if (!enabled() || suspended()) r.detected = false;
  return r;
}
int IrSensor::snapshot(uint8_t *out, int maxN) {
  if (!out || maxN <= 0) return 0;
  uint16_t values[64];
  portENTER_CRITICAL(&_mux);
  int n = min(min(_rcount, maxN), 64);
  for (int i = 0; i < n; ++i) values[i] = _ring[(_rhead - n + i + IR_RING) % IR_RING];
  portEXIT_CRITICAL(&_mux);
  uint16_t low = 4095, high = 0;
  for (int i = 0; i < n; ++i) { low = min(low, values[i]); high = max(high, values[i]); }
  for (int i = 0; i < n; ++i) out[i] = (values[i] - low) * 100 / max(1, int(high - low));
  return n;
}
void IrSensor::taskThunk(void *arg) { static_cast<IrSensor *>(arg)->run(); }
void IrSensor::run() {
  PulseDetector pulse;
  pulse.config.minHz = IR_MIN_HZ; pulse.config.maxHz = IR_MAX_HZ;
  pulse.config.threshold = IR_THR_IDLE; pulse.config.release = IR_THR_LOCKED;
  pulse.config.required = IR_REQUIRED_INTERVALS;
  pulse.config.staleUs = IR_ACTIVE_WINDOW_MS * 1000;
  pulse.config.minDuty = IR_DUTY_MIN; pulse.config.maxDuty = IR_DUTY_MAX;
  pulse.config.minWidthMs = IR_PULSE_MIN_MS; pulse.config.maxWidthMs = IR_PULSE_MAX_MS;
  pulse.config.maxGapUs = IR_MAX_SAMPLE_GAP_US;
  TickType_t last = xTaskGetTickCount();
  const TickType_t ticks = max(TickType_t(1), pdMS_TO_TICKS(1000 / IR_SAMPLE_HZ));
  uint32_t publish = 0, samples = 0, rateAt = millis();
  float rate = 0;
  int decim = 0;
  bool wasEnabled = false;uint32_t generation=_generation.load();
  uint32_t lastEvent = 0, droppedEvents = 0;
  bool previousMatch = false;
  for (;;) {
    if(suspended()) {
      pulse.reset();wasEnabled=false;previousMatch=false;rate=0;samples=0;rateAt=millis();
      vTaskDelay(pdMS_TO_TICKS(20));last=xTaskGetTickCount();continue;
    }
    uint32_t currentGeneration=_generation.load();
    if(generation!=currentGeneration){pulse.reset();generation=currentGeneration;previousMatch=false;}
    bool en = enabled();
    uint16_t raw = analogRead(IR_SENSOR_PIN);
    uint32_t now = millis();
    if (en != wasEnabled) { pulse.reset(); wasEnabled = en; previousMatch = false; }
    if (en) pulse.feed(raw, uint32_t(esp_timer_get_time()));
    ++samples;
    if (now - rateAt >= 1000) { rate = samples * 1000.0f / (now - rateAt); samples = 0; rateAt = now; }
    if (++decim >= max(1, IR_SAMPLE_HZ / 200)) {
      decim = 0;
      portENTER_CRITICAL(&_mux);
      _ring[_rhead] = raw; _rhead = (_rhead + 1) % IR_RING;
      if (_rcount < IR_RING) ++_rcount;
      portEXIT_CRITICAL(&_mux);
    }
    if (now - publish >= 100) {
      publish = now;
      IrResult r;
      r.detected = en && pulse.matched;
      r.freqHz = pulse.frequency; r.dutyCycle = pulse.duty;
      r.amp = uint16_t(pulse.amplitude); r.baseline = uint16_t(pulse.baseline);
      r.validCount = pulse.valid; r.present = _started;
      r.clipped = pulse.clipped; r.pulseMs = pulse.widthMs;
      r.sampleHz = rate; r.gaps = pulse.gaps; r.noise = pulse.noise; r.raw = raw;
      r.timestampMs = now;r.generation=generation;
      if (r.detected && (!previousMatch || now-lastEvent >= ALERT_HOLDOFF_MS)) {
        if (!_events || xQueueSend(_events,&r,0)!=pdTRUE) ++droppedEvents;
        lastEvent = now;
      }
      previousMatch = r.detected;
      r.droppedEvents = droppedEvents;
      portENTER_CRITICAL(&_mux); if(!suspended() && generation==_generation.load())_res = r; portEXIT_CRITICAL(&_mux);
    }
    vTaskDelayUntil(&last, ticks);
    if (xTaskGetTickCount() - last > ticks) last = xTaskGetTickCount();
  }
}
