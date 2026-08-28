#pragma once
#include <ESP8266WebServer.h>
#include <functional>
#include "DisplayManager.h"
#include "NetManager.h"
#include "TimeManager.h"
#include "MqttBridge.h"
#include "Leds.h"
#include "Battery.h"

// Standalone-Steuerung: hostet die Bedienseite ("/"), eine HTTP-API (/api/...)
// sowie die Einrichtung von WiFi (/wifi) und MQTT (/mqtt). loop() muss
// regelmaessig aufgerufen werden.
class WebPortal {
public:
  WebPortal(DisplayManager& display, NetManager& net, MqttBridge& mqtt,
            LedController& leds, TimeManager& time, BatteryMonitor& battery,
            uint16_t port = 80);
  void begin();
  void loop();

  // Behandelt die transportunabhaengigen Konfig-/Status-Befehle (get*/cfg*),
  // damit sie sowohl ueber HTTP (/api/cmd) als auch USB (SerialBridge) laufen.
  // true = behandelt (reply gesetzt, restart ggf. true); false = kein Config-Befehl.
  bool handleConfigCommand(const String& action, const String& value,
                           String& reply, bool& restart);

private:
  // Parameter-Zugriff: liefert Wert / prueft Vorhandensein (HTTP: _server.arg,
  // USB: aus dem Query-String im Befehlswert geparst).
  using Arg = std::function<String(const String&)>;
  using Has = std::function<bool(const String&)>;

  void setupRoutes();
  // CSRF-Schutz (siehe .cpp): true, wenn der Browser den Fetch-Metadata-Header
  // Sec-Fetch-Site mit "cross-site" mitschickt -- also eine fremde Webseite die
  // Anfrage ausgeloest hat (z. B. per <img>/<form>), nicht die eigene Web-UI.
  bool isCrossSite() const;
  // Registriert eine zustandsaendernde Route mit vorgeschaltetem CSRF-Schutz
  // (isCrossSite()) statt direkt ueber _server.on().
  void onGuarded(const char* uri, std::function<void()> handler);
  void loadAppearance();                       // Web-Darstellung (Farben + Anzeigename) aus /ui.txt
  bool saveAppearance(const String& led1, const String& led2, const String& matrix);
  bool saveDisplayName(const String& name);    // Anzeigename (Startseiten-Titel) speichern

  // Gemeinsame Status-/Apply-Logik (von HTTP-Routen und den Companion-Befehlen genutzt).
  String buildLedStatus();
  String buildAppearanceJson();
  String buildSystemJson();
  String buildMqttStatus();
  String buildWifiStatus();
  String buildBatteryJson();
  void   applyLedConfig(const Arg& get);
  bool   applySystemConfig(const Arg& get, const Has& has);   // Rueckgabe: Neustart noetig?
  bool   applyWifiConfig(const Arg& get, const Has& has);     // Neustart
  bool   applyMqttConfig(const Arg& get, const Has& has);     // Neustart
  bool   applyBatteryCalibration(const Arg& get, const Has& has);   // Rueckgabe: kalibriert?

  ESP8266WebServer _server;
  DisplayManager&  _display;
  NetManager&      _net;
  MqttBridge&      _mqtt;
  LedController&   _leds;
  TimeManager&     _time;
  BatteryMonitor&  _battery;

  // Rein visuelle Farben (Web-UI), passend zur real verbauten Hardware.
  String _colLed1;                             // LED 1 (default rot)
  String _colLed2;                             // LED 2 (default gruen)
  String _colMatrix;                           // Matrix-Vorschau (default rot)
  String _name;                                // Anzeigename (Startseiten-Titel)
};
