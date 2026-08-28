#include "SerialBridge.h"
#include "Commands.h"

SerialBridge::SerialBridge(DisplayManager& display, LedController& leds, WebPortal& web)
  : _display(display), _leds(leds), _web(web) {
  _line.reserve(256);
}

void SerialBridge::begin() {
  // Serial.begin() passiert bereits in main.cpp (gemeinsamer Port mit den Logs).
}

void SerialBridge::loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (_line.length() > 0) { process(_line); _line = ""; }
    } else if (_line.length() < 250) {
      _line += c;
    }
  }
}

void SerialBridge::process(const String& line) {
  // Erstes Wort = Aktion, Rest = Wert (darf Leerzeichen enthalten, z. B. Text).
  int sp = line.indexOf(' ');
  String action = sp < 0 ? line : line.substring(0, sp);
  String value  = sp < 0 ? ""   : line.substring(sp + 1);
  action.trim();

  // Konfig-/Status-Befehle (get*/cfg*) teilen sich die Logik mit /api/cmd.
  String reply; bool restart = false;
  if (_web.handleConfigCommand(action, value, reply, restart)) {
    Serial.println(reply);
    if (restart) { delay(400); ESP.restart(); }
    return;
  }
  runCommand(_display, _leds, action, value);
  Serial.println(combinedStateJson(_display, _leds));   // Antwort: JSON-Zeile, von der App auswertbar
}
