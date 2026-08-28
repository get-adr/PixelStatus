#pragma once
#include <Arduino.h>

// Verwaltet die WiFi-Verbindung: laedt gespeicherte Zugangsdaten (LittleFS,
// Fallback auf die Defaults aus config.h), versucht sich als Client (STA) zu
// verbinden und oeffnet bei Fehlschlag einen Setup-Hotspot (AP). Neue Creds
// werden ueber das Web-Portal gespeichert (save()) und beim naechsten Boot
// genutzt.
class NetManager {
public:
  void begin();                                   // LittleFS mounten + Creds laden
  bool connectSTA(uint32_t timeoutMs, void (*pump)() = nullptr); // true = verbunden
  void startAP();                                 // Fallback-Hotspot oeffnen
  // Creds + Hostname persistieren (leerer Hostname -> Default aus config.h)
  bool save(const String& ssid, const String& pass, const String& hostname);
  // Nur den Hostnamen aendern, SSID/Passwort behalten (fuer die System-Einstellungen).
  bool saveHostname(const String& hostname) { return save(_ssid, _pass, hostname); }

  const String& ssid() const { return _ssid; }
  const String& hostname() const { return _hostname; }
  bool apActive() const { return _apActive; }
  String apSsid() const;                          // SSID des Setup-Hotspots
  // Tatsaechlich genutztes Passwort des Setup-Hotspots: entweder ein eigenes,
  // mindestens 8-stelliges AP_PASSWORD aus config.h, oder -- falls das leer/zu
  // kurz ist -- ein deterministisch aus der Chip-ID generiertes 8-stelliges
  // Hex-Passwort (siehe startAP()). Nur nach startAP() gueltig.
  const String& apPassword() const { return _apPassword; }

private:
  void load();
  String _ssid;
  String _pass;
  String _hostname;
  bool   _apActive = false;
  String _apPassword;
};
