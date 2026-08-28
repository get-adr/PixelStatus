#pragma once
#include <time.h>
#include <sys/time.h>   // settimeofday() auf dem ESP8266
#include "DisplayManager.h"
#include "Leds.h"
#include "Presets.h"

// Zentraler Befehls-Dispatcher. Web (HTTP), MQTT und USB-Seriell rufen alle
// runCommand() auf -> ein einziger Ort definiert das Verhalten je Aktion.
//
//   action       value
//   ----------   ---------------------------------------------
//   text         <text>            statisch (scrollt nur wenn zu lang)
//   scroll       <text>            immer scrollend
//   preset       onair|call|...    siehe Presets.h
//   timer        <sek>|up|pause|resume|stop   Countdown in Sekunden / hochzaehlen /
//                                  pausieren / fortsetzen / stop
//   clock        on|off            Uhrzeit HH:MM (Systemzeit) / aus
//   settime      <epoch>           Systemzeit setzen (fuer USB ohne WiFi/NTP)
//   brightness   0..15             Matrix-Helligkeit
//   ledbrightness 0..15            Helligkeit der Zusatz-LEDs (Live-Uebernahme,
//                                  unabhaengig vom batched cfgled/saveConfig())
//   orient       0|180             Ausrichtung der Schrift (siehe DisplayManager.h)
//   scrolldir    left|right        Scrollrichtung (right = umgekehrt)
//   bootanim     0..5              Einschalt-Animation (siehe DisplayManager::BootAnim);
//                                  wird sofort einmal abgespielt (Vorschau)
//   led          <1|2|both> <off|on|blink>   Zusatz-LEDs schalten
//   led          alt <on|off>                Wechselblinken (beide gegenphasig)
//   game         start|up|down|fire|bomb     Space-Invaders-Gimmick (nur Web-UI,
//                                            "start" dient auch als Reset; zum
//                                            Beenden die generische Aktion "clear";
//                                            "bomb" sprengt alle sichtbaren Invader,
//                                            verbraucht eine der 2 an die Zusatz-LEDs
//                                            gekoppelten Bomben, siehe main.cpp)
//   clear        -
inline void runCommand(DisplayManager& d, LedController& leds,
                       const String& action, const String& value) {
  if      (action == "text")       d.showMessage(value);
  else if (action == "scroll")     d.scrollMessage(value);
  else if (action == "preset")     applyPreset(d, value);
  else if (action == "brightness") d.setBrightness(value.toInt());
  else if (action == "ledbrightness") leds.setBrightness((uint8_t)value.toInt());
  else if (action == "orient")     d.setOrientation((uint16_t)value.toInt());
  else if (action == "scrolldir")  d.setScrollReverse(value == "right");
  else if (action == "bootanim")   d.setBootAnimation((uint8_t)value.toInt());
  else if (action == "led") {
    // value = "<ziel> <zustand>", z. B. "1 on", "2 blink", "both off",
    // "alt on" (Wechselblinken ein -> beide blinken) / "alt off" (aus).
    int sp = value.indexOf(' ');
    String tgt = (sp < 0 ? value : value.substring(0, sp));
    String st  = (sp < 0 ? ""    : value.substring(sp + 1));
    tgt.trim(); st.trim(); st.toLowerCase();
    LedController::State s = (st == "on")    ? LedController::ON
                           : (st == "blink") ? LedController::BLINK
                                             : LedController::OFF;
    if (tgt == "alt") {                          // Wechselblinken als Ein-Klick-Aktion
      bool on = (st == "on" || st == "1" || st == "blink");
      leds.setAlternate(on);
      leds.setAll(on ? LedController::BLINK : LedController::OFF);
    }
    else if (tgt == "both" || tgt == "all") leds.setAll(s);
    else                                    leds.set((uint8_t)(tgt.toInt() - 1), s);  // "1"->0, "2"->1
  }
  else if (action == "game") {
    if      (value == "start") d.gameStart();
    else if (value == "up")    d.gameUp();
    else if (value == "down")  d.gameDown();
    else if (value == "fire")  d.gameFire();
    else if (value == "bomb")  d.gameBomb();
  }
  else if (action == "clear")      d.clear();
  else if (action == "clock")      { value == "off" ? d.clear() : d.startClock(); }
  else if (action == "settime") {
    time_t epoch = (time_t)value.toInt();
    struct timeval tv = { epoch, 0 };
    settimeofday(&tv, nullptr);
  }
  else if (action == "timer") {
    if      (value == "stop")   d.stopTimer();
    else if (value == "pause")  d.pauseTimer();
    else if (value == "resume") d.resumeTimer();
    else if (value == "up")     d.startTimer(0, false);
    else                        d.startTimer(value.toInt(), true);
  }
}

// Display-State + Zusatz-LED-Felder als ein JSON-Objekt. Genutzt von /api/state,
// /api/led und dem MQTT-state-Topic, damit alle denselben State liefern.
inline String combinedStateJson(DisplayManager& d, LedController& leds) {
  leds.updateCountdown(d.timerRemaining());   // warn-Feld aktuell halten (idempotent)
  String s = d.stateJson();
  if (s.endsWith("}")) s.remove(s.length() - 1);   // schliessende Klammer entfernen
  return s + "," + leds.stateFields() + "}";
}
