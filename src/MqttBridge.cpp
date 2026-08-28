#include "MqttBridge.h"
#include <LittleFS.h>
#include "config.h"
#include "Commands.h"

// PubSubClient erwartet einen freien C-Callback -> Zugriff auf die Instanz
// ueber diesen Datei-Zeiger (es gibt nur eine MqttBridge).
static MqttBridge* s_self = nullptr;

// Zeilenweise Ablage: enabled(0/1), host, port, user, pass, base.
static const char* CFG_PATH = "/mqtt.txt";

static void stripEol(String& s) {
  while (s.length() && (s[s.length() - 1] == '\r' || s[s.length() - 1] == '\n'))
    s.remove(s.length() - 1);
}

// Stabile, geraetespezifische ID fuer Discovery-unique_ids/Topics (Chip-ID
// aendert sich nie -> Entitaeten ueberleben Neustarts ohne Duplikate in HA).
static String nodeId() { return String(ESP.getChipId(), HEX); }

MqttBridge::MqttBridge(DisplayManager& display, LedController& leds, BatteryMonitor& battery)
  : _client(_net), _display(display), _leds(leds), _battery(battery) {}

void MqttBridge::begin() {
  s_self = this;
  LittleFS.begin();          // idempotent (NetManager mountet i.d.R. schon)
  loadConfig();
  _client.setCallback(onMessage);
  _client.setSocketTimeout(2); // unerreichbarer Broker blockiert loop() nur kurz
  // Default-Puffer von PubSubClient (256 Byte) reicht fuer die Discovery-Configs
  // nicht (Device-Block + Lichter mit brightness_scale/effect_list ueberschreiten
  // das leicht) -- zu grosse Nachrichten wuerden sonst STILL verworfen, ohne dass
  // publish() das sichtbar meldet.
  _client.setBufferSize(1024);
  if (_enabled && _host.length()) _client.setServer(_host.c_str(), _port);
}

void MqttBridge::loadConfig() {
  // Defaults aus config.h ...
  _enabled = MQTT_ENABLED;
  _host    = MQTT_HOST;
  _port    = MQTT_PORT;
  _user    = MQTT_USER;
  _pass    = MQTT_PASSWORD;
  _base    = MQTT_BASE_TOPIC;

  // ... von gespeicherter Konfiguration ueberschrieben.
  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) return;
  auto line = [&]() { String s = f.readStringUntil('\n'); stripEol(s); return s; };
  _enabled = (line() == "1");
  _host    = line();
  _port    = (uint16_t)line().toInt();
  _user    = line();
  _pass    = line();
  String b = line();
  if (b.length()) _base = b;
  f.close();
}

bool MqttBridge::saveConfig(bool enabled, const String& host, uint16_t port,
                            const String& user, const String& pass,
                            bool changePass, const String& base) {
  if (changePass) _pass = pass;
  _enabled = enabled;
  _host    = host;
  _port    = port ? port : 1883;
  _user    = user;
  _base    = base.length() ? base : String(MQTT_BASE_TOPIC);

  File f = LittleFS.open(CFG_PATH, "w");
  if (!f) return false;
  f.print(_enabled ? "1" : "0"); f.print('\n');
  f.print(_host); f.print('\n');
  f.print(_port); f.print('\n');
  f.print(_user); f.print('\n');
  f.print(_pass); f.print('\n');
  f.print(_base); f.print('\n');
  f.close();
  return true;
}

void MqttBridge::loop() {
  if (!_enabled || _host.length() == 0 || WiFi.status() != WL_CONNECTED) return;

  if (!_client.connected()) {
    if (millis() - _lastTry > 5000) { _lastTry = millis(); reconnect(); }
    return;
  }
  _client.loop();

  // <base>/state haelt Aenderungen aus JEDER Quelle nach (Web-UI/USB/Countdown-
  // Warnung), nicht nur aus MQTT selbst -- ohne diesen periodischen Abgleich
  // wuerden HA-Entitaeten nach z.B. einer Web-UI-Aenderung veraltet bleiben.
  // Gedrosselt + inhaltlich diff-geprueft (siehe publishState()), kein
  // staendiges Neuschreiben wie beim fruehen PWM-Bug bei den Zusatz-LEDs.
  static unsigned long lastStateCheck = 0;
  if (millis() - lastStateCheck > 1000) { lastStateCheck = millis(); publishState(false); }
  publishHealth(false);   // hat eigene, groessere Drosselung (15 s)
}

void MqttBridge::reconnect() {
  String id = String(HOSTNAME) + "-" + String(ESP.getChipId(), HEX);
  String availTopic = _base + "/availability";
  const char* u = _user.length() ? _user.c_str() : nullptr;
  const char* p = _user.length() ? _pass.c_str() : nullptr;
  // Last Will: Broker markiert das Geraet automatisch als "offline", wenn die
  // Verbindung ungeplant abreisst (Stromausfall, WiFi weg) -- ohne das wuerden
  // HA-Entitaeten nach einem Absturz faelschlich "verfuegbar" mit veraltetem
  // Stand bleiben.
  bool ok = _client.connect(id.c_str(), u, p, availTopic.c_str(), 0, true, "offline");
  if (!ok) return;
  _client.subscribe((_base + "/cmd/#").c_str());
  _client.publish(availTopic.c_str(), "online", true);
  publishDiscovery();
  // Caches leeren -> der naechste loop()-Tick publiziert garantiert einmal
  // frisch, auch wenn sich der Inhalt seit der letzten (nun verworfenen)
  // Verbindung nicht geaendert hat.
  _lastStateJson = "";
  _lastHealthJson = "";
  publishState(true);
  publishHealth(true);
}

void MqttBridge::onMessage(char* topic, uint8_t* payload, unsigned int len) {
  if (!s_self) return;
  String p;
  p.reserve(len);
  for (unsigned int i = 0; i < len; i++) p += (char)payload[i];
  s_self->handle(String(topic), p);
}

void MqttBridge::handle(const String& topic, const String& payload) {
  // Topic: <base>/cmd/<aktion>  -> Aktion ist das letzte Segment.
  // led1/led2/led1brightness/.../led1effect/... sind reine HA-Discovery-
  // Kommandotopics (Standard-Lichtschema: ein eigenes Topic je Faehigkeit,
  // schlichte Textnutzlast -- kein JSON, siehe publishDiscovery()) und
  // erreichen runCommand() bewusst nicht -- das allgemeine "led"-Flachtext-
  // Protokoll (Web/USB/manuelles MQTT, "<ziel> <zustand>" in EINEM Wert)
  // bleibt davon unberuehrt.
  String action = topic.substring(topic.lastIndexOf('/') + 1);
  if      (action == "led1" || action == "led2") {
    uint8_t idx = (action == "led1") ? 0 : 1;
    if (payload == "OFF") _leds.set(idx, LedController::OFF);
    // "ON" nur anwenden, wenn die LED bisher wirklich aus war -- HA schickt
    // state="ON" und effect="blink" bei einem kombinierten turn_on-Aufruf
    // (Helligkeit+Effekt in einem Schritt) als ZWEI getrennte Nachrichten auf
    // getrennten Topics (Standard-Lichtschema, kein JSON-Kommando), die
    // Reihenfolge ist nicht garantiert. Ein bedingungsloses "ON" wuerde ein
    // bereits/gleichzeitig gesetztes BLINK je nach Ankunftsreihenfolge wieder
    // zuruecksetzen; so gewinnt effect="blink" unabhaengig davon, welche der
    // beiden Nachrichten zuerst ankommt (am echten Broker beobachtet, ein
    // kombinierter turn_on mit brightness+effect blieb dadurch auf "on" statt
    // "blink" haengen, August 2026).
    else if (_leds.manualState(idx) == LedController::OFF) _leds.set(idx, LedController::ON);
  }
  else if (action == "led1effect")      _leds.set(0, LedController::BLINK);
  else if (action == "led2effect")      _leds.set(1, LedController::BLINK);
  else if (action == "led1brightness" || action == "led2brightness") {
    // Helligkeit ist geraeteweit gemeinsam (LedController::_brightness gilt
    // fuer BEIDE LEDs, siehe Leds.h) -- ein Helligkeitsbefehl an led1 wirkt
    // also automatisch auch auf led2 mit, genau wie der gemeinsame Regler im
    // Web-UI/der Companion-App.
    long lvl = payload.toInt();
    if (lvl < 0) lvl = 0; else if (lvl > 15) lvl = 15;
    _leds.setBrightness((uint8_t)lvl);
  }
  else                                   runCommand(_display, _leds, action, payload);
  publishState(true);   // sofort nach einem selbst empfangenen Kommando, wie bisher
}

void MqttBridge::publishState(bool force) {
  String j = combinedStateJson(_display, _leds);
  if (!force && j == _lastStateJson) return;
  _lastStateJson = j;
  _client.publish((_base + "/state").c_str(), j.c_str(), true);
}

void MqttBridge::publishHealth(bool force) {
  if (!force && millis() - _lastHealthMs < 15000) return;
  _lastHealthMs = millis();
  String j = "{\"heap\":" + String(ESP.getFreeHeap()) +
             ",\"rssi\":" + String(WiFi.RSSI()) +
             ",\"uptime\":" + String(millis() / 1000);
  if (_battery.enabled()) {
    j += ",\"battVoltage\":" + String(_battery.voltage(), 2) +
         ",\"battPct\":" + String(_battery.percent());
  }
  j += "}";
  if (!force && j == _lastHealthJson) return;
  _lastHealthJson = j;
  _client.publish((_base + "/health").c_str(), j.c_str(), true);
}

// ---- Home-Assistant-MQTT-Discovery ----
// https://www.home-assistant.io/integrations/mqtt/#discovery-messages
String MqttBridge::deviceBlock() const {
  return "\"device\":{\"identifiers\":[\"pixelstatus_" + nodeId() + "\"],"
         "\"name\":\"" + WiFi.hostname() + "\","
         "\"manufacturer\":\"DIY\","
         "\"model\":\"Pixel Status (ESP8266 + MAX7219)\"}";
}

void MqttBridge::publishOne(const String& component, const String& objectId, const String& json) {
  String topic = "homeassistant/" + component + "/pixelstatus_" + nodeId() + "/" + objectId + "/config";
  _client.publish(topic.c_str(), json.c_str(), true);
}

// Einmal pro (Re-)Connect, retained -> HA behaelt die Entitaeten auch ueber
// einen HA-Neustart hinweg, ohne dass das Geraet sie staendig neu senden muss.
void MqttBridge::publishDiscovery() {
  const String avail = "\"availability_topic\":\"" + _base + "/availability\",";
  const String dev = deviceBlock();
  const String base = _base;

  // Status-Presets als Select. Bewusst OHNE state_topic (optimistic): der
  // Anzeigeinhalt (mode/text in <base>/state) laesst sich nicht verlustfrei
  // auf einen der Preset-Namen zurueckrechnen (z.B. bei eigenem Text oder der
  // Uhr) -- HA merkt sich stattdessen einfach die zuletzt gesendete Auswahl.
  publishOne("select", "status",
    "{\"name\":\"Status\",\"unique_id\":\"pixelstatus_" + nodeId() + "_status\"," + avail +
    "\"command_topic\":\"" + base + "/cmd/preset\","
    "\"options\":[\"onair\",\"call\",\"busy\",\"brb\",\"free\",\"off\"]," + dev + "}");

  // Eigener Text -- state_topic zeigt, was gerade tatsaechlich angezeigt wird
  // (auch wenn per Web-UI/USB gesetzt), nicht nur den zuletzt per MQTT gesendeten.
  publishOne("text", "customtext",
    "{\"name\":\"Text\",\"unique_id\":\"pixelstatus_" + nodeId() + "_text\"," + avail +
    "\"command_topic\":\"" + base + "/cmd/text\","
    "\"state_topic\":\"" + base + "/state\",\"value_template\":\"{{ value_json.text }}\","
    "\"max\":128," + dev + "}");

  // Matrix-Helligkeit (0..15, wie im Web-UI/der Companion-App).
  publishOne("number", "matrixbright",
    "{\"name\":\"Matrix-Helligkeit\",\"unique_id\":\"pixelstatus_" + nodeId() + "_matrixbright\"," + avail +
    "\"command_topic\":\"" + base + "/cmd/brightness\","
    "\"state_topic\":\"" + base + "/state\",\"value_template\":\"{{ value_json.brightness }}\","
    "\"min\":0,\"max\":15,\"step\":1,\"icon\":\"mdi:brightness-6\"," + dev + "}");

  // Zusatz-LEDs als Licht -- Standard-Lichtschema (NICHT "json"): ein eigenes
  // Kommandotopic je Faehigkeit (on/off, Helligkeit, Effekt) mit schlichter
  // Textnutzlast, alle drei lesen ihren State-Wert per value_template aus
  // demselben <base>/state (kein eigenes State-Topic pro Licht noetig, exakt
  // dasselbe Prinzip wie bei den Sensoren unten). Das JSON-Schema (state+
  // brightness+effect atomar in einer Nachricht) wurde bewusst NICHT verwendet
  // -- HA erwartet dafuer direkt {"state":...} auf dem State-Topic, ein per
  // value_template umgebauter Wert wird nicht unterstuetzt (am echten Broker
  // als KeyError:'state' im HA-Log aufgefallen, August 2026).
  // brightness_scale=15 -> HA sendet/zeigt direkt unsere native Stufenskala,
  // keine 0..255-Umrechnung noetig. Helligkeit ist geraeteweit gemeinsam
  // (siehe handle()), beide Lichter zeigen deshalb denselben Wert.
  for (uint8_t i = 0; i < 2; i++) {
    String obj = "led" + String(i + 1);
    publishOne("light", obj,
      "{\"name\":\"LED " + String(i + 1) + "\",\"unique_id\":\"pixelstatus_" + nodeId() + "_" + obj + "\"," + avail +
      "\"command_topic\":\"" + base + "/cmd/" + obj + "\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\","
      "\"supported_color_modes\":[\"brightness\"],"
      "\"state_topic\":\"" + base + "/state\","
      "\"state_value_template\":\"{{ 'ON' if value_json." + obj + " != 'off' else 'OFF' }}\","
      "\"brightness_command_topic\":\"" + base + "/cmd/" + obj + "brightness\","
      "\"brightness_state_topic\":\"" + base + "/state\","
      "\"brightness_value_template\":\"{{ value_json.ledBrightness }}\",\"brightness_scale\":15,"
      "\"effect_command_topic\":\"" + base + "/cmd/" + obj + "effect\","
      "\"effect_state_topic\":\"" + base + "/state\","
      "\"effect_value_template\":\"{{ 'blink' if value_json." + obj + " == 'blink' else 'none' }}\","
      "\"effect_list\":[\"blink\"]," + dev + "}");
  }

  // Diagnose-Sensoren aus <base>/health (siehe publishHealth()).
  publishOne("sensor", "heap",
    "{\"name\":\"Freier Speicher\",\"unique_id\":\"pixelstatus_" + nodeId() + "_heap\"," + avail +
    "\"state_topic\":\"" + base + "/health\",\"value_template\":\"{{ value_json.heap }}\","
    "\"unit_of_measurement\":\"B\",\"entity_category\":\"diagnostic\",\"icon\":\"mdi:memory\"," + dev + "}");

  publishOne("sensor", "rssi",
    "{\"name\":\"WLAN-Signal\",\"unique_id\":\"pixelstatus_" + nodeId() + "_rssi\"," + avail +
    "\"state_topic\":\"" + base + "/health\",\"value_template\":\"{{ value_json.rssi }}\","
    "\"unit_of_measurement\":\"dBm\",\"device_class\":\"signal_strength\","
    "\"entity_category\":\"diagnostic\"," + dev + "}");

  publishOne("sensor", "uptime",
    "{\"name\":\"Laufzeit\",\"unique_id\":\"pixelstatus_" + nodeId() + "_uptime\"," + avail +
    "\"state_topic\":\"" + base + "/health\",\"value_template\":\"{{ value_json.uptime }}\","
    "\"unit_of_measurement\":\"s\",\"device_class\":\"duration\","
    "\"entity_category\":\"diagnostic\"," + dev + "}");

  // Nur ankuendigen, wenn die Akku-Hardware tatsaechlich verbaut/aktiviert ist
  // (BATTERY_MONITOR_ENABLED) -- sonst gaebe es in HA zwei Sensoren, die nie
  // einen Wert liefern (gleiches Prinzip wie die Health-Seite im Web-UI).
  if (_battery.enabled()) {
    publishOne("sensor", "battvolt",
      "{\"name\":\"Akkuspannung\",\"unique_id\":\"pixelstatus_" + nodeId() + "_battvolt\"," + avail +
      "\"state_topic\":\"" + base + "/health\",\"value_template\":\"{{ value_json.battVoltage }}\","
      "\"unit_of_measurement\":\"V\",\"entity_category\":\"diagnostic\"," + dev + "}");

    publishOne("sensor", "battpct",
      "{\"name\":\"Akkustand\",\"unique_id\":\"pixelstatus_" + nodeId() + "_battpct\"," + avail +
      "\"state_topic\":\"" + base + "/health\",\"value_template\":\"{{ value_json.battPct }}\","
      "\"unit_of_measurement\":\"%\",\"device_class\":\"battery\","
      "\"entity_category\":\"diagnostic\"," + dev + "}");
  }

  // Countdown-Warnung (letzte Minuten eines Timers, siehe Leds.h).
  publishOne("binary_sensor", "warn",
    "{\"name\":\"Countdown-Warnung\",\"unique_id\":\"pixelstatus_" + nodeId() + "_warn\"," + avail +
    "\"state_topic\":\"" + base + "/state\",\"value_template\":\"{{ 'ON' if value_json.warn else 'OFF' }}\","
    "\"entity_category\":\"diagnostic\"," + dev + "}");
}
