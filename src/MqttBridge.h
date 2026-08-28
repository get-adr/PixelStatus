#pragma once
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DisplayManager.h"
#include "Leds.h"
#include "Battery.h"

// Verbindet das Display mit einem MQTT-Broker. Abonniert <base>/cmd/# und
// publiziert nach jeder Aenderung <base>/state (retained).
// Konfiguration zur Laufzeit ueber das Web-Portal (/mqtt), persistiert in
// LittleFS (/mqtt.txt); config.h liefert nur die Defaults. Ist MQTT deaktiviert
// oder kein Host gesetzt, tut loop() nichts. Reconnect nicht-blockierend.
//
// Home-Assistant-MQTT-Discovery: bei jedem (Re-)Connect werden retained
// Config-Nachrichten unter homeassistant/<komponente>/pixelstatus_<chipid>/
// <objekt>/config veroeffentlicht -- HA legt die Entitaeten daraus automatisch
// an, gruppiert unter einem gemeinsamen Geraet. <base>/state (bereits
// vorhanden) und ein neues <base>/health dienen als gemeinsame State-Topics;
// alle Entitaeten lesen daraus per value_template ein einzelnes Feld (auch die
// zwei LED-Lichter -- Standard-Lichtschema mit je einem eigenen Kommandotopic
// pro Faehigkeit, siehe publishDiscovery(); NICHT das JSON-Schema: das erwartet
// auf dem State-Topic direkt {"state":...} statt eines per value_template
// umgebauten Werts -- am echten Broker als KeyError:'state' im HA-Log
// aufgefallen, August 2026). Nur der Status-Select ist eine Ausnahme
// (optimistic, kein state_topic -- siehe publishDiscovery()).
class MqttBridge {
public:
  MqttBridge(DisplayManager& display, LedController& leds, BatteryMonitor& battery);
  void begin();
  void loop();

  // ---- Laufzeit-Konfiguration (vom Web-Portal genutzt) ----
  bool            enabled()   const { return _enabled; }
  const String&   host()      const { return _host; }
  uint16_t        port()      const { return _port; }
  const String&   user()      const { return _user; }
  const String&   base()      const { return _base; }
  bool            connected()       { return _client.connected(); }
  // Speichert die Konfiguration in Flash. changePass=false laesst das
  // bestehende Passwort unveraendert (Formular schickt es nicht erneut mit).
  bool saveConfig(bool enabled, const String& host, uint16_t port,
                  const String& user, const String& pass, bool changePass,
                  const String& base);

private:
  void loadConfig();
  void reconnect();
  void handle(const String& topic, const String& payload);
  static void onMessage(char* topic, uint8_t* payload, unsigned int len);

  // ---- Home-Assistant-Discovery/State (siehe Klassenkommentar) ----
  String deviceBlock() const;   // gemeinsames "device":{...}-Fragment, jede Config braucht ihr eigenes
  void   publishDiscovery();    // einmalig pro (Re-)Connect
  void   publishOne(const String& component, const String& objectId, const String& json);
  void   publishState(bool force);    // <base>/state, nur bei Aenderung (ausser force)
  void   publishHealth(bool force);   // <base>/health, gedrosselt + nur bei Aenderung

  WiFiClient      _net;
  PubSubClient    _client;
  DisplayManager& _display;
  LedController&  _leds;
  BatteryMonitor& _battery;
  unsigned long   _lastTry = 0;
  unsigned long   _lastHealthMs = 0;
  String          _lastStateJson;
  String          _lastHealthJson;

  bool     _enabled = false;
  String   _host;
  uint16_t _port = 1883;
  String   _user;
  String   _pass;
  String   _base;
};
