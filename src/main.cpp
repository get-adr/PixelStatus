#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#include "config.h"
#include "DisplayManager.h"
#include "Leds.h"
#include "Battery.h"
#include "NetManager.h"
#include "TimeManager.h"
#include "WebPortal.h"
#include "MqttBridge.h"
#include "SerialBridge.h"
#include "LoopStats.h"

DisplayManager display(CS_PIN, MAX_DEVICES);
LedController  leds(LED_PIN_1, LED_PIN_2, LED_ACTIVE_LOW, LED_WARN_SECS, LED_WARN_FAST_SECS);
BatteryMonitor battery(A0, BATTERY_MONITOR_ENABLED, BATTERY_EMPTY_V, BATTERY_FULL_V,
                       BATTERY_LOW_PERCENT, BATTERY_LOW_BRIGHTNESS);
NetManager     net;
TimeManager    timeMgr;
MqttBridge     mqtt(display, leds, battery);
WebPortal      web(display, net, mqtt, leds, timeMgr, battery);
SerialBridge   serialCtl(display, leds, web);

// Haelt die Scroll-Animation waehrend der (blockierenden) WiFi-Wartezeit am Laufen
// und treibt bei BOOT_PROGRESS zusaetzlich den an den echten Connect gekoppelten
// Fortschrittsbalken an (siehe DisplayManager::bootProgressUpdate()).
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
static unsigned long  s_connectStart = 0;
static void pumpDisplay() {
  display.loop();
  if (display.bootAnimation() == DisplayManager::BOOT_PROGRESS) {
    float frac = (float)(millis() - s_connectStart) / (float)WIFI_CONNECT_TIMEOUT_MS;
    display.bootProgressUpdate(frac > 1.0f ? 1.0f : frac);
  }
}

// Waehrend der Setup-Hotspot offen ist (kein WLAN konfiguriert/erreichbar,
// siehe NetManager::startAP()), wechselt die Matrix im Sekundentakt zwischen
// AP-Name und -Passwort, damit man das (geraetespezifisch generierte, siehe
// SEC-05) Passwort ablesen kann, ohne die Weboberflaeche zu brauchen. Endet,
// sobald sich ein Geraet mit dem Hotspot verbindet -- ab dann ist vermutlich
// schon jemand am Smartphone/Laptop im Einrichtungsdialog.
static bool          s_apSetupDone    = false;
static bool          s_apTimerStarted = false;
static bool          s_apShowingPass  = false;
static unsigned long s_apLastSwitch   = 0;
static void handleApSetupDisplay() {
  if (!net.apActive() || s_apSetupDone) return;
  if (WiFi.softAPgetStationNum() > 0) {
    // Jemand hat sich verbunden -> Anzeige aktiv beenden statt SSID/Passwort
    // einfach stehen zu lassen. clearBrightnessOverride() nur zur Sicherheit,
    // falls die Akku-Warnung (siehe handleBatteryWarning()) waehrend der
    // Wartezeit doch schon einmal gedimmt hatte.
    s_apSetupDone = true;
    display.clear();
    display.clearBrightnessOverride();
    return;
  }
  if (!s_apTimerStarted) {
    s_apTimerStarted = true;
    s_apLastSwitch = millis();
    // Passwort muss lesbar sein, auch wenn schon vor dem Setup-Hotspot eine
    // Akku-Dimmung aktiv war -- handleBatteryWarning() haelt sich waehrenddessen
    // ohnehin zurueck (siehe dort), das hier faengt nur den Fall ab, dass die
    // Dimmung schon VOR startAP() gesetzt wurde.
    display.clearBrightnessOverride();
    return;
  }
  if (millis() - s_apLastSwitch < 6000) return;   // ca. eine volle Scroll-Runde je Text
  s_apLastSwitch = millis();
  s_apShowingPass = !s_apShowingPass;
  display.scrollMessage(s_apShowingPass ? net.apPassword() : net.apSsid());
}

// Einmaliger Uebergang bei Unter-/Ueberschreiten der Low-Battery-Schwelle:
// Anzeige "LOW BATT" nur beim Eintritt (kein Neustart der Scroll-Animation in
// jedem loop()), Helligkeit bleibt gedrosselt, bis der Zustand wieder verlaesst.
static bool s_batteryWasLow = false;
static void handleBatteryWarning() {
  if (!battery.enabled()) return;
  // Solange der Setup-Hotspot noch SSID/Passwort anzeigt, hat das Vorrang:
  // "LOW BATT" wuerde die Anzeige ueberschreiben, die Dimmung sie unlesbar
  // machen (am Geraet beobachtet -- Passwort nach ein paar Durchlaeufen
  // nicht mehr lesbar). low bleibt dabei unausgewertet -> keine verlorene
  // Zustandsaenderung, s_batteryWasLow wird beim naechsten regulaeren Aufruf
  // (AP-Setup beendet) ganz normal gegen den dann aktuellen Wert geprueft.
  if (net.apActive() && !s_apSetupDone) return;
  bool low = battery.low();
  if (low && !s_batteryWasLow) {
    display.scrollMessage("LOW BATT");
    display.setBrightnessOverride(battery.lowBrightness());
  } else if (!low && s_batteryWasLow) {
    display.clearBrightnessOverride();
  }
  s_batteryWasLow = low;
}

// Koppelt das Bomben-Gimmick des Spiels an die beiden Zusatz-LEDs, ohne dass
// DisplayManager LedController kennen muesste (dieselbe Entkopplung wie bei
// der Countdown-Warnung oben). Waehrend eines Spiels zeigen beide LEDs per
// Ein/Aus, ob ihre Bombe noch verfuegbar ist (An = verfuegbar, startet mit
// Spielbeginn; Blinken = wird gerade gezuendet; Aus = verbraucht). Verlaesst
// das Spiel GAME (Game Over ODER "Aus"), schaltet gameLedState() beide LEDs
// explizit aus, statt sie im letzten Bomben-Zustand haengen zu lassen. Nur
// bei tatsaechlichem Wechsel anwenden (Flankenerkennung je LED) --
// LedController::set() setzt bei jedem Aufruf mit s != BLINK auch das
// globale Wechselblinken zurueck, staendiges Aufrufen wuerde das kaputt machen.
static DisplayManager::GameLedState s_gameLed[2] = {
  DisplayManager::GLED_NONE, DisplayManager::GLED_NONE
};
static void handleGameBombLed() {
  for (uint8_t i = 0; i < 2; i++) {
    DisplayManager::GameLedState st = display.gameLedState(i);
    if (st == s_gameLed[i]) continue;
    s_gameLed[i] = st;
    switch (st) {
      case DisplayManager::GLED_AVAILABLE: leds.set(i, LedController::ON);    break;
      case DisplayManager::GLED_FLASH:     leds.set(i, LedController::BLINK); break;
      case DisplayManager::GLED_USED:      leds.set(i, LedController::OFF);  break;
      case DisplayManager::GLED_NONE:      break;   // noch nie gespielt -> LED unangetastet lassen
    }
  }
}

void setup() {
  Serial.begin(115200);
  display.begin(DEFAULT_BRIGHTNESS);
  leds.begin();
  battery.begin();
  net.begin();
  timeMgr.begin();   // Zeitzone setzen + NTP gemaess gespeicherter Konfig starten/stoppen

  // Einschalt-Animation fuellt die dunkle Luecke bis hierhin. BOOT_PROGRESS
  // ist ein Sonderfall: er laeuft nicht vorab fix-dauernd, sondern waehrend
  // der eigentlichen WiFi-Wartezeit unten (angetrieben von pumpDisplay()) --
  // und ersetzt dafuer den "WiFi..."-Text, statt gleichzeitig mit ihm um
  // denselben Pixelpuffer zu konkurrieren. Alle anderen Typen laufen VOR dem
  // WiFi-Versuch: startBootAnimation() ist nicht-blockierend (loop() treibt
  // sie ueber stepBootAnimation() an), das kleine Wartschleifchen hier ist an
  // dieser Stelle unproblematisch, da web/mqtt/seriell noch gar nicht laufen.
  uint8_t bootAnim = display.bootAnimation();
  bool showConnectingText = true;
  if (bootAnim == DisplayManager::BOOT_PROGRESS) {
    display.bootProgressBegin();
    showConnectingText = false;
  } else if (bootAnim != DisplayManager::BOOT_OFF) {
    display.startBootAnimation(bootAnim);
    while (display.animationActive()) { display.loop(); delay(5); }
  }

  if (showConnectingText) display.scrollMessage("WiFi...");
  s_connectStart = millis();
  if (net.connectSTA(WIFI_CONNECT_TIMEOUT_MS, pumpDisplay)) {
    MDNS.begin(net.hostname().c_str());
    MDNS.addService("http", "tcp", 80);
    Serial.printf("\nBereit: http://%s.local  (%s)\n",
                  net.hostname().c_str(), WiFi.localIP().toString().c_str());
    display.scrollMessage(WiFi.localIP().toString());
  } else {
    // Kein WiFi -> Setup-Hotspot oeffnen (Steuerung + WiFi-Einrichtung unter 192.168.4.1).
    net.startAP();
    Serial.printf("\nKein WiFi - Setup-Hotspot '%s' offen (http://192.168.4.1/wifi)\n",
                  net.apSsid().c_str());
    display.scrollMessage(net.apSsid());
  }

  web.begin();
  serialCtl.begin();
  mqtt.begin();   // verbindet nur, wenn zur Laufzeit aktiviert (siehe /mqtt)
}

void loop() {
  LoopStats::tick();
  display.loop();
  leds.updateCountdown(display.timerRemaining());   // Countdown-Warnung der Zusatz-LEDs
  handleGameBombLed();
  leds.loop();
  battery.loop();
  handleBatteryWarning();
  handleApSetupDisplay();
  web.loop();
  serialCtl.loop();
  MDNS.update();
  mqtt.loop();
  timeMgr.loop();   // NTP nach WiFi-(Re-)Connect neu synchronisieren
}
