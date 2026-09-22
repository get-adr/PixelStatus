#pragma once
#include <Arduino.h>

// Verwaltet die Uhrzeit-Synchronisation per NTP. An/Aus, Server UND Zeitzone
// liegen in LittleFS (/ntp.txt), config.h liefert nur die Defaults
// (NTP_SERVER/NTP_TZ). Die Zeitzone wird immer gesetzt -- also auch bei
// abgeschaltetem NTP -- damit eine manuell gestellte Zeit lokal korrekt
// angezeigt wird.
//
// Die Zeitzone war urspruenglich compile-time: uebertragen wird (per Web-UI,
// MQTT oder USB) immer nur ein UTC-Zeitstempel, die Matrix rechnete ihn aber
// fest in die einkompilierte Zone um und zeigte damit unabhaengig vom Standort
// deutsche Zeit an -- sichtbar vor allem beim Stellen der Uhr aus einem Browser
// in einer anderen Zone. Format ist der POSIX-TZ-String (z.B.
// "CET-1CEST,M3.5.0,M10.5.0/3"), den newlibs tzset() direkt versteht; eine
// IANA-Zonendatenbank gibt es auf dem ESP8266 nicht.
//
// loop() erkennt einen WiFi-(Re)Connect und stoesst NTP dann neu an. Ist NTP
// deaktiviert, wird der SNTP-Client gestoppt (sntp_stop) -> eine manuell per
// setManual() gestellte Zeit bleibt stehen und wird nicht ueberschrieben.
class TimeManager {
public:
  void begin();                  // LittleFS laden, Zeitzone setzen, NTP starten/stoppen
  void loop();                   // WiFi-(Re)Connect -> NTP neu synchronisieren
  // persistieren + sofort anwenden; leere Werte lassen das jeweilige Feld unveraendert
  void saveConfig(bool enabled, const String& server, const String& tz);
  static void setManual(time_t epoch);                  // Systemzeit manuell setzen

  bool enabled() const { return _enabled; }
  const String& server() const { return _server; }
  const String& tz() const { return _tz; }
  static bool synced();          // true, wenn die Uhr eine plausible Zeit hat (nach ~2020)

private:
  void load();
  void applyTz();                // setenv(TZ)+tzset -- wirkt auch ohne NTP
  void apply();                  // configTime (enabled) bzw. sntp_stop (disabled)
  bool   _enabled = true;
  String _server;
  String _tz;
  bool   _wasConnected = false;
};
