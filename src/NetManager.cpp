#include "NetManager.h"
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include "config.h"

// Zeilenweise Ablage: Zeile 1 = SSID, Zeile 2 = Passwort.
static const char* CRED_PATH = "/wifi.txt";

// Entfernt nur CR/LF am Zeilenende (Passwoerter duerfen fuehrende/nachfolgende
// Leerzeichen enthalten, daher kein voller trim()).
static void stripEol(String& s) {
  while (s.length() && (s[s.length() - 1] == '\r' || s[s.length() - 1] == '\n'))
    s.remove(s.length() - 1);
}

void NetManager::begin() {
  LittleFS.begin();   // formatiert das FS automatisch beim ersten Start
  load();
}

void NetManager::load() {
  // Defaults aus config.h, werden von gespeicherten Werten ueberschrieben.
  _ssid     = WIFI_SSID;
  _pass     = WIFI_PASSWORD;
  _hostname = HOSTNAME;

  File f = LittleFS.open(CRED_PATH, "r");
  if (!f) return;
  String s = f.readStringUntil('\n'); stripEol(s);
  String p = f.readStringUntil('\n'); stripEol(p);
  String h = f.readStringUntil('\n'); stripEol(h);   // Zeile 3 optional (Altdaten haben sie nicht)
  f.close();
  if (s.length()) { _ssid = s; _pass = p; }
  if (h.length()) _hostname = h;
}

bool NetManager::save(const String& ssid, const String& pass, const String& hostname) {
  _ssid     = ssid;
  _pass     = pass;
  _hostname = hostname.length() ? hostname : String(HOSTNAME);

  File f = LittleFS.open(CRED_PATH, "w");
  if (!f) return false;
  f.print(_ssid);     f.print('\n');
  f.print(_pass);     f.print('\n');
  f.print(_hostname); f.print('\n');
  f.close();
  return true;
}

bool NetManager::connectSTA(uint32_t timeoutMs, void (*pump)()) {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(_hostname);
  WiFi.begin(_ssid.c_str(), _pass.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    if (pump) pump();   // z.B. display.loop() am Laufen halten
    delay(1);
  }
  _apActive = false;
  return WiFi.status() == WL_CONNECTED;
}

// Deterministisches, geraetespezifisches 8-stelliges Hex-Passwort aus der
// Chip-ID -- ersetzt ein fest einprogrammiertes Standardpasswort (SEC-05:
// "12345678" ist eines der weltweit haeufigsten Passwoerter). Erfuellt WPA2s
// Mindestlaenge automatisch, ist pro Geraet eindeutig und bleibt ueber
// Neustarts stabil (aus der Chip-ID abgeleitet, nicht zufaellig -- sonst
// waere es nach jedem Reboot ein anderes). Wird auf der Matrix angezeigt
// (siehe main.cpp), das Vertrauensmodell ist wie bei einem aufgedruckten
// Router-WLAN-Schluessel: nur wer das Geraet physisch vor sich hat, liest es ab.
static String chipIdPassword() {
  char buf[9];
  snprintf(buf, sizeof(buf), "%08X", ESP.getChipId());
  return String(buf);
}

void NetManager::startAP() {
  WiFi.mode(WIFI_AP);
  // Eigenes AP_PASSWORD (>=8 Zeichen, WPA2-Minimum) aus config.h hat Vorrang;
  // sonst (leer oder zu kurz) automatisch generiertes Passwort -- anders als
  // zuvor gibt es keinen "offenes Netz"-Fallback mehr fuer ungueltige Werte.
  const char* configured = AP_PASSWORD;
  _apPassword = (strlen(configured) >= 8) ? String(configured) : chipIdPassword();
  _apActive = WiFi.softAP(AP_SSID, _apPassword.c_str());
}

String NetManager::apSsid() const { return AP_SSID; }
