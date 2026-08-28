#include "TimeManager.h"
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <time.h>
#include <sys/time.h>
#include "config.h"

extern "C" {
  #include <sntp.h>   // nonos-sdk: sntp_stop() -- dieselbe Instanz, die configTime nutzt
}

static const char* NTP_FILE = "/ntp.txt";

void TimeManager::begin() {
  _server = NTP_SERVER;          // Default, ggf. von load() ueberschrieben
  load();
  setenv("TZ", NTP_TZ, 1);       // Zeitzone immer setzen (auch ohne NTP -> korrekte Lokalzeit)
  tzset();
  apply();
}

// /ntp.txt: Zeile 1 = "1"/"0" (enabled), Zeile 2 = Serveradresse.
void TimeManager::load() {
  File f = LittleFS.open(NTP_FILE, "r");
  if (!f) return;
  String en = f.readStringUntil('\n'); en.trim();
  String sv = f.readStringUntil('\n'); sv.trim();
  f.close();
  if (en.length()) _enabled = (en == "1");
  if (sv.length()) _server  = sv;
}

void TimeManager::apply() {
  if (_enabled) configTime(NTP_TZ, _server.c_str());  // startet/aktualisiert SNTP
  else          sntp_stop();                          // manuell gestellte Zeit bleibt stehen
}

void TimeManager::saveConfig(bool enabled, const String& server) {
  _enabled = enabled;
  if (server.length()) _server = server;              // leer -> bisherigen Server behalten
  File f = LittleFS.open(NTP_FILE, "w");
  if (f) { f.println(_enabled ? "1" : "0"); f.println(_server); f.close(); }
  apply();
}

void TimeManager::setManual(time_t epoch) {
  struct timeval tv = { epoch, 0 };
  settimeofday(&tv, nullptr);
}

bool TimeManager::synced() {
  return time(nullptr) > 1600000000;   // nach ~2020 -> plausible (synchronisierte) Zeit
}

void TimeManager::loop() {
  bool now = (WiFi.status() == WL_CONNECTED);
  // Flanke nicht-verbunden -> verbunden: NTP neu anstossen (deckt Reconnect und
  // "beim Boot kein WiFi, spaeter doch" ab). Nur wenn NTP aktiviert ist.
  if (now && !_wasConnected && _enabled) configTime(NTP_TZ, _server.c_str());
  _wasConnected = now;
}
