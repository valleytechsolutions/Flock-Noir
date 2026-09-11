// =============================================================================
//  Flock Noir  -  buzzer.cpp
// =============================================================================
#include "buzzer.h"
#include <Preferences.h>
#include <math.h>

Buzzer buzzer;
static Preferences prefs;

// ---- default tone library ---------------------------------------------------
// NOTE: the "Power Rangers communicator" default is a best-effort approximation
// of that rising chirp -- tweak it in the Settings tab until it sounds right.
struct DefTone { const char *name; const char *rtttl; };
static const DefTone kDefaults[] = {
  { "Power Rangers", "PwrRngr:d=16,o=6,b=200:c,e,g,c7,g7,c7,g,e,c,e,g" },
  { "ALPR Alarm",    "Alarm:d=8,o=6,b=180:c,p,c,p,c7,p,c7,p,g,p,g" },
  { "Triple Chirp",  "Chirp:d=32,o=7,b=200:c,p,c,p,c" },
};
static const int kDefaultCount = sizeof(kDefaults) / sizeof(kDefaults[0]);

// -----------------------------------------------------------------------------
void Buzzer::attachIfNeeded() {
#ifdef BUZZER_ENABLE_PIN
  if (_attached) return;
  // Dedicated LEDC channel, away from the camera XCLK (channel/timer 0).
  ledcAttachChannel(BUZZER_PIN, 2000, 10, BUZZER_LEDC_CHANNEL);
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
  prefs.end();

  if (cnt < 0) {                                 // first boot -> seed + save
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
    if (_alertIdx >= _count) _alertIdx = 0;
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

void Buzzer::save() {
  prefs.begin("flockbuz", false);
  prefs.putBool("en", _enabled);
  prefs.putInt("ai", _alertIdx);
  prefs.putInt("cnt", _count);
  for (int i = 0; i < _count; i++) {
    prefs.putString(("nm" + String(i)).c_str(), _name[i]);
    prefs.putString(("rt" + String(i)).c_str(), _rtttl[i]);
  }
  prefs.end();
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
  if (c1 < 0 || c2 < 0) { _playing = false; return; }

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

void Buzzer::stop() { _playing = false; toneHz(0); }

void Buzzer::update() {
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
    String nm = _name[i];  nm.replace("\"", "\\\"");
    String rt = _rtttl[i]; rt.replace("\"", "\\\"");
    j += "{\"name\":\"" + nm + "\",\"rtttl\":\"" + rt + "\"}";
  }
  j += "]}";
  return j;
}
