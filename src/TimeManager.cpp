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
  _server = NTP_SERVER;          // Defaults, ggf. von load() ueberschrieben
  _tz     = NTP_TZ;
  load();
  applyTz();                     // Zeitzone immer setzen (auch ohne NTP -> korrekte Lokalzeit)
  apply();
}

// /ntp.txt: Zeile 1 = "1"/"0" (enabled), Zeile 2 = Serveradresse, Zeile 3 =
// POSIX-TZ-String. Aeltere Dateien haben nur zwei Zeilen -- dann bleibt es
// beim Default aus config.h, statt die Zone auf einen leeren String zu setzen.
void TimeManager::load() {
  File f = LittleFS.open(NTP_FILE, "r");
  if (!f) return;
  String en = f.readStringUntil('\n'); en.trim();
  String sv = f.readStringUntil('\n'); sv.trim();
  String tz = f.readStringUntil('\n'); tz.trim();
  f.close();
  if (en.length()) _enabled = (en == "1");
  if (sv.length()) _server  = sv;
  if (tz.length()) _tz      = tz;
}

void TimeManager::applyTz() {
  setenv("TZ", _tz.c_str(), 1);
  tzset();
}

void TimeManager::apply() {
  if (_enabled) configTime(_tz.c_str(), _server.c_str());  // startet/aktualisiert SNTP
  else          sntp_stop();                               // manuell gestellte Zeit bleibt stehen
}

void TimeManager::saveConfig(bool enabled, const String& server, const String& tz) {
  _enabled = enabled;
  if (server.length()) _server = server;              // leer -> bisherigen Server behalten
  if (tz.length())     _tz     = tz;                  // leer -> bisherige Zone behalten
  File f = LittleFS.open(NTP_FILE, "w");
  if (f) { f.println(_enabled ? "1" : "0"); f.println(_server); f.println(_tz); f.close(); }
  applyTz();   // auch ohne NTP wirksam: eine manuell gestellte Zeit wird sofort neu umgerechnet
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
  if (now && !_wasConnected && _enabled) configTime(_tz.c_str(), _server.c_str());
  _wasConnected = now;
}
