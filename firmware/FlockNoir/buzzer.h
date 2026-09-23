// =============================================================================
//  Flock Noir  -  buzzer.h
//  Non-blocking RTTTL (ringtone) player + persistent tone library (NVS).
//  Settings assigns presets or custom slots to each device/evidence category.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"
#include "alert_tones.h"

class Buzzer {
public:
  void begin();                       // attach pin, load settings from NVS
  void update();                      // call every loop() -- advances the tune

  // Playback
  void play(const String &rtttl);     // start any RTTTL string (non-blocking)
  void playSlot(int idx);             // play tone slot idx
  void playAlert();                   // play the configured alert slot
  void playDeviceAlert();             // legacy diagnostic: ALPR BLE preset
  void requestAlert(AlertTones::Kind kind);
  void playKind(AlertTones::Kind kind); // explicit settings preview
  void setMuted(bool muted);
  void setAlertSound(int kind,const String &sound);
  void stop();
  void clearAlerts() {_alerts.clear();stop();}
  bool isPlaying() const { return _playing; }
  uint32_t alertsPlayed() const { return _alertsPlayed; }

  // Settings (persisted in NVS)
  bool enabled() const { return _enabled; }
  int  alertIdx() const { return _alertIdx; }
  int  count() const { return _count; }
  const String &name(int i) const { return _name[i]; }
  const String &rtttl(int i) const { return _rtttl[i]; }

  void setEnabled(bool e) { _enabled = e; if(!e){_alerts.clear();stop();} }
  void setAlertIdx(int i) { if (i >= 0 && i < _count) _alertIdx = i; }
  void setCount(int c);
  void setTone(int i, const String &nm, const String &rt);
  bool save();                        // persist to NVS

  // Build a JSON blob of current settings for the web UI.
  String toJson() const;

private:
  bool  _attached = false;
  bool  _enabled  = true;
  int   _alertIdx = 0;
  int   _count    = 0;
  String _name[BUZZER_MAX_TONES];
  String _rtttl[BUZZER_MAX_TONES];
  String _alertSound[AlertTones::Count];
  AlertTones::Queue _alerts;
  bool _muted=false;
  uint32_t _alertsPlayed=0;

  // active playback state
  String   _notes;
  int      _i        = 0;
  bool     _playing  = false;
  uint32_t _noteEnd  = 0;
  int      _defDur   = 4;
  int      _defOct   = 6;
  int      _bpm      = 120;
  float    _wholeMs  = 2000.0f;

  void attachIfNeeded();
  void toneHz(float hz);              // drive/silence the piezo
  void seedDefaults();
};

extern Buzzer buzzer;
