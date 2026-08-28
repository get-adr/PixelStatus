#include "Leds.h"
#include <LittleFS.h>
#include <math.h>

// Zeilenweise Ablage: autoWarn(0/1), warnSecs, warnFastSecs, activeLow(0/1),
// alternate(0/1), blinkPeriod, brightness(0..15, dieselbe Stufenskala wie die
// Matrix-Helligkeit).
static const char* CFG_PATH = "/leds.txt";

// ESP8266-Core in diesem Projekt definiert kein PWMRANGE-Makro (aeltere
// Core-Version) -- analogWrite()s Standardbereich ist aber dokumentiert
// 10 Bit (0..1023), solange analogWriteRange() nicht aufgerufen wird.
static const uint16_t LED_PWM_RANGE = 1023;

// Erfahrungswert (kein Lichtmessgeraet, am Geraet beobachtet, mehrere Runden):
// linear (~30% Duty), Gamma 2.2 ueber den VOLLEN Bereich (~33% Duty bei
// Stufe 9/15) und ein erster komprimierter Versuch (LED_USABLE_MAX_DUTY=450,
// Stufe 11 = Duty 242 bereits nicht mehr von Stufe 15 unterscheidbar) zeigen
// alle: "voll hell" wirkt schon bei einem kleinen Bruchteil des PWM-Bereichs
// (<=~240 von 1023) -- die LEDs/das Auge saettigen dort unabhaengig von der
// Kurvenform, eine reine Potenzkurve ueber den vollen 0..1023-Bereich kann
// Dunkel- UND Hell-Ende also nicht gleichzeitig gut aufloesen. Deshalb: Stufen
// 1-14 mappen per Gamma-Kurve (vgl. CIE 1931, menschliche Helligkeitswahr-
// nehmung ist grob logarithmisch) auf einen komprimierten, tatsaechlich
// sichtbar dimmbaren Bereich (0..LED_USABLE_MAX_DUTY), bewusst unterhalb des
// beobachteten Saettigungspunkts; Stufe 15 erzwingt separat den vollen Duty,
// damit "ganz hell" garantiert wirklich maximal ist (nicht nur "sieht schon
// voll aus") und sich sichtbar von Stufe 14 abhebt. Reiner Erfahrungswert --
// bei Bedarf einfach anpassen.
static const uint16_t LED_USABLE_MAX_DUTY = 220;
// Nur bei tatsaechlicher Aenderung aufgerufen (siehe write()), pow() ist hier
// also unproblematisch, selbst ohne FPU.
static uint16_t gammaDuty(uint8_t step) {
  if (step == 0)   return 0;
  if (step >= 15)  return LED_PWM_RANGE;
  float norm = step / 15.0f;
  return (uint16_t)(powf(norm, 2.0f) * LED_USABLE_MAX_DUTY + 0.5f);
}

LedController::LedController(uint8_t pin1, uint8_t pin2, bool activeLow,
                            long warnSecs, long warnFastSecs)
  : _activeLow(activeLow), _warnSecs(warnSecs), _warnFastSecs(warnFastSecs) {
  _pin[0] = pin1;
  _pin[1] = pin2;
}

void LedController::begin() {
  LittleFS.begin();          // idempotent; fuer gespeicherte Konfiguration
  loadConfig();              // ueberschreibt die config.h-Defaults, falls vorhanden
  for (uint8_t i = 0; i < 2; i++) {
    pinMode(_pin[i], OUTPUT);
    write(i, false);
  }
}

void LedController::loadConfig() {
  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) return;
  auto line = [&]() { String s = f.readStringUntil('\n'); s.trim(); return s; };
  _autoWarn     = (line() == "1");
  long ws = line().toInt(); if (ws > 0) _warnSecs = ws;
  long fs = line().toInt(); if (fs >= 0) _warnFastSecs = fs;
  _activeLow    = (line() == "1");
  _alternate    = (line() == "1");
  long bp = line().toInt(); if (bp >= 100 && bp <= 5000) _blinkPeriod = (uint16_t)bp;  // fehlt in alten Dateien -> Default bleibt
  // 0 ist bei der Helligkeit ein gueltiger Wert (LEDs per Helligkeit "aus"),
  // die 100-5000-Untergrenze von blinkPeriod eignet sich als Anwesenheitscheck
  // hier also nicht -- fehlt die Zeile in einer alten Datei ganz (f.available()
  // false), bliebe eine reine Wertepruefung sonst bei 0 haengen und wuerde den
  // Default (15 = voll) faelschlich auf "aus" ueberschreiben. Alte Dateien aus
  // der kurzlebigen 0-100%-Skala haben hier fast immer einen Wert > 15 stehen
  // und werden dadurch ohnehin verworfen (Default bleibt) -- fuer ein
  // Einzelgeraet reicht das als Migration.
  if (f.available()) {
    long br = line().toInt();
    if (br >= 0 && br <= 15) _brightness = (uint8_t)br;
  }
  f.close();
}

bool LedController::saveConfig(bool autoWarn, long warnSecs, long warnFastSecs,
                               bool activeLow, bool alternate, uint16_t blinkPeriod,
                               uint8_t brightness) {
  _autoWarn     = autoWarn;
  if (warnSecs > 0)      _warnSecs = warnSecs;
  if (warnFastSecs >= 0) _warnFastSecs = warnFastSecs;
  _activeLow    = activeLow;
  _alternate    = alternate;
  if (blinkPeriod >= 100 && blinkPeriod <= 5000) _blinkPeriod = blinkPeriod;
  if (brightness <= 15) _brightness = brightness;

  File f = LittleFS.open(CFG_PATH, "w");
  if (!f) return false;
  f.print(_autoWarn ? "1" : "0");  f.print('\n');
  f.print(_warnSecs);              f.print('\n');
  f.print(_warnFastSecs);          f.print('\n');
  f.print(_activeLow ? "1" : "0"); f.print('\n');
  f.print(_alternate ? "1" : "0"); f.print('\n');
  f.print(_blinkPeriod);           f.print('\n');
  f.print(_brightness);            f.print('\n');
  f.close();
  return true;
}

// Live-Uebernahme beim Schieben des Reglers (Web-UI/App), analog zu
// DisplayManager::setBrightness(): eigener, einzelner Setter statt ueber den
// batched saveConfig()-Aufruf des LED-Tabs -- ruft saveConfig() intern aber
// mit den unveraenderten restlichen Werten auf, es gibt also weiterhin nur
// EIN Ablageformat (/leds.txt), keine zweite Datei nur fuer die Helligkeit.
void LedController::setBrightness(uint8_t level) {
  if (level > 15) level = 15;
  if (level == _brightness) return;   // Flash schonen, wie DisplayManager::setBrightness()
  saveConfig(_autoWarn, _warnSecs, _warnFastSecs, _activeLow, _alternate, _blinkPeriod, level);
}

// Setzt den physischen Pegel per PWM; bei activeLow ist ein niedriger Duty-
// Cycle = heller (LED haengt gegen 3V3, wird durch LOW-Anteile durchgeschaltet).
// _brightness=100 ergibt denselben Duty wie vorher digitalWrite(HIGH) -- reine
// On/Off-Nutzung (Default) verhaelt sich also unveraendert.
// WICHTIG: nur bei tatsaechlicher Aenderung neu schreiben -- loop() ruft dies
// bei JEDEM Tick auf (frueher billiges digitalWrite()), analogWrite() setzt
// aber Software-PWM neu auf und ist auf dem ESP8266 spuerbar teurer; staendiges
// Neusetzen ohne Aenderung bremste die WiFi-Housekeeping der SDK genug aus, um
// das Geraet unerreichbar werden zu lassen (am Geraet beobachtet).
void LedController::write(uint8_t idx, bool on) {
  uint16_t duty = on ? gammaDuty(_brightness) : 0;
  if (_activeLow) duty = LED_PWM_RANGE - duty;
  if (duty == _lastDuty[idx]) return;
  _lastDuty[idx] = duty;
  analogWrite(_pin[idx], duty);
}

// An in der ersten Haelfte der Periode, aus in der zweiten.
bool LedController::blinkPhase(uint16_t periodMs) {
  return (millis() % periodMs) < (periodMs / 2);
}

// Wechselblinken ergibt nur Sinn, wenn beide LEDs blinken -- wird eine auf
// einen anderen Zustand gesetzt, ist es kein echtes Wechselblinken mehr.
void LedController::set(uint8_t idx, State s) {
  if (idx < 2) _manual[idx] = s;
  if (s != BLINK) _alternate = false;
}

void LedController::setAll(State s) {
  _manual[0] = _manual[1] = s;
  if (s != BLINK) _alternate = false;
}

// Restzeit des Timer-Countdowns; <0 = kein Countdown -> manueller Zustand gilt.
void LedController::updateCountdown(long remaining) {
  if (!_autoWarn) { _override = false; return; }
  if (remaining < 0 || remaining > _warnSecs) { _override = false; return; }
  _override = true;
  if (remaining == 0) {                       // Zeit abgelaufen -> Dauerlicht
    _ovState = ON;
  } else if (remaining <= _warnFastSecs) {    // letzte Minute -> schnell blinken
    _ovState = BLINK; _ovPeriod = 150;
  } else {                                     // Warnfenster -> ruhig blinken
    _ovState = BLINK; _ovPeriod = 500;
  }
}

void LedController::loop() {
  for (uint8_t i = 0; i < 2; i++) {
    State    s      = _override ? _ovState  : _manual[i];
    uint16_t period = _override ? _ovPeriod : _blinkPeriod;
    bool phase = blinkPhase(period);
    if (_alternate && i == 1) phase = !phase;   // zweite LED gegenphasig -> Wechselblinken
    bool on = (s == ON) || (s == BLINK && phase);
    write(i, on);
  }
}

static const char* stateName(LedController::State s) {
  return s == LedController::ON ? "on" : (s == LedController::BLINK ? "blink" : "off");
}

String LedController::stateFields() const {
  // Effektive Vorschau je LED: waehrend der Countdown-Warnung zeigt sie das
  // Override-Verhalten (blink / fast / on bei 0), sonst den manuellen Zustand.
  auto prev = [&](uint8_t i) -> const char* {
    if (_override)
      return (_ovState == ON) ? "on" : (_ovPeriod <= 200 ? "fast" : "blink");
    return stateName(_manual[i]);
  };
  return String("\"led1\":\"") + stateName(_manual[0]) +
         "\",\"led2\":\"" + stateName(_manual[1]) +
         "\",\"p1\":\"" + prev(0) +
         "\",\"p2\":\"" + prev(1) +
         "\",\"warn\":" + (_override ? "true" : "false") +
         ",\"alt\":" + (_alternate ? "true" : "false") +
         ",\"ledBrightness\":" + String(_brightness);
}
