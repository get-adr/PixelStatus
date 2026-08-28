#pragma once
#include <Arduino.h>

// Zwei separat schaltbare Status-LEDs an frei waehlbaren GPIOs. Ergaenzung zur
// LED-Matrix, unabhaengig vom DisplayManager (eigenes Peripherie-Geraet).
//
// Zwei Betriebsarten, die sich ueberlagern:
//   1. Manuell  - per 'led'-Befehl (Web/MQTT/USB): je LED off | on | blink.
//   2. Auto     - Countdown-Warnung: waehrend eines Timer-Countdowns blinken in
//                 den letzten Minuten BEIDE LEDs (letzte Minute schneller, bei 0
//                 Dauerlicht). Endet der Countdown, kehren die LEDs in ihren
//                 manuell gesetzten Zustand zurueck (wie last_manual der App).
//
// Helligkeit (_brightness, 0..100%) gilt fuer beide LEDs und fuer beide Modi
// gleichermassen -- write() rechnet sie erst beim tatsaechlichen "an"-Schreiben
// per PWM (analogWrite) ein, Blinken/Override kennen nur ON/OFF/BLINK.
//
// loop() muss in jedem Arduino-loop() laufen, sonst blinkt nichts.
class LedController {
public:
  enum State { OFF, ON, BLINK };

  // warnSecs / warnFastSecs steuern die Countdown-Warnung (Restsekunden).
  LedController(uint8_t pin1, uint8_t pin2, bool activeLow = false,
                long warnSecs = 300, long warnFastSecs = 60);

  void begin();
  void loop();

  void set(uint8_t idx, State s);            // idx 0 oder 1
  void setAll(State s);
  State manualState(uint8_t idx) const { return (idx < 2) ? _manual[idx] : OFF; }  // fuer MQTT-Discovery-Lichter
  void setAlternate(bool on) { _alternate = on; }  // Laufzeit-Umschaltung (nicht persistiert)
  void updateCountdown(long remainingSecs);  // <0 = kein Countdown aktiv

  String stateFields() const;                // "led1":"off","led2":"on","warn":false (ohne Klammern)

  // ---- Laufzeit-Konfiguration (vom Web-Portal /leds genutzt, persistiert in LittleFS) ----
  bool autoWarn()     const { return _autoWarn; }
  long warnSecs()     const { return _warnSecs; }
  long warnFastSecs() const { return _warnFastSecs; }
  bool activeLow()    const { return _activeLow; }
  bool alternate()    const { return _alternate; }  // LEDs gegenphasig blinken?
  bool warnActive()   const { return _override; }   // Countdown-Warnung gerade aktiv?
  uint16_t blinkPeriod() const { return _blinkPeriod; }  // manuelle Blinkperiode in ms
  uint8_t brightness() const { return _brightness; }     // 0..15 (wie Matrix-Helligkeit), gilt fuer den "an"-Zustand (auch beim Blinken)
  uint8_t pin1()      const { return _pin[0]; }
  uint8_t pin2()      const { return _pin[1]; }
  // Speichert die Konfiguration in Flash und wendet sie sofort an (kein Neustart noetig).
  bool saveConfig(bool autoWarn, long warnSecs, long warnFastSecs, bool activeLow, bool alternate,
                  uint16_t blinkPeriod, uint8_t brightness);
  // Setzt NUR die Helligkeit und persistiert sofort (analog zu DisplayManager::
  // setBrightness()) -- fuer Live-Uebernahme beim Schieben des Reglers, ohne
  // den restlichen LED-Tab per "Speichern" mit abzuspeichern.
  void setBrightness(uint8_t level);

private:
  void     write(uint8_t idx, bool on);
  static bool blinkPhase(uint16_t periodMs); // millis-basierter An/Aus-Takt
  void     loadConfig();                      // ueberschreibt Defaults aus /leds.txt

  uint16_t _lastDuty[2] = { 0xFFFF, 0xFFFF };  // zuletzt tatsaechlich geschriebener PWM-Duty, siehe write()
  uint8_t _pin[2];
  bool    _activeLow;
  long    _warnSecs;
  long    _warnFastSecs;
  bool    _autoWarn = true;                   // Countdown-Warnung aktiv?
  bool    _alternate = false;                 // LEDs gegenphasig blinken?
  uint16_t _blinkPeriod = 500;                // manuelle Blinkperiode in ms (An+Aus zusammen)
  uint8_t  _brightness = 15;                  // 0..15 (wie Matrix-Helligkeit), per PWM (analogWrite) im "an"-Zustand

  State   _manual[2] = { OFF, OFF };

  // Countdown-Override (gilt fuer beide LEDs gleichzeitig)
  bool     _override  = false;
  State    _ovState   = OFF;    // ON = Dauerlicht, BLINK = blinken
  uint16_t _ovPeriod  = 500;    // Blinkperiode im Override (ms)
};
