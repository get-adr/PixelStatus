#pragma once
#include <Arduino.h>
#include "DisplayManager.h"
#include "Leds.h"
#include "WebPortal.h"

// Steuerung ueber das USB-Kabel (UART0 / Serial). Liest zeilenweise Befehle
// "<aktion> <wert>" und antwortet mit einer JSON-Statuszeile. Funktioniert ohne
// WiFi. loop() muss regelmaessig aufgerufen werden.
//
// Steuerbefehle (via runCommand) und Konfig-/Status-Befehle (via
// WebPortal::handleConfigCommand, gleiches Protokoll wie /api/cmd) werden hier
// gemeinsam behandelt -> die Companion-App kann alles auch ueber USB einstellen.
//
// Beispiele (115200 Baud, mit Newline):
//   preset onair            text Hallo Welt      timer 300      clock on
//   led 1 blink             led both off         settime 1719600000
//   getled                  getmqtt              getwifi        getsys   getappear
//   cfgled autoWarn=1&warnSecs=300&...           cfgsys name=Studio&host=studio
//   cfgwifi ssid=Netz&pass=geheim                cfgmqtt enabled=1&host=...
class SerialBridge {
public:
  SerialBridge(DisplayManager& display, LedController& leds, WebPortal& web);
  void begin();
  void loop();

private:
  void process(const String& line);
  DisplayManager& _display;
  LedController&  _leds;
  WebPortal&      _web;
  String          _line;
};
