#pragma once
#include <Arduino.h>

// Verwaltet die Uhrzeit-Synchronisation per NTP. Server und An/Aus liegen in
// LittleFS (/ntp.txt), config.h liefert nur die Defaults (NTP_SERVER). Die
// Zeitzone (NTP_TZ) bleibt compile-time und wird immer gesetzt, damit auch eine
// manuell gestellte Zeit (ohne NTP) lokal korrekt angezeigt wird.
//
// loop() erkennt einen WiFi-(Re)Connect und stoesst NTP dann neu an. Ist NTP
// deaktiviert, wird der SNTP-Client gestoppt (sntp_stop) -> eine manuell per
// setManual() gestellte Zeit bleibt stehen und wird nicht ueberschrieben.
class TimeManager {
public:
  void begin();                  // LittleFS laden, Zeitzone setzen, NTP starten/stoppen
  void loop();                   // WiFi-(Re)Connect -> NTP neu synchronisieren
  void saveConfig(bool enabled, const String& server);  // persistieren + sofort anwenden
  static void setManual(time_t epoch);                  // Systemzeit manuell setzen

  bool enabled() const { return _enabled; }
  const String& server() const { return _server; }
  static bool synced();          // true, wenn die Uhr eine plausible Zeit hat (nach ~2020)

private:
  void load();
  void apply();                  // configTime (enabled) bzw. sntp_stop (disabled)
  bool   _enabled = true;
  String _server;
  bool   _wasConnected = false;
};
