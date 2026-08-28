#pragma once
#include <Arduino.h>

// Liest die Akkuspannung ueber einen externen Spannungsteiler an A0 (der
// ESP8266 hat nur diesen einen ADC-Pin). Zwei Teiler wirken in Reihe:
//   1. extern, auf der Platine: R1=100k (Akku OUT+ -> A0) / R2=220k (A0 -> GND)
//   2. eingebaut im D1 mini selbst: 220k/100k vom A0-Pin zum eigentlichen
//      ESP-ADC-Pin (skaliert dessen 0..3,2V-Bereich auf die 0..1V, die der
//      ESP8266-ADC vertraegt).
// Siehe hardware/wiring/parts-list.md fuer die Herleitung der Werte.
//
// Ohne verdrahtete Hardware liefert A0 nur Rauschen/Schwebespannung -> das
// Feature bleibt ueber BATTERY_MONITOR_ENABLED (config.h) standardmaessig AUS,
// bis der Teiler tatsaechlich angeschlossen ist.
//
// Der theoretische Teilerfaktor aus den Nennwerten (100k/220k) trifft die
// real verbauten Widerstaende wegen Bauteiltoleranz oft nicht genau genug
// (beim Testgeraet ca. 18% daneben). Deshalb ist der Faktor zur Laufzeit
// per calibrate() korrigierbar: eine mit dem Multimeter direkt an OUT+/OUT-
// gemessene echte Spannung eintragen, calibrate() rechnet daraus den
// tatsaechlichen Faktor zurueck und persistiert ihn in LittleFS (/battcal.txt)
// -> ueberlebt Neustarts, kein Neu-Flashen noetig. Web-UI: Settings/System-Tab.
//
// loop() muss in jedem Arduino-loop() laufen (kein zusaetzlicher Aufwand,
// misst nur gedrosselt alle paar Sekunden).
class BatteryMonitor {
public:
  BatteryMonitor(uint8_t pin, bool enabled, float emptyVolts, float fullVolts,
                 uint8_t lowPercent, uint8_t lowBrightness);

  void begin();
  void loop();

  bool    enabled()      const { return _enabled; }
  float   voltage()      const { return _voltage; }
  uint8_t percent()      const { return _percent; }
  bool    low()          const { return _enabled && _percent <= _lowPercent; }
  uint8_t lowBrightness() const { return _lowBrightness; }
  float   dividerFactor() const { return _dividerFactor; }

  // Kalibriert den Teilerfaktor anhand einer soeben mit dem Multimeter direkt
  // an OUT+/OUT- gemessenen echten Spannung; wirkt sofort und persistiert.
  // false, wenn noch kein ADC-Sample vorliegt (z.B. direkt nach dem Boot) oder
  // die Messung nicht aktiv ist.
  bool calibrate(float realVolts);

private:
  uint8_t _pin;
  bool    _enabled;
  float   _emptyVolts;
  float   _fullVolts;
  uint8_t _lowPercent;
  uint8_t _lowBrightness;

  float    _dividerFactor;
  float    _smoothedRaw = -1.0f;   // EMA des ADC-Rohwerts, -1 = noch keine Messung
  float    _voltage     = 0.0f;
  uint8_t  _percent     = 0;
  uint32_t _lastSampleMs = 0;

  void recompute();   // _voltage/_percent aus _smoothedRaw + _dividerFactor neu berechnen
};
