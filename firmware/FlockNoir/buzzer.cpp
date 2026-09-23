// =============================================================================
//  Flock Noir  -  buzzer.cpp
// =============================================================================
#include "buzzer.h"
#include "radio.h"
#include <Preferences.h>
#include <math.h>

Buzzer buzzer;
static Preferences prefs;

// ---- default tone library ---------------------------------------------------
// Bump BUZZER_DEFAULTS_VERSION (config.h) to force existing devices to re-seed
// these defaults on next boot (otherwise the old set stays in NVS).
struct DefTone { const char *name; const char *rtttl; };
static const DefTone kDefaults[] = {
  { "Power Rangers", "MMPR:d=16,o=7,b=400:c#8,p,c#8,p,b,c#8,p,e8,p,c#8" },
  { "ALPR Alarm",    "Alarm:d=8,o=6,b=180:c,p,c,p,c7,p,c7,p,g,p,g" },
  { "Triple Chirp",  "Chirp:d=32,o=7,b=200:c,p,c,p,c" },
};
static const int kDefaultCount = sizeof(kDefaults) / sizeof(kDefaults[0]);

// -----------------------------------------------------------------------------
void Buzzer::attachIfNeeded() {
#ifdef BUZZER_ENABLE_PIN
  if (_attached) return;
  // Dedicated LEDC channel, away from the camera XCLK (channel/timer 0).
  if(!ledcAttachChannel(BUZZER_PIN, 2000, 10, BUZZER_LEDC_CHANNEL)) {
    Serial.println("[BUZZER] LEDC attach failed");return;
  }
  ledcWrite(BUZZER_PIN, 0);
  _attached = true;
#endif
}

void Buzzer::toneHz(float hz) {
#ifdef BUZZER_ENABLE_PIN
  attachIfNeeded();
  if (hz <= 0) { ledcWrite(BUZZER_PIN, 0); return; }       // silence
  ledcWriteTone(BUZZER_PIN, (uint32_t)hz);                 // ~50% duty square
#endif
}

// -----------------------------------------------------------------------------
void Buzzer::seedDefaults() {
  _count = kDefaultCount;
  for (int i = 0; i < _count; i++) {
    _name[i]  = kDefaults[i].name;
    _rtttl[i] = kDefaults[i].rtttl;
  }
  _alertIdx = 0;
  _enabled  = true;
}

void Buzzer::begin() {
  attachIfNeeded();

  prefs.begin("flockbuz", true);                 // read-only probe
  int cnt = prefs.getInt("cnt", -1);
  int ver = prefs.getInt("ver", 0);
  prefs.end();

  if (cnt < 0 || ver != BUZZER_DEFAULTS_VERSION) {   // first boot / new defaults
    seedDefaults();
    save();
  } else {
    prefs.begin("flockbuz", true);
    _enabled  = prefs.getBool("en", true);
    _alertIdx = prefs.getInt("ai", 0);
    _count    = constrain(cnt, 0, BUZZER_MAX_TONES);
    for (int i = 0; i < _count; i++) {
      _name[i]  = prefs.getString(("nm" + String(i)).c_str(), "");
      _rtttl[i] = prefs.getString(("rt" + String(i)).c_str(), "");
    }
    prefs.end();
    if (_alertIdx < 0 || _alertIdx >= _count) _alertIdx = 0;
  }

  for(int i=0;i<AlertTones::Count;++i)_alertSound[i]=AlertTones::defaults[i];
  if(prefs.begin("flockbuz",true)) {
    for(int i=0;i<AlertTones::Count;++i) {
      String value=prefs.getString(("snd"+String(i)).c_str(),AlertTones::defaults[i]);
      if(AlertTones::valid(value.c_str(),_count))_alertSound[i]=value;
    }
    prefs.end();
  }

#if BUZZER_STARTUP_BEEP
  // Power-on jingle: the Super Mario Bros. overworld theme (opening phrase).
  if (_enabled) play("Mario:d=4,o=5,b=200:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g,8p,"
                     "8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,"
                     "8g6,8e6,16c6,16d6,8b");
#endif
}

void Buzzer::setCount(int c) { _count = constrain(c, 0, BUZZER_MAX_TONES); }

void Buzzer::setTone(int i, const String &nm, const String &rt) {
  if (i < 0 || i >= BUZZER_MAX_TONES) return;
  _name[i] = nm; _rtttl[i] = rt;
}

bool Buzzer::save() {
  if(!prefs.begin("flockbuz", false))return false;
  bool ok=prefs.putInt("ver", BUZZER_DEFAULTS_VERSION)>0;
  ok=(prefs.putBool("en", _enabled)>0) && ok;
  ok=(prefs.putInt("ai", _alertIdx)>0) && ok;
  ok=(prefs.putInt("cnt", _count)>0) && ok;
  for (int i = 0; i < _count; i++) {
    ok=(prefs.putString(("nm" + String(i)).c_str(), _name[i])==_name[i].length()) && ok;
    ok=(prefs.putString(("rt" + String(i)).c_str(), _rtttl[i])==_rtttl[i].length()) && ok;
  }
  for(int i=0;i<AlertTones::Count;++i) {
    if(!AlertTones::valid(_alertSound[i].c_str(),_count))_alertSound[i]=AlertTones::defaults[i];
    ok=(prefs.putString(("snd"+String(i)).c_str(),_alertSound[i])==_alertSound[i].length()) && ok;
  }
  prefs.end();
  return ok;
}

// -----------------------------------------------------------------------------
//  RTTTL playback (non-blocking)
// -----------------------------------------------------------------------------
static float noteToHz(char letter, bool sharp, int octave) {
  int idx;
  switch (letter) {
    case 'c': idx = 0;  break;  case 'd': idx = 2;  break;
    case 'e': idx = 4;  break;  case 'f': idx = 5;  break;
    case 'g': idx = 7;  break;  case 'a': idx = 9;  break;
    case 'b': idx = 11; break;  default:  return 0;   // 'p' pause / unknown
  }
  if (sharp) idx++;
  int midi = (octave + 1) * 12 + idx;              // C4 = 60, A4 = 69 = 440Hz
  return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

void Buzzer::play(const String &rtttl) {
  // split "name:defaults:notes"
  int c1 = rtttl.indexOf(':');
  int c2 = rtttl.indexOf(':', c1 + 1);
  if (c1 < 0 || c2 < 0) { stop(); return; }

  String defs  = rtttl.substring(c1 + 1, c2);
  _notes       = rtttl.substring(c2 + 1);
  _notes.toLowerCase();
  _notes.replace(" ", "");

  _defDur = 4; _defOct = 6; _bpm = 120;
  // parse defaults d=,o=,b=
  int start = 0;
  while (start < (int)defs.length()) {
    int comma = defs.indexOf(',', start);
    if (comma < 0) comma = defs.length();
    String tok = defs.substring(start, comma); tok.toLowerCase();
    int eq = tok.indexOf('=');
    if (eq > 0) {
      char key = tok.charAt(0);
      int val  = tok.substring(eq + 1).toInt();
      if (key == 'd' && val > 0) _defDur = val;
      else if (key == 'o' && val > 0) _defOct = val;
      else if (key == 'b' && val > 0) _bpm = val;
    }
    start = comma + 1;
  }
  _wholeMs = (60000.0f / _bpm) * 4.0f;
  _i = 0; _noteEnd = 0; _playing = true;
}

void Buzzer::playSlot(int idx) {
  if (idx >= 0 && idx < _count) play(_rtttl[idx]);
}
void Buzzer::playAlert() {
  if (_enabled && _count > 0) playSlot(_alertIdx);
}
void Buzzer::playDeviceAlert() {if(_enabled)playKind(AlertTones::Ble);}
void Buzzer::setAlertSound(int kind,const String &sound) {
  if(kind>=0 && kind<AlertTones::Count && AlertTones::valid(sound.c_str(),_count))_alertSound[kind]=sound;
}
void Buzzer::requestAlert(AlertTones::Kind kind) {
  if(_enabled && !_muted)_alerts.request(kind,millis());
}
void Buzzer::setMuted(bool muted) {
  if(muted && !_muted)stop();
  _muted=muted;
  if(muted)_alerts.clear();
}
void Buzzer::playKind(AlertTones::Kind kind) {
  if(kind>=AlertTones::Count)return;
  const auto *p=AlertTones::preset(_alertSound[kind].c_str());
  if(p) {if(*p->rtttl)play(p->rtttl);else stop();return;}
  playSlot(AlertTones::slot(_alertSound[kind].c_str()));
}

void Buzzer::stop() { _playing = false; toneHz(0); }

void Buzzer::update() {
  AlertTones::Kind next;
  if(_alerts.take(millis(),_muted,_enabled,_playing,next)) {
    playKind(next);if(_playing)++_alertsPlayed;
  }
  if (!_playing) return;
  if ((int32_t)(millis() - _noteEnd) < 0) return;      // current note still ringing

  const int n = _notes.length();
  if (_i >= n) { stop(); return; }

  // ---- parse one note token: [duration] note [#] [octave] [.] ----
  int dur = 0;
  while (_i < n && isDigit(_notes[_i])) { dur = dur * 10 + (_notes[_i]-'0'); _i++; }
  if (dur == 0) dur = _defDur;

  char letter = (_i < n) ? _notes[_i++] : 'p';
  bool sharp = false;
  if (_i < n && _notes[_i] == '#') { sharp = true; _i++; }

  int octave = _defOct;
  bool dotted = false;
  // octave and dot can appear in either order
  while (_i < n && (isDigit(_notes[_i]) || _notes[_i] == '.')) {
    if (_notes[_i] == '.') { dotted = true; _i++; }
    else { octave = _notes[_i] - '0'; _i++; }
  }
  if (_i < n && _notes[_i] == ',') _i++;               // consume separator

  float ms = _wholeMs / dur;
  if (dotted) ms *= 1.5f;

  float hz = (letter == 'p') ? 0 : noteToHz(letter, sharp, octave);
  toneHz(hz);
  _noteEnd = millis() + (uint32_t)ms;
}

// -----------------------------------------------------------------------------
String Buzzer::toJson() const {
  String j = "{";
  j += "\"enabled\":" + String(_enabled ? "true" : "false");
  j += ",\"alertIdx\":" + String(_alertIdx);
  j += ",\"max\":" + String(BUZZER_MAX_TONES);
  j += ",\"tones\":[";
  for (int i = 0; i < _count; i++) {
    if (i) j += ",";
    j += "{\"name\":" + jsonQuote(_name[i]) + ",\"rtttl\":" + jsonQuote(_rtttl[i]) + "}";
  }
  j += "],\"alertKinds\":[";
  for(int i=0;i<AlertTones::Count;++i) {
    if(i)j+=',';
    j+="{\"id\":"+jsonQuote(AlertTones::ids[i])+",\"name\":"+jsonQuote(AlertTones::labels[i])+",\"sound\":"+jsonQuote(_alertSound[i])+"}";
  }
  j+="],\"soundPresets\":[";
  bool first=true;
  for(const auto &p:AlertTones::presets) {
    if(!first)j+=',';
    first=false;
    j+="{\"id\":"+jsonQuote(p.id)+",\"name\":"+jsonQuote(p.name)+",\"rtttl\":"+jsonQuote(p.rtttl)+"}";
  }
  j += "]}";
  return j;
}
