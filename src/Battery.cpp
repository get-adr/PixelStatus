#include "Battery.h"
#include <LittleFS.h>

namespace {
  // Default, bis eine Kalibrierung ueber die Web-UI (Settings/System-Tab)
  // gespeichert wurde: empirisch am Testgeraet ermittelt (August 2026).
  // Der theoretische Faktor aus 100k/220k extern * 100k/220k onboard
  // (0,6875*0,3125=0,21484375) lag ca. 18% daneben (Bauteiltoleranz beim
  // Verloeten) -- per Multimeter real gemessen 4,08V an OUT+/SYS, gleichzeitig
  // zeigte die alte Formel 3,35V. Andere Geraete/Widerstaende brauchen ihre
  // eigene Kalibrierung, siehe calibrate().
  // Vbat = (ADC-Rohwert/1023 * 1,0V) / DIVIDER_FACTOR.
  constexpr float DEFAULT_DIVIDER_FACTOR = 0.176404f;
  constexpr uint32_t SAMPLE_INTERVAL_MS = 5000;         // Akkuspannung aendert sich langsam
  constexpr float EMA_ALPHA = 0.1f;                     // glaettet ADC-Rauschen
  const char* CAL_FILE = "/battcal.txt";
}

BatteryMonitor::BatteryMonitor(uint8_t pin, bool enabled, float emptyVolts, float fullVolts,
                               uint8_t lowPercent, uint8_t lowBrightness)
  : _pin(pin), _enabled(enabled), _emptyVolts(emptyVolts), _fullVolts(fullVolts),
    _lowPercent(lowPercent), _lowBrightness(lowBrightness),
    _dividerFactor(DEFAULT_DIVIDER_FACTOR) {}

void BatteryMonitor::begin() {
  LittleFS.begin();   // idempotent, falls vor DisplayManager::begin() aufgerufen
  File f = LittleFS.open(CAL_FILE, "r");
  if (!f) return;
  float v = f.parseFloat();
  f.close();
  if (v > 0.01f && v < 1.0f) _dividerFactor = v;   // Plausibilitaetscheck, sonst Default
}

void BatteryMonitor::recompute() {
  float adcVoltage = (_smoothedRaw / 1023.0f) * 1.0f;
  _voltage = adcVoltage / _dividerFactor;

  float pct = (_voltage - _emptyVolts) / (_fullVolts - _emptyVolts) * 100.0f;
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  _percent = (uint8_t)(pct + 0.5f);
}

void BatteryMonitor::loop() {
  if (!_enabled) return;
  uint32_t now = millis();
  if (now - _lastSampleMs < SAMPLE_INTERVAL_MS) return;
  _lastSampleMs = now;

  int raw = analogRead(_pin);   // 0..1023 = 0..1,0V am ESP-ADC-Pin
  _smoothedRaw = (_smoothedRaw < 0) ? (float)raw : (_smoothedRaw * (1 - EMA_ALPHA) + raw * EMA_ALPHA);
  recompute();
}

bool BatteryMonitor::calibrate(float realVolts) {
  if (!_enabled || _smoothedRaw < 0 || realVolts <= 0) return false;

  float adcVoltage = (_smoothedRaw / 1023.0f) * 1.0f;
  _dividerFactor = adcVoltage / realVolts;
  recompute();   // sofort mit dem neuen Faktor, nicht erst beim naechsten Sample

  File f = LittleFS.open(CAL_FILE, "w");
  if (!f) return false;
  f.println(_dividerFactor, 6);
  f.close();
  return true;
}
